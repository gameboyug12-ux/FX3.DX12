#pragma once
#include <stdint.h>
#include <wchar.h>

#if defined(_WIN32)
  #if defined(XWE_BUILD_DLL)
    #define XWE_API __declspec(dllexport)
  #else
    #define XWE_API __declspec(dllimport)
  #endif
#else
  #define XWE_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void* XWeApplicationHandle;

typedef enum XWeUpscalingMode {
    XWE_UPSCALING_AUTOMATIC = 0,
    XWE_UPSCALING_NATIVE = 1,
    XWE_UPSCALING_RENDER_720P = 2,
    XWE_UPSCALING_RENDER_900P = 3,
    XWE_UPSCALING_RENDER_1080P = 4,
    XWE_UPSCALING_RENDER_1440P = 5
} XWeUpscalingMode;

typedef struct XWeApplicationDesc {
    uint32_t struct_size;
    const wchar_t* window_title;
    uint32_t window_width;
    uint32_t window_height;
    uint32_t enable_debug_layer;
    /* Optional fixed render resolution mode; zero selects automatic. */
    uint32_t upscaling_mode;
} XWeApplicationDesc;

/* Creates an engine application instance. Returns NULL on failure. */
XWE_API XWeApplicationHandle XWeCreateApplication(const XWeApplicationDesc* desc);

/* Runs the Win32 / Direct3D 12 application loop. */
XWE_API int32_t XWeRunApplication(XWeApplicationHandle application);

/* Destroys an application created by XWeCreateApplication. */
XWE_API void XWeDestroyApplication(XWeApplicationHandle application);

/* Returns a static, null-terminated engine version string. */
XWE_API const wchar_t* XWeGetVersion(void);

#ifdef __cplusplus
}
#endif
