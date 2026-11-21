#pragma once

#include <Windows.h>

#include <memory>

namespace XWe {

struct EngineConfig {
    const wchar_t* window_title = L"FX3.DX12";
    unsigned int window_width = 1280;
    unsigned int window_height = 720;
    bool enable_debug_layer = true;
};

class Engine final {
public:
    explicit Engine(const EngineConfig& config);
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    int Run(HINSTANCE instance, int show_command);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace XWe
