#include "XWeApplication.h"
#include "XWe/Engine.hpp"

#include <Windows.h>
#include <new>

namespace {
struct XWeApplicationInstance {
    explicit XWeApplicationInstance(const XWe::EngineConfig& config)
        : engine(config) {}
    XWe::Engine engine;
};
}

extern "C" XWE_API XWeApplicationHandle
XWeCreateApplication(const XWeApplicationDesc* desc) {
    try {
        XWe::EngineConfig config{};
        if (desc != nullptr) {
            if (desc->struct_size < sizeof(XWeApplicationDesc)) {
                return nullptr;
            }
            if (desc->window_title != nullptr) config.window_title = desc->window_title;
            if (desc->window_width != 0) config.window_width = desc->window_width;
            if (desc->window_height != 0) config.window_height = desc->window_height;
            config.enable_debug_layer = desc->enable_debug_layer != 0;
        }
        return new (std::nothrow) XWeApplicationInstance(config);
    } catch (...) {
        return nullptr;
    }
}

extern "C" XWE_API int32_t
XWeRunApplication(XWeApplicationHandle application) {
    if (application == nullptr) return -1;
    try {
        auto* instance = static_cast<XWeApplicationInstance*>(application);
        return static_cast<int32_t>(instance->engine.Run(GetModuleHandleW(nullptr), SW_SHOW));
    } catch (...) {
        return -2;
    }
}

extern "C" XWE_API void
XWeDestroyApplication(XWeApplicationHandle application) {
    delete static_cast<XWeApplicationInstance*>(application);
}

extern "C" XWE_API const wchar_t* XWeGetVersion(void) {
    return L"0.3.5";
}
