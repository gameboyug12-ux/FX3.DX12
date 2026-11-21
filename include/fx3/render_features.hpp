#pragma once
#include <cstdint>

namespace XWe {

enum class AntiAliasingMode : std::uint32_t {
    None = 0,
    MSAA2x,
    MSAA4x,
    MSAA8x,
    TAA,
    FSR3_1
};

enum class UpscalingMode : std::uint32_t {
    Native = 0,
    FSR3_1_Quality,
    FSR3_1_Balanced,
    FSR3_1_Performance,
    FSR3_1_UltraPerformance
};

enum class FrameGenerationMode : std::uint32_t {
    Disabled = 0,
    FSR3_1
};

/*
 * Configuration contract only. These options become active after the native
 * D3D12 renderer is wired to the AMD FidelityFX SDK and supplies depth,
 * motion vectors, jitter, exposure and frame pacing data.
 */
struct RenderFeatureConfig {
    AntiAliasingMode anti_aliasing = AntiAliasingMode::TAA;
    UpscalingMode upscaling = UpscalingMode::Native;
    FrameGenerationMode frame_generation = FrameGenerationMode::Disabled;
    std::uint32_t render_width = 0;
    std::uint32_t render_height = 0;
};

} // namespace XWe
