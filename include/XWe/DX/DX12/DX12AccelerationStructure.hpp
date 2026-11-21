#pragma once
#include <d3d12.h>
#include <wrl/client.h>

namespace XWe::DX::DX12 {
class DX12AccelerationStructure final {
public:
    HRESULT BuildBottomLevel(ID3D12Device5* device, ID3D12GraphicsCommandList4* command_list,
        const D3D12_RAYTRACING_GEOMETRY_DESC* geometries, UINT geometry_count,
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags =
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE) noexcept;
    HRESULT BuildTopLevel(ID3D12Device5* device, ID3D12GraphicsCommandList4* command_list,
        const D3D12_RAYTRACING_INSTANCE_DESC* instances, UINT instance_count,
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags =
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE) noexcept;
    ID3D12Resource* Result() const noexcept { return result_.Get(); }
    D3D12_GPU_VIRTUAL_ADDRESS Address() const noexcept { return result_ ? result_->GetGPUVirtualAddress() : 0; }
    void Reset() noexcept { result_.Reset(); scratch_.Reset(); }
private:
    HRESULT AllocateBuffer(ID3D12Device* device, UINT64 size, D3D12_RESOURCE_STATES state,
                           Microsoft::WRL::ComPtr<ID3D12Resource>& output) noexcept;
    Microsoft::WRL::ComPtr<ID3D12Resource> result_;
    Microsoft::WRL::ComPtr<ID3D12Resource> scratch_;
    Microsoft::WRL::ComPtr<ID3D12Resource> instance_upload_;
};
}
