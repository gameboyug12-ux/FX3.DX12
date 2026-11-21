#pragma once
#include "DX12Shader.hpp"
#include "DX12Texture.hpp"
#include <memory>
#include <vector>
#include <utility>

namespace XWe::DX::DX12 {
class DX12Materail final {
public:
    void SetShader(std::shared_ptr<DX12Shader> shader) noexcept { shader_ = std::move(shader); }
    void AddTexture(std::shared_ptr<DX12Texture> texture);
    void ClearTextures() noexcept { textures_.clear(); }
    const std::shared_ptr<DX12Shader>& Shader() const noexcept { return shader_; }
    const std::vector<std::shared_ptr<DX12Texture>>& Textures() const noexcept { return textures_; }
private:
    std::shared_ptr<DX12Shader> shader_;
    std::vector<std::shared_ptr<DX12Texture>> textures_;
};
using DX12Material = DX12Materail;
}
