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

struct alignas(16) RTFrameData final {
    float inverse_view_projection[16]{};
    float camera_position[3]{};
    float exposure = 1.0f;
    float sun_direction[3]{-0.45f, 0.82f, -0.34f};
    float sun_intensity = 3.5f;
    float sun_color[3]{1.0f, 0.93f, 0.82f};
    float gi_intensity = 1.0f;
    float background_color[3]{0.035f, 0.045f, 0.065f};
    float padding = 0.0f;
};

inline constexpr std::uint32_t kRTMaxGeometriesPerInstance = 16;
inline constexpr std::uint32_t kRTPathTraceSrvDescriptorCount = 5;
inline constexpr std::uint32_t kRTHybridGBufferSrvDescriptorCount = 12;
inline constexpr std::uint32_t kRTOutputUavDescriptorCount = 1;

static_assert(sizeof(RTVertex) == 48);
static_assert(sizeof(RTGeometryInfo) == 16);
static_assert(sizeof(RTMaterial) == 48);
static_assert(sizeof(RTFrameData) == 128);
static_assert(std::is_standard_layout_v<RTVertex>);
static_assert(std::is_standard_layout_v<RTGeometryInfo>);
static_assert(std::is_standard_layout_v<RTMaterial>);
static_assert(std::is_standard_layout_v<RTFrameData>);

} // namespace XWe::DX::DX12
