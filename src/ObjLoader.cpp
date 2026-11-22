#include "fx3/Assets.hpp"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

namespace fx3 {
namespace {
struct Key { int p{}, t{}, n{}; bool operator==(const Key&) const = default; };
struct KeyHash { std::size_t operator()(const Key& k) const noexcept {
    auto h = static_cast<std::size_t>(k.p);
    h = h * 73856093u ^ static_cast<std::size_t>(k.t + 1) * 19349663u;
    return h ^ static_cast<std::size_t>(k.n + 1) * 83492791u;
}};
int Resolve(int i, std::size_t count) { return i > 0 ? i - 1 : (i < 0 ? static_cast<int>(count) + i : -1); }
bool ParseKey(const std::string& s, Key& k) {
    k = {};
    const auto a = s.find('/');
    if (a == std::string::npos) return std::sscanf(s.c_str(), "%d", &k.p) == 1;
    if (std::sscanf(s.c_str(), "%d", &k.p) != 1) return false;
    const auto b = s.find('/', a + 1);
    if (b == std::string::npos) {
        if (a + 1 < s.size()) std::sscanf(s.c_str() + a + 1, "%d", &k.t);
    } else {
        if (b > a + 1) std::sscanf(s.c_str() + a + 1, "%d", &k.t);
        if (b + 1 < s.size()) std::sscanf(s.c_str() + b + 1, "%d", &k.n);
    }
    return true;
}
}
bool LoadOBJ(const std::filesystem::path& path, Mesh& out, char* error, std::size_t cap) {
    out = {};
    std::ifstream file(path);
    if (!file) { if (cap) std::snprintf(error, cap, "Cannot open OBJ: %s", path.string().c_str()); return false; }
    std::vector<float> positions, uvs, normals;
    std::unordered_map<Key, std::uint32_t, KeyHash> unique;
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream row(line);
        std::string tag; row >> tag;
        if (tag.empty() || tag[0] == '#') continue;
        if (tag == "v") { float x,y,z; if (row >> x >> y >> z) { positions.insert(positions.end(), {x,y,z}); } }
        else if (tag == "vt") { float u,v; if (row >> u >> v) { uvs.insert(uvs.end(), {u,1.0f-v}); } }
        else if (tag == "vn") { float x,y,z; if (row >> x >> y >> z) normals.insert(normals.end(), {x,y,z}); }
        else if (tag == "f") {
            std::vector<std::uint32_t> face;
            std::string token;
            while (row >> token) {
                Key k; if (!ParseKey(token,k)) continue;
                k.p = Resolve(k.p, positions.size()/3);
                k.t = k.t ? Resolve(k.t, uvs.size()/2) : -1;
                k.n = k.n ? Resolve(k.n, normals.size()/3) : -1;
                if (k.p < 0 || static_cast<std::size_t>(k.p*3+2) >= positions.size()) {
                    if (cap) std::snprintf(error,cap,"OBJ face references an invalid position"); return false;
                }
                auto it = unique.find(k);
                if (it == unique.end()) {
                    Vertex v{};
                    for (int i=0;i<3;++i) v.position[i]=positions[static_cast<std::size_t>(k.p)*3+i];
                    if (k.t>=0 && static_cast<std::size_t>(k.t*2+1)<uvs.size()) for(int i=0;i<2;++i) v.uv[i]=uvs[static_cast<std::size_t>(k.t)*2+i];
                    if (k.n>=0 && static_cast<std::size_t>(k.n*3+2)<normals.size()) for(int i=0;i<3;++i) v.normal[i]=normals[static_cast<std::size_t>(k.n)*3+i];
                    const auto index=static_cast<std::uint32_t>(out.vertices.size());
                    out.vertices.push_back(v); unique.emplace(k,index); face.push_back(index);
                } else face.push_back(it->second);
            }
            for (std::size_t i=1;i+1<face.size();++i) {
                out.indices.push_back(face[0]); out.indices.push_back(face[i]); out.indices.push_back(face[i+1]);
            }
        }
    }
    if (out.vertices.empty() || out.indices.empty()) {
        if (cap) std::snprintf(error,cap,"OBJ contains no triangulatable faces"); return false;
    }
    // Supply a stable default normal when the OBJ omits vn data.
    for (auto& v : out.vertices) if (v.normal[0]==0 && v.normal[1]==0 && v.normal[2]==0) v.normal[1]=1.0f;
    return true;
}
}
