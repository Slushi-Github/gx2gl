#include "gx2gl/sdl_bridge.h"

#include "core/gl_context.h"
#include "core/gl_framebuffer.h"
#include "mem/gl_mem.h"
#include "whb/gfx.h"
#include "whb/proc.h"
#include "gx2gl/proc.h"

#include <gx2/swap.h>
#include <gx2/event.h>
#include <proc_ui/procui.h>

typedef struct
{
    int library_refs;
    int context_refs;
    int proc_owned;
    int proc_ready;
    int gfx_owned;
    int gfx_ready;
    int mem_ready;
    int render_active;
    int render_tv_active;
    int render_drc_active;
    int swap_interval;
    int automatic_drc_enabled;
    int default_target_drc;
    gl_context_t *context;
} GX2GLSDLState;

static GX2GLSDLState g_gx2gl_sdl_state = {0};

typedef enum
{
    GX2GL_RENDER_TARGET_NONE = 0,
    GX2GL_RENDER_TARGET_TV,
    GX2GL_RENDER_TARGET_DRC
} GX2GLRenderTarget;

static GX2GLRenderTarget gx2glGetDefaultRenderTarget(void)
{
    if (g_gx2gl_sdl_state.default_target_drc) {
        if (WHBGfxGetDRCColourBuffer()) {
            return GX2GL_RENDER_TARGET_DRC;
        }
        if (WHBGfxGetTVColourBuffer()) {
            return GX2GL_RENDER_TARGET_TV;
        }
        return GX2GL_RENDER_TARGET_NONE;
    }

    if (WHBGfxGetTVColourBuffer()) {
        return GX2GL_RENDER_TARGET_TV;
    }
    if (WHBGfxGetDRCColourBuffer()) {
        return GX2GL_RENDER_TARGET_DRC;
    }
    return GX2GL_RENDER_TARGET_NONE;
}

static int gx2glEnsureGraphicsReady(void)
{
    if (!g_gx2gl_sdl_state.proc_ready) {
        if (!ProcUIIsRunning()) {
            WHBProcInit();
            g_gx2gl_sdl_state.proc_owned = 1;
        }
        g_gx2gl_sdl_state.proc_ready = 1;
    }

    if (!g_gx2gl_sdl_state.gfx_ready) {
        if (!WHBGfxGetTVColourBuffer() && !WHBGfxGetDRCColourBuffer()) {
            if (!WHBGfxInit()) {
                return -1;
            }
            g_gx2gl_sdl_state.gfx_owned = 1;
        }
        if (!WHBGfxGetTVColourBuffer() && !WHBGfxGetDRCColourBuffer()) {
            return -1;
        }
        g_gx2gl_sdl_state.gfx_ready = 1;
    }

    if (!g_gx2gl_sdl_state.mem_ready) {
        gl_mem_init();
        g_gx2gl_sdl_state.mem_ready = 1;
    }

    return 0;
}

static void gx2glBeginFrameIfNeeded(void)
{
    GX2GLRenderTarget target;

    if (g_gx2gl_sdl_state.render_active) {
        return;
    }
    if (gx2glEnsureGraphicsReady() != 0) {
        return;
    }

    target = gx2glGetDefaultRenderTarget();
    if (target == GX2GL_RENDER_TARGET_NONE) {
        return;
    }

    WHBGfxBeginRender();
    if (target == GX2GL_RENDER_TARGET_TV) {
        WHBGfxBeginRenderTV();
        g_gx2gl_sdl_state.render_tv_active = 1;
    }
    if (target == GX2GL_RENDER_TARGET_DRC) {
        WHBGfxBeginRenderDRC();
        g_gx2gl_sdl_state.render_drc_active = 1;
    }
    g_gx2gl_sdl_state.render_active = 1;
}

static void gx2glEndFrameIfNeeded(void)
{
    if (!g_gx2gl_sdl_state.render_active) {
        return;
    }

    if (g_gx2gl_sdl_state.render_drc_active) {
        WHBGfxFinishRenderDRC();
        g_gx2gl_sdl_state.render_drc_active = 0;
    }
    if (g_gx2gl_sdl_state.render_tv_active) {
        WHBGfxFinishRenderTV();
        g_gx2gl_sdl_state.render_tv_active = 0;
    }
    WHBGfxFinishRender();
    g_gx2gl_sdl_state.render_active = 0;
}

int GX2GL_LoadLibrary(const char *path)
{
    (void) path;

    if (gx2glEnsureGraphicsReady() != 0) {
        return -1;
    }

    g_gx2gl_sdl_state.library_refs += 1;
    return 0;
}

void GX2GL_UnloadLibrary(void)
{
    if (g_gx2gl_sdl_state.library_refs > 0) {
        g_gx2gl_sdl_state.library_refs -= 1;
    }

    if (g_gx2gl_sdl_state.library_refs > 0 ||
        g_gx2gl_sdl_state.context_refs > 0) {
        return;
    }

    gx2glEndFrameIfNeeded();

    if (g_gx2gl_sdl_state.mem_ready) {
        gl_mem_shutdown();
        g_gx2gl_sdl_state.mem_ready = 0;
    }

    if (g_gx2gl_sdl_state.gfx_owned) {
        WHBGfxShutdown();
        g_gx2gl_sdl_state.gfx_owned = 0;
    }
    g_gx2gl_sdl_state.gfx_ready = 0;

    if (g_gx2gl_sdl_state.proc_owned) {
        WHBProcShutdown();
        g_gx2gl_sdl_state.proc_owned = 0;
    }
    g_gx2gl_sdl_state.proc_ready = 0;
}

void* GX2GL_GetSDLProcAddress(const char *proc)
{
    if (!proc) {
        return NULL;
    }
    return GX2GL_GetProcAddress(proc);
}

GX2GL_Context GX2GL_CreateContext(void)
{
    if (GX2GL_LoadLibrary(NULL) != 0) {
        return NULL;
    }

    if (!g_gx2gl_sdl_state.context) {
        g_gx2gl_sdl_state.context = gl_context_create();
        if (!g_gx2gl_sdl_state.context) {
            GX2GL_UnloadLibrary();
            return NULL;
        }
    }

    g_gx2gl_sdl_state.context_refs += 1;
    g_gl_context = g_gx2gl_sdl_state.context;
    gl_framebuffer_set_default_target_drc(g_gx2gl_sdl_state.default_target_drc ? GL_TRUE : GL_FALSE);
    gx2glBeginFrameIfNeeded();
    return (GX2GL_Context) g_gx2gl_sdl_state.context;
}

int GX2GL_MakeCurrent(GX2GL_Context context)
{
    if (context && g_gx2gl_sdl_state.library_refs <= 0) {
        return -1;
    }
    if (context && (gl_context_t*) context != g_gx2gl_sdl_state.context) {
        return -1;
    }

    g_gl_context = (gl_context_t*) context;

    if (g_gl_context) {
        gx2glBeginFrameIfNeeded();
    } else {
        gx2glEndFrameIfNeeded();
    }

    return 0;
}

void GX2GL_DeleteContext(GX2GL_Context context)
{
    if ((gl_context_t*) context == g_gx2gl_sdl_state.context) {
        if (g_gx2gl_sdl_state.context_refs > 0) {
            g_gx2gl_sdl_state.context_refs -= 1;
        }

        if (g_gl_context == g_gx2gl_sdl_state.context) {
            g_gl_context = NULL;
        }

        if (g_gx2gl_sdl_state.context_refs == 0) {
            gx2glEndFrameIfNeeded();
            gl_context_destroy(g_gx2gl_sdl_state.context);
            g_gx2gl_sdl_state.context = NULL;
            g_gl_context = NULL;
        }
    }

    if (g_gx2gl_sdl_state.context_refs == 0) {
        GX2GL_UnloadLibrary();
    }
}

int GX2GL_SetSwapInterval(int interval)
{
    if (interval < -1 || interval > 1) {
        return -1;
    }
    g_gx2gl_sdl_state.swap_interval = interval;
    return 0;
}

int GX2GL_GetSwapInterval(void)
{
    return g_gx2gl_sdl_state.swap_interval;
}

int GX2GL_SwapWindow(void)
{
    if (!g_gl_context || g_gx2gl_sdl_state.library_refs <= 0) {
        return -1;
    }

    _gl_Flush();
    if (!g_gx2gl_sdl_state.default_target_drc &&
        g_gx2gl_sdl_state.automatic_drc_enabled) {
        GX2GL_CopyToDRC();
    }
    gx2glEndFrameIfNeeded();
    gx2glBeginFrameIfNeeded();
    return 0;
}

int GX2GL_SetAutomaticDRCEnabled(int enabled)
{
    g_gx2gl_sdl_state.automatic_drc_enabled = enabled ? 1 : 0;
    return 0;
}

int GX2GL_GetAutomaticDRCEnabled(void)
{
    return g_gx2gl_sdl_state.automatic_drc_enabled;
}

int GX2GL_SetDefaultFramebufferTargetDRC(int enabled)
{
    g_gx2gl_sdl_state.default_target_drc = enabled ? 1 : 0;
    if (g_gx2gl_sdl_state.context) {
        gl_framebuffer_set_default_target_drc(enabled ? GL_TRUE : GL_FALSE);
    }
    if (g_gx2gl_sdl_state.render_active) {
        gx2glEndFrameIfNeeded();
        gx2glBeginFrameIfNeeded();
    }
    return 0;
}

int GX2GL_GetDefaultFramebufferTargetDRC(void)
{
    return g_gx2gl_sdl_state.default_target_drc;
}

int GX2GL_CopyToDRC(void)
{
    GX2ColorBuffer *tv_color;

    if (!g_gl_context || g_gx2gl_sdl_state.library_refs <= 0) {
        return -1;
    }
    if (gx2glEnsureGraphicsReady() != 0) {
        return -1;
    }

    tv_color = WHBGfxGetTVColourBuffer();
    if (!tv_color || !WHBGfxGetDRCColourBuffer()) {
        return -1;
    }

    _gl_Flush();
    GX2DrawDone();
    GX2CopyColorBufferToScanBuffer(tv_color, GX2_SCAN_TARGET_DRC);
    GX2Flush();
    return 0;
}
