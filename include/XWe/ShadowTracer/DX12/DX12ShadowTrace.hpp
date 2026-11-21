#pragma once

#include "XWe/DX/DX12/DX12RaytracingDispatch.hpp"

namespace XWe::DX::DX12 {

// Non-owning inputs required to record one DXR shadow pass.
// The caller owns the pipeline, descriptor heap, resources and command list.
struct DX12ShadowTraceDesc final {
    ID3D12GraphicsCommandList4* command_list = nullptr;
    ID3D12StateObject* pipeline = nullptr;
    ID3D12RootSignature* global_root_signature = nullptr;
    ID3D12DescriptorHeap* shader_visible_heap = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE scene_srv_table{};
    D3D12_GPU_DESCRIPTOR_HANDLE shadow_output_uav_table{};
    D3D12_DISPATCH_RAYS_DESC dispatch{};
};

class DX12ShadowTrace final {
public:
    // Records a DXR shadow dispatch. Resource transitions and synchronization
    // remain the responsibility of the render graph / caller.
    HRESULT Record(const DX12ShadowTraceDesc& description) const noexcept {
        return dispatcher_.Dispatch(
            description.command_list,
            description.pipeline,
            description.global_root_signature,
            description.shader_visible_heap,
            description.scene_srv_table,
            description.shadow_output_uav_table,
            description.dispatch);
    }

private:
    DX12RaytracingDispatch dispatcher_;
};

} // namespace XWe::DX::DX12
