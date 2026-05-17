#include "gl_mem.h"
#include "Platform/WiiU_Log.hpp"
#include <coreinit/memdefaultheap.h>
#include <coreinit/memexpheap.h>
#include <coreinit/memheap.h>
#include <stdarg.h>
#include <stdio.h>

static MEMHeapHandle mem1_heap        = NULL;
static MEMHeapHandle mem2_heap        = NULL;
static bool mem1_use_default_heap     = false;
static bool mem2_use_default_heap     = false;

#ifndef GX2GL_ENABLE_VERBOSE_FILE_LOGS
#define GX2GL_ENABLE_VERBOSE_FILE_LOGS 0
#endif

static void log_mem_step(const char *format, ...) {
#if GX2GL_ENABLE_VERBOSE_FILE_LOGS
    char buffer[512];
    va_list args;
    FILE *log_file;

    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    WiiU_Log("%s", buffer);

    log_file = fopen(WiiU_GetGX2GLInitLogPath(), "a");
    if (log_file) {
        fprintf(log_file, "[gx2gl-mem] %s\n", buffer);
        fclose(log_file);
    }
#else
    (void)format;
#endif
}

static bool can_use_exp_heap(MEMHeapHandle heap) {
    return heap && heap->tag == MEM_EXPANDED_HEAP_TAG;
}

static MEMHeapHandle get_heap(gl_mem_type_t type) {
    return type == GL_MEM_TYPE_MEM1 ? mem1_heap : mem2_heap;
}

static bool should_use_default_heap(gl_mem_type_t type) {
    return type == GL_MEM_TYPE_MEM1 ? mem1_use_default_heap : mem2_use_default_heap;
}

void gl_mem_init(void) {
    mem1_heap = MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM1);
    mem2_heap = MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM2);

    mem1_use_default_heap = true;
    mem2_use_default_heap = true;

    log_mem_step("gl_mem_init: base heaps mem1=%p tag=0x%X mem2=%p tag=0x%X using default heap for allocations",
                 mem1_heap, mem1_heap ? (unsigned int)mem1_heap->tag : 0u,
                 mem2_heap, mem2_heap ? (unsigned int)mem2_heap->tag : 0u);
}

void gl_mem_shutdown(void) {
    log_mem_step("gl_mem_shutdown: releasing heap references only");
    mem1_heap = NULL;
    mem2_heap = NULL;
    mem1_use_default_heap = false;
    mem2_use_default_heap = false;
}

void* gl_mem_alloc(gl_mem_type_t type, size_t size, uint32_t align) {
    void *ptr = NULL;

    if (align == 0) {
        align = 4;
    }

    ptr = MEMAllocFromDefaultHeapEx((uint32_t)size, (int32_t)align);

    if (!ptr) {
        log_mem_step("gl_mem_alloc: failed type=%u size=%u align=%u",
                     (unsigned int)type, (unsigned int)size, (unsigned int)align);
    }

    return ptr;
}

void gl_mem_free(gl_mem_type_t type, void* ptr) {
    if (!ptr) {
        return;
    }

    (void)type;
    MEMFreeToDefaultHeap(ptr);
}
