#pragma once
#include <d3d12.h>

namespace XWe::DX::DX12 {
class DX12RaytracingDispatch final {
public:
    // scene_srv_table is the first descriptor in the contiguous t0-t4 SRV table;
    // output_uav_table is the first descriptor in the u0 UAV table. Both tables
    // must be resident in shader_visible_heap and match the active pipeline layout.
    HRESULT Dispatch(ID3D12GraphicsCommandList4* command_list,
        ID3D12StateObject* pipeline, ID3D12RootSignature* global_root_signature,
        ID3D12DescriptorHeap* shader_visible_heap,
        D3D12_GPU_DESCRIPTOR_HANDLE scene_srv_table,
        D3D12_GPU_DESCRIPTOR_HANDLE output_uav_table,
        const D3D12_DISPATCH_RAYS_DESC& description) const noexcept;
};
}
