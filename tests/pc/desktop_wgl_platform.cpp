#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "desktop_wgl_platform.h"

#include <windows.h>
#include <GL/gl.h>

#ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#endif
#ifndef WGL_CONTEXT_MINOR_VERSION_ARB
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#endif
#ifndef WGL_CONTEXT_PROFILE_MASK_ARB
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#endif
#ifndef WGL_CONTEXT_CORE_PROFILE_BIT_ARB
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#endif

typedef HGLRC(WINAPI *PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int *);

typedef struct DesktopGLPlatformState {
    HMODULE opengl32;
    HWND hwnd;
    HDC hdc;
    HGLRC hglrc;
} DesktopGLPlatformState;

static DesktopGLPlatformState g_platform = {0};

static LRESULT CALLBACK desktop_gl_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

static bool desktop_gl_register_class(void) {
    static bool registered = false;
    WNDCLASSA wc;

    if (registered) {
        return true;
    }

    ZeroMemory(&wc, sizeof(wc));
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = desktop_gl_window_proc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "GX2GLDesktopGL33Window";

    if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    registered = true;
    return true;
}

static bool desktop_gl_setup_pixel_format(HDC hdc) {
    PIXELFORMATDESCRIPTOR pfd;
    int pixel_format;

    ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    pixel_format = ChoosePixelFormat(hdc, &pfd);
    if (pixel_format == 0) {
        return false;
    }

    if (!SetPixelFormat(hdc, pixel_format, &pfd)) {
        return false;
    }

    return true;
}

static void *desktop_gl_get_any_proc_address(const char *name) {
    PROC proc = NULL;

    if (!g_platform.opengl32) {
        return NULL;
    }

    proc = wglGetProcAddress(name);
    if (proc == NULL || proc == (PROC)0x1 || proc == (PROC)0x2 ||
        proc == (PROC)0x3 || proc == (PROC)-1) {
        proc = GetProcAddress(g_platform.opengl32, name);
    }

    return (void *)proc;
}

bool DesktopGLPlatformInitialize(const char *window_title, int width, int height) {
    PFNWGLCREATECONTEXTATTRIBSARBPROC create_context_attribs = NULL;
    HGLRC legacy_context = NULL;
    int attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };

    if (g_platform.hglrc) {
        return true;
    }

    if (!desktop_gl_register_class()) {
        return false;
    }

    g_platform.opengl32 = LoadLibraryA("opengl32.dll");
    if (!g_platform.opengl32) {
        return false;
    }

    g_platform.hwnd = CreateWindowExA(
        0, "GX2GLDesktopGL33Window", window_title ? window_title : "gx2gl-pc",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        NULL, NULL, GetModuleHandleA(NULL), NULL);
    if (!g_platform.hwnd) {
        DesktopGLPlatformShutdown();
        return false;
    }

    g_platform.hdc = GetDC(g_platform.hwnd);
    if (!g_platform.hdc || !desktop_gl_setup_pixel_format(g_platform.hdc)) {
        DesktopGLPlatformShutdown();
        return false;
    }

    legacy_context = wglCreateContext(g_platform.hdc);
    if (!legacy_context) {
        DesktopGLPlatformShutdown();
        return false;
    }

    if (!wglMakeCurrent(g_platform.hdc, legacy_context)) {
        wglDeleteContext(legacy_context);
        DesktopGLPlatformShutdown();
        return false;
    }

    create_context_attribs = (PFNWGLCREATECONTEXTATTRIBSARBPROC)
        desktop_gl_get_any_proc_address("wglCreateContextAttribsARB");
    if (!create_context_attribs) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(legacy_context);
        DesktopGLPlatformShutdown();
        return false;
    }

    g_platform.hglrc = create_context_attribs(g_platform.hdc, NULL, attribs);
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(legacy_context);

    if (!g_platform.hglrc) {
        DesktopGLPlatformShutdown();
        return false;
    }

    if (!wglMakeCurrent(g_platform.hdc, g_platform.hglrc)) {
        DesktopGLPlatformShutdown();
        return false;
    }

    ShowWindow(g_platform.hwnd, SW_HIDE);
    return true;
}

void DesktopGLPlatformShutdown(void) {
    if (g_platform.hglrc) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(g_platform.hglrc);
        g_platform.hglrc = NULL;
    }
    if (g_platform.hdc && g_platform.hwnd) {
        ReleaseDC(g_platform.hwnd, g_platform.hdc);
        g_platform.hdc = NULL;
    }
    if (g_platform.hwnd) {
        DestroyWindow(g_platform.hwnd);
        g_platform.hwnd = NULL;
    }
    if (g_platform.opengl32) {
        FreeLibrary(g_platform.opengl32);
        g_platform.opengl32 = NULL;
    }
}

void *DesktopGLPlatformGetProcAddress(const char *name) {
    return desktop_gl_get_any_proc_address(name);
}

void DesktopGLPlatformSwapBuffers(void) {
    if (g_platform.hdc) {
        SwapBuffers(g_platform.hdc);
    }
}

bool DesktopGLPlatformReadBackbufferRGBA8(int width, int height, unsigned char *pixels) {
    if (!pixels || width < 0 || height < 0) {
        return false;
    }

    if (width == 0 || height == 0) {
        return true;
    }

    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    return true;
}
