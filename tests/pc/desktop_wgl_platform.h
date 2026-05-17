#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool DesktopGLPlatformInitialize(const char *window_title, int width, int height);
void DesktopGLPlatformShutdown(void);
void *DesktopGLPlatformGetProcAddress(const char *name);
void DesktopGLPlatformSwapBuffers(void);
bool DesktopGLPlatformReadBackbufferRGBA8(int width, int height, unsigned char *pixels);

#ifdef __cplusplus
}
#endif
