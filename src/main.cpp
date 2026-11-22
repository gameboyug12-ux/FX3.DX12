#include "fx3/Assets.hpp"
#include "fx3/Renderer.hpp"
#include <cstdio>
#include <filesystem>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    fx3::Mesh mesh;
    fx3::Texture texture;
    char error[512]{};
    const auto model = std::filesystem::path(L"assets/model.obj");
    const auto image = std::filesystem::path(L"assets/albedo.fxtex");
    if (!fx3::LoadOBJ(model, mesh, error, sizeof(error))) {
        std::fprintf(stderr, "FX3: %s\n", error);
        MessageBoxA(nullptr, error, "FX3 Engine v0.4 - OBJ loading failed", MB_ICONERROR);
        return 1;
    }
    if (!fx3::LoadFXTEX(image, texture, error, sizeof(error))) {
        std::fprintf(stderr, "FX3: %s\n", error);
        MessageBoxA(nullptr, error, "FX3 Engine v0.4 - FXTEX loading failed", MB_ICONERROR);
        return 1;
    }
    return fx3::RunRenderer(instance, showCommand, mesh, texture);
}
