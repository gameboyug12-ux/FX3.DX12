#include "XWe/DX/DX12/DX12Materail.hpp"
namespace XWe::DX::DX12 {
void DX12Materail::AddTexture(std::shared_ptr<DX12Texture> texture) {
    if (texture) textures_.push_back(std::move(texture));
}
}
