#include "XWeApplication.h"
#include <Windows.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    XWeApplicationDesc desc{};
    desc.struct_size = sizeof(desc);
    desc.window_title = L"FX3.DX12 | XWe Engine 0.3.7";
    desc.window_width = 1600;
    desc.window_height = 900;
#ifdef XWE_DEBUG
    desc.enable_debug_layer = 1;
#else
    desc.enable_debug_layer = 0;
#endif
    XWeApplicationHandle app = XWeCreateApplication(&desc);
    if (app == nullptr) {
        MessageBoxW(nullptr, L"Could not create XWe application.", L"FX3.DX12",
                    MB_OK | MB_ICONERROR);
        return 1;
    }
    const int result = XWeRunApplication(app);
    XWeDestroyApplication(app);
    return result;
}
