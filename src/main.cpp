#include "fx3/engine.hpp"

#include <Windows.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    fx3::EngineConfig config{};
    config.window_title = L"FX3.DX12 | Renderer 0.1.0";
    config.window_width = 1280;
    config.window_height = 720;
#ifdef FX3_DEBUG
    config.enable_debug_layer = true;
#else
    config.enable_debug_layer = false;
#endif

    fx3::Engine engine(config);
    return engine.Run(instance, show_command);
}
