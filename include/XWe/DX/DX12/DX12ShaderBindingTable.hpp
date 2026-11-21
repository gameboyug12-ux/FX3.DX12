#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <cstddef>

namespace XWe::DX::DX12 {
class DX12ShaderBindingTable final {
public:
    HRESULT Create(ID3D12Device* device, ID3D12StateObjectProperties* properties,
                   const wchar_t* ray_generation, const wchar_t* miss,
                   const wchar_t* hit_group, UINT record_stride = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) noexcept;
    const D3D12_DISPATCH_RAYS_DESC& DispatchDescription(UINT width, UINT height, UINT depth = 1) const noexcept;
    ID3D12Resource* Resource() const noexcept { return buffer_.Get(); }
private:
    Microsoft::WRL::ComPtr<ID3D12Resource> buffer_;
    D3D12_GPU_VIRTUAL_ADDRESS_RANGE raygen_{};
    D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE miss_{};
    D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE hit_{};
    mutable D3D12_DISPATCH_RAYS_DESC dispatch_{};
};
}
