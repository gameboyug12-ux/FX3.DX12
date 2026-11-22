#include "fx3/Assets.hpp"
#include <cstdio>
#include <algorithm>
#include <iterator>
#include <fstream>

namespace fx3 {
// FXTEX v1: 8-byte magic "FXTEX001", uint32 width, uint32 height, then tightly packed RGBA8 pixels.
bool LoadFXTEX(const std::filesystem::path& path, Texture& out, char* error, std::size_t cap) {
    out = {};
    std::ifstream f(path, std::ios::binary);
    if (!f) { if(cap) std::snprintf(error,cap,"Cannot open FXTEX: %s",path.string().c_str()); return false; }
    char magic[8]{}; std::uint32_t w=0,h=0;
    f.read(magic,8); f.read(reinterpret_cast<char*>(&w),4); f.read(reinterpret_cast<char*>(&h),4);
    constexpr char expected[8]={'F','X','T','E','X','0','0','1'};
    const std::uint64_t bytes=static_cast<std::uint64_t>(w)*h*4;
    if (!f || !std::equal(std::begin(magic),std::end(magic),std::begin(expected)) ||
        w==0 || h==0 || bytes>512ull*1024ull*1024ull) {
        if(cap) std::snprintf(error,cap,"Invalid FXTEX v1 header or dimensions"); return false;
    }
    out.width=w; out.height=h; out.rgba.resize(static_cast<std::size_t>(bytes));
    f.read(reinterpret_cast<char*>(out.rgba.data()),static_cast<std::streamsize>(bytes));
    if (!f) { out={}; if(cap) std::snprintf(error,cap,"Truncated FXTEX pixel payload"); return false; }
    return true;
}
}
