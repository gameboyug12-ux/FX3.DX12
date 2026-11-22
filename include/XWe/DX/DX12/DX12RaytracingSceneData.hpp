#pragma once
#include <cstdint>
#include <type_traits>

namespace XWe::DX::DX12 {

struct alignas(16) RTVertex final {
    float position[3]{};
    float normal[3]{};
    float uv[2]{};
    float color[4]{1.0f, 1.0f, 1.0f, 1.0f};
};

struct alignas(16) RTGeometryInfo final {
    std::uint32_t vertex_offset = 0;
    std::uint32_t first_index = 0;
    std::uint32_t material_index = 0;
    std::uint32_t flags = 0;
};

struct alignas(16) RTMaterial final {
    float base_color[4]{0.8f, 0.8f, 0.8f, 1.0f};
    float emission[4]{0.0f, 0.0f, 0.0f, 0.0f};
    float metallic = 0.0f;
    float roughness = 0.6f;
    float padding[2]{};
};

inline constexpr std::uint32_t kRTMaxGeometriesPerInstance = 16;
inline constexpr std::uint32_t kRTSceneSrvDescriptorCount = 5;
inline constexpr std::uint32_t kRTOutputUavDescriptorCount = 1;

static_assert(sizeof(RTVertex) == 48);
static_assert(sizeof(RTGeometryInfo) == 16);
static_assert(sizeof(RTMaterial) == 48);
static_assert(std::is_standard_layout_v<RTVertex>);
static_assert(std::is_standard_layout_v<RTGeometryInfo>);
static_assert(std::is_standard_layout_v<RTMaterial>);

} // namespace XWe::DX::DX12
