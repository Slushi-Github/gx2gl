#include "gl_buffer.h"
#include "gl_vao.h"
#include "endian/endian.h"
#include "mem/gl_mem.h"
#include <coreinit/cache.h>
#include <gx2/draw.h>
#include <gx2r/buffer.h>
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_BUFFERS 1024

typedef struct {
  bool in_use;
  bool gpu_owned;
  GLenum target;
  GLsizeiptr size;
  GLenum usage;
  GX2RBuffer gx2_buffer;
  uint8_t *uniform_shadow;
  void *mapped_ptr;
  GLenum mapped_access;
  GLboolean mapped_persistent;
  GLboolean mapped_uniform_shadow;
  GLintptr mapped_offset;
  GLsizeiptr mapped_length;
} GLBuffer;

static GLBuffer g_buffers[MAX_BUFFERS];

void gl_buffer_init(void) { memset(g_buffers, 0, sizeof(g_buffers)); }

static bool is_buffer_target(GLenum target) {
  return target == GL_ARRAY_BUFFER || target == GL_ELEMENT_ARRAY_BUFFER || target == GL_UNIFORM_BUFFER;
}

static void free_uniform_shadow(GLBuffer *buf) {
  if (!buf || !buf->uniform_shadow) return;
  free(buf->uniform_shadow);
  buf->uniform_shadow = NULL;
}

static bool sync_uniform_shadow_range(GLBuffer *buf, GLintptr offset, GLsizeiptr size) {
  GLintptr start;
  GLintptr end;
  GLintptr full_word_bytes;
  uint8_t *gpu_data;
  uint8_t *shadow_data;

  if (!buf || !buf->gpu_owned || !buf->uniform_shadow) return false;
  if (offset < 0 || size < 0 || offset + size > buf->size) return false;
  if (size == 0) return true;

  start = offset & ~((GLintptr)3);
  end = offset + size;
  if (end < buf->size) {
    end = (end + 3) & ~((GLintptr)3);
  } else {
    end = buf->size;
  }

  gpu_data = (uint8_t *)GX2RLockBufferEx(&buf->gx2_buffer, (GX2RResourceFlags)0);
  if (!gpu_data) return false;

  shadow_data = buf->uniform_shadow;
  full_word_bytes = (end - start) & ~((GLintptr)3);
  for (GLintptr i = 0; i < full_word_bytes; i += 4) {
    uint32_t value;
    memcpy(&value, shadow_data + start + i, sizeof(value));
    value = CPU_TO_GPU_32(value);
    memcpy(gpu_data + start + i, &value, sizeof(value));
  }
  if (full_word_bytes < (end - start)) {
    memcpy(gpu_data + start + full_word_bytes,
           shadow_data + start + full_word_bytes,
           (size_t)((end - start) - full_word_bytes));
  }

  GX2RUnlockBufferEx(&buf->gx2_buffer, (GX2RResourceFlags)0);
  return true;
}

static bool initialize_uniform_shadow(GLBuffer *buf, const GLvoid *data) {
  if (!buf || buf->size < 0) return false;

  free_uniform_shadow(buf);
  if (buf->size == 0) return true;

  buf->uniform_shadow = (uint8_t *)malloc((size_t)buf->size);
  if (!buf->uniform_shadow) return false;

  if (data) {
    memcpy(buf->uniform_shadow, data, (size_t)buf->size);
  } else {
    memset(buf->uniform_shadow, 0, (size_t)buf->size);
  }

  return sync_uniform_shadow_range(buf, 0, buf->size);
}

static GLuint get_bound_buffer(GLenum target) {
  if (!g_gl_context) return 0;
  switch (target) {
  case GL_ARRAY_BUFFER: return g_gl_context->bound_array_buffer;
  case GL_ELEMENT_ARRAY_BUFFER: return gl_vao_get_element_array_buffer();
  case GL_UNIFORM_BUFFER: return g_gl_context->bound_uniform_buffer;
  default: return 0;
  }
}

void _gl_GenBuffers(GLsizei n, GLuint *buffers) {
  if (n < 0) { _gl_set_error(GL_INVALID_VALUE); return; }
  if (n > 0 && !buffers) { _gl_set_error(GL_INVALID_VALUE); return; }
  int count = 0;
  for (int i = 1; i < MAX_BUFFERS && count < n; i++) {
    if (!g_buffers[i].in_use) {
      memset(&g_buffers[i], 0, sizeof(GLBuffer));
      g_buffers[i].in_use = true;
      buffers[count++] = i;
    }
  }
}

void _gl_DeleteBuffers(GLsizei n, const GLuint *buffers) {
  if (n < 0) { _gl_set_error(GL_INVALID_VALUE); return; }
  if (n > 0 && !buffers) { _gl_set_error(GL_INVALID_VALUE); return; }
  for (int i = 0; i < n; i++) {
    GLuint id = buffers[i];
    if (id > 0 && id < MAX_BUFFERS && g_buffers[id].in_use) {
      if (g_gl_context) {
        if (g_gl_context->bound_array_buffer == id) g_gl_context->bound_array_buffer = 0;
        if (g_gl_context->bound_uniform_buffer == id) g_gl_context->bound_uniform_buffer = 0;
        for (uint32_t binding = 0; binding < GL33_MAX_UNIFORM_BUFFER_BINDINGS; ++binding) {
          if (g_gl_context->uniform_buffer_bindings[binding].buffer == id) {
            g_gl_context->uniform_buffer_bindings[binding].buffer = 0;
            g_gl_context->uniform_buffer_bindings[binding].offset = 0;
            g_gl_context->uniform_buffer_bindings[binding].size = 0;
            g_gl_context->uniform_buffer_bindings[binding].whole_buffer = GL_FALSE;
          }
        }
      }
      gl_vao_unbind_buffer(id);
      if (g_buffers[id].gpu_owned) GX2RDestroyBufferEx(&g_buffers[id].gx2_buffer, (GX2RResourceFlags)0);
      free_uniform_shadow(&g_buffers[id]);
      memset(&g_buffers[id].gx2_buffer, 0, sizeof(g_buffers[id].gx2_buffer));
      g_buffers[id].gpu_owned = false;
      g_buffers[id].mapped_ptr = NULL;
      g_buffers[id].mapped_uniform_shadow = GL_FALSE;
      g_buffers[id].in_use = false;
    }
  }
}

GLboolean _gl_IsBuffer(GLuint buffer) { return (buffer < MAX_BUFFERS && g_buffers[buffer].in_use) ? GL_TRUE : GL_FALSE; }

void _gl_BindBuffer(GLenum target, GLuint buffer) {
  if (!g_gl_context) return;
  if (buffer >= MAX_BUFFERS || (buffer > 0 && !g_buffers[buffer].in_use)) { _gl_set_error(GL_INVALID_OPERATION); return; }
  switch (target) {
  case GL_ARRAY_BUFFER: g_gl_context->bound_array_buffer = buffer; break;
  case GL_ELEMENT_ARRAY_BUFFER: gl_vao_set_element_array_buffer(buffer); break;
  case GL_UNIFORM_BUFFER: g_gl_context->bound_uniform_buffer = buffer; break;
  default: _gl_set_error(GL_INVALID_ENUM); return;
  }
}

void _gl_BindBufferBase(GLenum target, GLuint index, GLuint buffer) {
    if (!g_gl_context) return;
    if (target != GL_UNIFORM_BUFFER) { _gl_set_error(GL_INVALID_ENUM); return; }
    if (index >= GL33_MAX_UNIFORM_BUFFER_BINDINGS) { _gl_set_error(GL_INVALID_VALUE); return; }
    if (buffer >= MAX_BUFFERS || (buffer > 0 && !g_buffers[buffer].in_use)) { _gl_set_error(GL_INVALID_OPERATION); return; }
    g_gl_context->bound_uniform_buffer = buffer;
    g_gl_context->uniform_buffer_bindings[index].buffer = buffer;
    g_gl_context->uniform_buffer_bindings[index].offset = 0;
    g_gl_context->uniform_buffer_bindings[index].size = 0;
    g_gl_context->uniform_buffer_bindings[index].whole_buffer = GL_TRUE;
    g_gl_context->dirty_flags |= GL_DIRTY_UNIFORM_BINDINGS;
}

void _gl_BindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    if (!g_gl_context) return;
    if (target != GL_UNIFORM_BUFFER) { _gl_set_error(GL_INVALID_ENUM); return; }
    if (index >= GL33_MAX_UNIFORM_BUFFER_BINDINGS) { _gl_set_error(GL_INVALID_VALUE); return; }
    if (buffer >= MAX_BUFFERS || (buffer > 0 && !g_buffers[buffer].in_use)) { _gl_set_error(GL_INVALID_OPERATION); return; }
    if (buffer == 0) {
      g_gl_context->bound_uniform_buffer = 0;
      g_gl_context->uniform_buffer_bindings[index].buffer = 0;
      g_gl_context->uniform_buffer_bindings[index].offset = 0;
      g_gl_context->uniform_buffer_bindings[index].size = 0;
      g_gl_context->uniform_buffer_bindings[index].whole_buffer = GL_FALSE;
      g_gl_context->dirty_flags |= GL_DIRTY_UNIFORM_BINDINGS;
      return;
    }
    if (offset < 0 || size <= 0) { _gl_set_error(GL_INVALID_VALUE); return; }
    if ((offset & 0xFF) != 0) { _gl_set_error(GL_INVALID_VALUE); return; }
    if (buffer != 0 && offset + size > g_buffers[buffer].size) { _gl_set_error(GL_INVALID_VALUE); return; }
    g_gl_context->bound_uniform_buffer = buffer;
    g_gl_context->uniform_buffer_bindings[index].buffer = buffer;
    g_gl_context->uniform_buffer_bindings[index].offset = offset;
    g_gl_context->uniform_buffer_bindings[index].size = size;
    g_gl_context->uniform_buffer_bindings[index].whole_buffer = GL_FALSE;
    g_gl_context->dirty_flags |= GL_DIRTY_UNIFORM_BINDINGS;
}

void _gl_BufferData(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage) {
  if (!is_buffer_target(target)) { _gl_set_error(GL_INVALID_ENUM); return; }
  if (size < 0) { _gl_set_error(GL_INVALID_VALUE); return; }
  GLuint id = get_bound_buffer(target);
  if (id == 0) { _gl_set_error(GL_INVALID_OPERATION); return; }
  GLBuffer *buf = &g_buffers[id];
  if (buf->gpu_owned) GX2RDestroyBufferEx(&buf->gx2_buffer, (GX2RResourceFlags)0);
  free_uniform_shadow(buf);
  buf->target = target;
  buf->size = size; buf->usage = usage;
  GX2RResourceFlags bind = GX2R_RESOURCE_BIND_NONE;
  if (target == GL_ARRAY_BUFFER) bind = GX2R_RESOURCE_BIND_VERTEX_BUFFER;
  else if (target == GL_ELEMENT_ARRAY_BUFFER) bind = GX2R_RESOURCE_BIND_INDEX_BUFFER;
  else if (target == GL_UNIFORM_BUFFER) bind = GX2R_RESOURCE_BIND_UNIFORM_BLOCK;
  buf->gx2_buffer.flags = (GX2RResourceFlags)(bind | GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ);
  buf->gx2_buffer.elemSize = 1; buf->gx2_buffer.elemCount = (uint32_t)size;
  if (!GX2RCreateBuffer(&buf->gx2_buffer)) {
    memset(&buf->gx2_buffer, 0, sizeof(buf->gx2_buffer));
    buf->gpu_owned = false;
    _gl_set_error(GL_OUT_OF_MEMORY);
    return;
  }
  buf->gpu_owned = true;
  if (target == GL_UNIFORM_BUFFER) {
    if (!initialize_uniform_shadow(buf, data)) {
      GX2RDestroyBufferEx(&buf->gx2_buffer, (GX2RResourceFlags)0);
      memset(&buf->gx2_buffer, 0, sizeof(buf->gx2_buffer));
      free_uniform_shadow(buf);
      buf->gpu_owned = false;
      _gl_set_error(GL_OUT_OF_MEMORY);
    }
    return;
  }
  if (data) {
    void *ptr = GX2RLockBufferEx(&buf->gx2_buffer, GX2R_RESOURCE_USAGE_CPU_WRITE);
    if (!ptr) { _gl_set_error(GL_OUT_OF_MEMORY); return; }
    memcpy(ptr, data, size);
    GX2RUnlockBufferEx(&buf->gx2_buffer, GX2R_RESOURCE_USAGE_CPU_WRITE);
    GX2RInvalidateBuffer(&buf->gx2_buffer,
                         (GX2RResourceFlags)(GX2R_RESOURCE_USAGE_CPU_WRITE |
                                             GX2R_RESOURCE_USAGE_GPU_READ));
  }
}

void _gl_BufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data) {
  if (!is_buffer_target(target)) { _gl_set_error(GL_INVALID_ENUM); return; }
  if (offset < 0 || size < 0 || !data) { _gl_set_error(GL_INVALID_VALUE); return; }
  GLuint id = get_bound_buffer(target);
  if (id == 0) { _gl_set_error(GL_INVALID_OPERATION); return; }
  GLBuffer *buf = &g_buffers[id];
  if (!buf->gpu_owned || offset + size > buf->size) { _gl_set_error(GL_INVALID_VALUE); return; }
  if (target == GL_UNIFORM_BUFFER) {
    if (!buf->uniform_shadow) { _gl_set_error(GL_INVALID_OPERATION); return; }
    memcpy(buf->uniform_shadow + offset, data, (size_t)size);
    if (!sync_uniform_shadow_range(buf, offset, size)) {
      _gl_set_error(GL_OUT_OF_MEMORY);
    }
    return;
  }
  void *ptr = GX2RLockBufferEx(&buf->gx2_buffer, GX2R_RESOURCE_USAGE_CPU_WRITE);
  if (!ptr) { _gl_set_error(GL_OUT_OF_MEMORY); return; }
  memcpy((uint8_t *)ptr + offset, data, size);
  GX2RUnlockBufferEx(&buf->gx2_buffer, GX2R_RESOURCE_USAGE_CPU_WRITE);
  GX2RInvalidateBuffer(&buf->gx2_buffer,
                       (GX2RResourceFlags)(GX2R_RESOURCE_USAGE_CPU_WRITE |
                                           GX2R_RESOURCE_USAGE_GPU_READ));
}

void _gl_GetBufferParameteriv(GLenum target, GLenum pname, GLint *params) {
  if (!params) return;
  if (!is_buffer_target(target)) { _gl_set_error(GL_INVALID_ENUM); return; }
  GLuint id = get_bound_buffer(target);
  if (id == 0) { _gl_set_error(GL_INVALID_OPERATION); return; }
  GLBuffer *buf = &g_buffers[id];
  switch (pname) {
  case GL_BUFFER_SIZE: *params = (GLint)buf->size; break;
  case GL_BUFFER_USAGE: *params = (GLint)buf->usage; break;
  case GL_BUFFER_ACCESS: *params = (GLint)buf->mapped_access; break;
  case GL_BUFFER_MAPPED: *params = buf->mapped_ptr ? GL_TRUE : GL_FALSE; break;
  default: _gl_set_error(GL_INVALID_ENUM); break;
  }
}

void _gl_GetBufferPointerv(GLenum target, GLenum pname, GLvoid **params) {
  if (!params) return;
  if (!is_buffer_target(target)) { _gl_set_error(GL_INVALID_ENUM); return; }
  GLuint id = get_bound_buffer(target);
  if (id == 0) { _gl_set_error(GL_INVALID_OPERATION); return; }
  if (pname != GL_BUFFER_MAP_POINTER) { _gl_set_error(GL_INVALID_ENUM); return; }
  *params = g_buffers[id].mapped_ptr;
}

void *_gl_MapBuffer(GLenum target, GLenum access) {
  if (!is_buffer_target(target)) { _gl_set_error(GL_INVALID_ENUM); return NULL; }
  GLuint id = get_bound_buffer(target);
  if (id == 0) { _gl_set_error(GL_INVALID_OPERATION); return NULL; }
  GLBuffer *buf = &g_buffers[id];
  if (buf->mapped_ptr) { _gl_set_error(GL_INVALID_OPERATION); return NULL; }
  if (target == GL_UNIFORM_BUFFER) {
    if (!buf->uniform_shadow && !initialize_uniform_shadow(buf, NULL)) {
      _gl_set_error(GL_OUT_OF_MEMORY);
      return NULL;
    }
    buf->mapped_ptr = buf->uniform_shadow;
    buf->mapped_access = access;
    buf->mapped_uniform_shadow = GL_TRUE;
    buf->mapped_offset = 0;
    buf->mapped_length = buf->size;
    return buf->mapped_ptr;
  }
  buf->mapped_ptr = GX2RLockBufferEx(&buf->gx2_buffer, (GX2RResourceFlags)0);
  if (!buf->mapped_ptr) { _gl_set_error(GL_OUT_OF_MEMORY); return NULL; }
  buf->mapped_access = access;
  buf->mapped_uniform_shadow = GL_FALSE;
  buf->mapped_offset = 0;
  buf->mapped_length = buf->size;
  return buf->mapped_ptr;
}

void *_gl_MapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access) {
  if (!is_buffer_target(target)) { _gl_set_error(GL_INVALID_ENUM); return NULL; }
  GLuint id = get_bound_buffer(target);
  if (id == 0) { _gl_set_error(GL_INVALID_OPERATION); return NULL; }
  GLBuffer *buf = &g_buffers[id];
  if (buf->mapped_ptr) { _gl_set_error(GL_INVALID_OPERATION); return NULL; }
  if (offset < 0 || length < 0 || offset + length > buf->size) { _gl_set_error(GL_INVALID_VALUE); return NULL; }
  if (target == GL_UNIFORM_BUFFER) {
    if (!buf->uniform_shadow && !initialize_uniform_shadow(buf, NULL)) {
      _gl_set_error(GL_OUT_OF_MEMORY);
      return NULL;
    }
    buf->mapped_ptr = buf->uniform_shadow + offset;
    buf->mapped_access = (access & GL_MAP_WRITE_BIT) ? GL_WRITE_ONLY : GL_READ_ONLY;
    buf->mapped_uniform_shadow = GL_TRUE;
    buf->mapped_offset = offset;
    buf->mapped_length = length;
    return buf->mapped_ptr;
  }
  void *base = GX2RLockBufferEx(&buf->gx2_buffer, (GX2RResourceFlags)0);
  if (!base) { _gl_set_error(GL_OUT_OF_MEMORY); return NULL; }
  buf->mapped_ptr   = (uint8_t*)base + offset;
  buf->mapped_access = (access & GL_MAP_WRITE_BIT) ? GL_WRITE_ONLY : GL_READ_ONLY;
  buf->mapped_uniform_shadow = GL_FALSE;
  buf->mapped_offset = offset;
  buf->mapped_length = length;
  return buf->mapped_ptr;
}

GLboolean _gl_UnmapBuffer(GLenum target) {
  if (!is_buffer_target(target)) { _gl_set_error(GL_INVALID_ENUM); return GL_FALSE; }
  GLuint id = get_bound_buffer(target);
  if (id == 0) { _gl_set_error(GL_INVALID_OPERATION); return GL_FALSE; }
  GLBuffer *buf = &g_buffers[id];
  if (!buf->mapped_ptr) return GL_FALSE;
  if (buf->mapped_uniform_shadow) {
    GLboolean ok = sync_uniform_shadow_range(buf, buf->mapped_offset, buf->mapped_length) ? GL_TRUE : GL_FALSE;
    buf->mapped_ptr = NULL;
    buf->mapped_uniform_shadow = GL_FALSE;
    buf->mapped_offset = 0;
    buf->mapped_length = 0;
    return ok;
  }
  GX2RUnlockBufferEx(&buf->gx2_buffer, (GX2RResourceFlags)0);
  if (buf->mapped_access == GL_WRITE_ONLY) {
    GX2RInvalidateBuffer(&buf->gx2_buffer,
                         (GX2RResourceFlags)(GX2R_RESOURCE_USAGE_CPU_WRITE |
                                             GX2R_RESOURCE_USAGE_GPU_READ));
  }
  buf->mapped_ptr = NULL;
  buf->mapped_offset = 0;
  buf->mapped_length = 0;
  return GL_TRUE;
}

void _gl_FlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length) {
  if (!is_buffer_target(target)) { _gl_set_error(GL_INVALID_ENUM); return; }
  GLuint id = get_bound_buffer(target);
  if (id == 0) { _gl_set_error(GL_INVALID_OPERATION); return; }
  GLBuffer *buf = &g_buffers[id];
  if (!buf->mapped_ptr) { _gl_set_error(GL_INVALID_OPERATION); return; }
  if (offset < 0 || length < 0) { _gl_set_error(GL_INVALID_VALUE); return; }
  if (buf->mapped_uniform_shadow) {
    if (offset + length > buf->mapped_length) { _gl_set_error(GL_INVALID_VALUE); return; }
    if (!sync_uniform_shadow_range(buf, buf->mapped_offset + offset, length)) {
      _gl_set_error(GL_OUT_OF_MEMORY);
    }
    return;
  }
  DCFlushRange((uint8_t *)buf->mapped_ptr + offset, length);
}

void *gl_buffer_get_data(GLuint id) {
  if (id > 0 && id < MAX_BUFFERS && g_buffers[id].in_use && g_buffers[id].gpu_owned) return g_buffers[id].gx2_buffer.buffer;
  return NULL;
}

GLsizeiptr gl_buffer_get_size(GLuint id) {
  if (id > 0 && id < MAX_BUFFERS && g_buffers[id].in_use) return g_buffers[id].size;
  return 0;
}

GX2RBuffer *gl_buffer_get_gx2r_buffer(GLuint id) {
  if (id > 0 && id < MAX_BUFFERS && g_buffers[id].in_use && g_buffers[id].gpu_owned) return &g_buffers[id].gx2_buffer;
  return NULL;
}

#ifdef __cplusplus
}
#endif
