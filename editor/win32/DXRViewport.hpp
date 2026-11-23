#pragma once
#include <Windows.h>
#include <memory>
#include <string>
class DXRViewport final {
public:
    DXRViewport();
    ~DXRViewport();
    DXRViewport(const DXRViewport&)=delete;
    DXRViewport& operator=(const DXRViewport&)=delete;
    void initialize(HWND hwnd,UINT width,UINT height);
    void resize(UINT width,UINT height);
    void render();
    std::wstring status() const;
private:
    class Renderer;
    std::unique_ptr<Renderer> renderer_;
};
