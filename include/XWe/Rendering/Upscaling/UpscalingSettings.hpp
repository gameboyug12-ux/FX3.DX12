#pragma once

#include <cstdint>

namespace XWe::Rendering::Upscaling {

struct Resolution final {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    constexpr bool IsValid() const noexcept { return width != 0 && height != 0; }
};

enum class ResolutionPreset : std::uint8_t {
    Native, HD720, HD900, FullHD1080, QHD1440, UHD4K
};

enum class UpscalingMode : std::uint8_t {
    Native, Automatic, Render720p, Render900p, Render1080p, Render1440p
};

struct UpscalingConfig final {
    UpscalingMode mode = UpscalingMode::Automatic;
    bool enabled = true;
};

constexpr Resolution GetResolution(ResolutionPreset preset) noexcept {
    switch (preset) {
    case ResolutionPreset::HD720:      return {1280, 720};
    case ResolutionPreset::HD900:      return {1600, 900};
    case ResolutionPreset::FullHD1080: return {1920, 1080};
    case ResolutionPreset::QHD1440:    return {2560, 1440};
    case ResolutionPreset::UHD4K:      return {3840, 2160};
    case ResolutionPreset::Native:
    default:                           return {};
    }
}

constexpr Resolution SelectRenderResolution(
    Resolution output, UpscalingConfig config = {}) noexcept {
    if (!output.IsValid() || !config.enabled ||
        config.mode == UpscalingMode::Native) return output;

    Resolution requested{};
    switch (config.mode) {
    case UpscalingMode::Render720p:
        requested = GetResolution(ResolutionPreset::HD720); break;
    case UpscalingMode::Render900p:
        requested = GetResolution(ResolutionPreset::HD900); break;
    case UpscalingMode::Render1080p:
        requested = GetResolution(ResolutionPreset::FullHD1080); break;
    case UpscalingMode::Render1440p:
        requested = GetResolution(ResolutionPreset::QHD1440); break;
    case UpscalingMode::Automatic:
    default:
        if (output.width <= 1280 || output.height <= 720) return output;
        if (output.width <= 1920 && output.height <= 1080)
            requested = GetResolution(ResolutionPreset::HD720);
        else if (output.width <= 2560 && output.height <= 1440)
            requested = GetResolution(ResolutionPreset::FullHD1080);
        else
            requested = GetResolution(ResolutionPreset::QHD1440);
        break;
    }

    if (requested.width >= output.width || requested.height >= output.height)
        return output;
    return requested;
}

} // namespace XWe::Rendering::Upscaling
