#include "XWe/DX/DX12/DX12GBuffer.hpp"
namespace XWe::DX::DX12 {
HRESULT DX12GBuffer::Create(DX12Ctx& context, UINT width, UINT height) noexcept {
    if (!context.Device() || !width || !height) return E_INVALIDARG;
    const DXGI_FORMAT formats[] = {DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R16G16B16A16_FLOAT,
        DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R16G16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT};
    std::array<DX12Texture, static_cast<unsigned>(DX12GBufferSlot::Count)> created;
    for (unsigned i=0;i<created.size();++i) {
        HRESULT hr = created[i].Create2D(context.Device(),width,height,formats[i],
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        if (FAILED(hr)) return hr;
    }
    targets_ = std::move(created); width_ = width; height_ = height; return S_OK;
}
void DX12GBuffer::Reset() noexcept { for(auto& target:targets_) target.Reset(); width_=height_=0; }
DX12Texture* DX12GBuffer::Get(DX12GBufferSlot slot) noexcept {
    const auto i=static_cast<unsigned>(slot); return i<targets_.size()?&targets_[i]:nullptr;
}
const DX12Texture* DX12GBuffer::Get(DX12GBufferSlot slot) const noexcept {
    const auto i=static_cast<unsigned>(slot); return i<targets_.size()?&targets_[i]:nullptr;
}
}
