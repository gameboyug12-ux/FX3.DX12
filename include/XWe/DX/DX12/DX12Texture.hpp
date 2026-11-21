#pragma once
#include <d3d12.h>
#include <wrl/client.h>

namespace XWe::DX::DX12 {
class DX12Texture final {
public:
    HRESULT Create2D(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COMMON) noexcept;
    void CreateSRV(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE destination,
        const D3D12_SHADER_RESOURCE_VIEW_DESC* description = nullptr) const noexcept;
    void CreateRTV(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE destination,
        const D3D12_RENDER_TARGET_VIEW_DESC* description = nullptr) const noexcept;
    void CreateDSV(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE destination,
        const D3D12_DEPTH_STENCIL_VIEW_DESC* description = nullptr) const noexcept;
    void CreateUAV(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE destination,
        const D3D12_UNORDERED_ACCESS_VIEW_DESC* description = nullptr) const noexcept;
    HRESULT Transition(ID3D12GraphicsCommandList* command_list,
        D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) const noexcept;
    ID3D12Resource* Get() const noexcept { return resource_.Get(); }
    const D3D12_RESOURCE_DESC& Description() const noexcept { return desc_; }
    void Reset() noexcept { resource_.Reset(); desc_ = {}; }
    explicit operator bool() const noexcept { return resource_ != nullptr; }
private:
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    D3D12_RESOURCE_DESC desc_{};
};
}
