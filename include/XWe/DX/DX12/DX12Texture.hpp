#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>

namespace XWe::DX::DX12 {
class DX12Texture final {
public:
    HRESULT Create2D(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format,
                     D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
                     D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COMMON) noexcept;
    ID3D12Resource* Get() const noexcept { return resource_.Get(); }
    const D3D12_RESOURCE_DESC& Description() const noexcept { return desc_; }
    void Reset() noexcept { resource_.Reset(); desc_ = {}; }
    explicit operator bool() const noexcept { return resource_ != nullptr; }
private:
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    D3D12_RESOURCE_DESC desc_{};
};
}
