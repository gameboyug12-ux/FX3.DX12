#pragma once
#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <vector>

namespace fx3 {
struct Vertex {
    float position[3]{};
    float uv[2]{};
    float normal[3]{};
};
struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};
struct Texture {
    std::uint32_t width{}, height{};
    std::vector<std::uint8_t> rgba;
};
bool LoadOBJ(const std::filesystem::path& path, Mesh& output, char* error, std::size_t errorCapacity);
bool LoadFXTEX(const std::filesystem::path& path, Texture& output, char* error, std::size_t errorCapacity);
}
