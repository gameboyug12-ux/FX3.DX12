#pragma once

#include "fx3/Assets.hpp"

#include <cstdint>
#include <filesystem>

namespace fx3 {

// FX3 .tex v1 binary layout:
//   uint16 little-endian Rows
//   uint16 little-endian Columns
//   Rows * Columns pixels in row-major order, each pixel RGBA8.
// The header is exactly 4 bytes; there is no magic or padding.
inline constexpr std::uint64_t TEX_MAX_PAYLOAD_BYTES = 512ull * 1024ull * 1024ull;

// Loads an FX3 .tex v1 file into Texture (width = Columns, height = Rows).
// Rejects zero dimensions, payloads above the safety limit, truncated data,
// and unexpected trailing bytes. On failure, output is reset.
bool LoadTEX(const std::filesystem::path& path,
             Texture& output,
             char* error,
             std::size_t errorCapacity);

} // namespace fx3
