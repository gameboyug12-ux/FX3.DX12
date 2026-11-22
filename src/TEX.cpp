#include "fx3/TEX.hpp"

#include <cstdio>
#include <fstream>
#include <limits>
#include <vector>

namespace fx3 {
namespace {

bool ReadU16LE(std::istream& stream, std::uint16_t& value) {
    unsigned char bytes[2]{};
    if (!stream.read(reinterpret_cast<char*>(bytes), sizeof(bytes))) {
        return false;
    }

    value = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(bytes[0]) |
        (static_cast<std::uint16_t>(bytes[1]) << 8));
    return true;
}

void SetError(char* error, std::size_t capacity, const char* message,
              const std::filesystem::path& path) {
    if (error != nullptr && capacity > 0) {
        std::snprintf(error, capacity, message, path.string().c_str());
    }
}

} // namespace

bool LoadTEX(const std::filesystem::path& path,
             Texture& output,
             char* error,
             std::size_t errorCapacity) {
    output = {};

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        SetError(error, errorCapacity, "Cannot open TEX file: %s", path);
        return false;
    }

    std::uint16_t rows = 0;
    std::uint16_t columns = 0;
    if (!ReadU16LE(file, rows) || !ReadU16LE(file, columns)) {
        if (error != nullptr && errorCapacity > 0) {
            std::snprintf(error, errorCapacity, "TEX header is truncated");
        }
        return false;
    }

    if (rows == 0 || columns == 0) {
        if (error != nullptr && errorCapacity > 0) {
            std::snprintf(error, errorCapacity,
                          "TEX Rows and Columns must both be non-zero");
        }
        return false;
    }

    constexpr std::uint64_t bytesPerPixel = 4;
    const std::uint64_t pixelCount =
        static_cast<std::uint64_t>(rows) * static_cast<std::uint64_t>(columns);
    const std::uint64_t payloadBytes = pixelCount * bytesPerPixel;

    if (payloadBytes > TEX_MAX_PAYLOAD_BYTES ||
        payloadBytes > static_cast<std::uint64_t>(
                           (std::numeric_limits<std::size_t>::max)())) {
        if (error != nullptr && errorCapacity > 0) {
            std::snprintf(error, errorCapacity,
                          "TEX pixel payload exceeds the 512 MiB limit");
        }
        return false;
    }

    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(payloadBytes));
    if (!file.read(reinterpret_cast<char*>(pixels.data()),
                   static_cast<std::streamsize>(pixels.size()))) {
        if (error != nullptr && errorCapacity > 0) {
            std::snprintf(error, errorCapacity, "TEX pixel data is truncated");
        }
        return false;
    }

    // The v1 file must contain exactly the declared pixel payload.
    char extraByte = 0;
    if (file.read(&extraByte, 1)) {
        if (error != nullptr && errorCapacity > 0) {
            std::snprintf(error, errorCapacity,
                          "TEX file contains unexpected trailing data");
        }
        return false;
    }

    output.width = columns;
    output.height = rows;
    output.rgba = std::move(pixels);
    return true;
}

} // namespace fx3
