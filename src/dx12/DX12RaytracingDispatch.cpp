#include "XWe/DX/DX12/DX12RaytracingDispatch.hpp"
namespace XWe::DX::DX12 {
HRESULT DX12RaytracingDispatch::Dispatch(ID3D12GraphicsCommandList4* list,
    ID3D12StateObject* pipeline, ID3D12RootSignature* root,
    ID3D12DescriptorHeap* heap, D3D12_GPU_DESCRIPTOR_HANDLE scene_srv,
    D3D12_GPU_DESCRIPTOR_HANDLE output_uav,
    const D3D12_DISPATCH_RAYS_DESC& desc) const noexcept {
    if(!list || !pipeline || !root || !heap || heap->GetDesc().Type!=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ||
       !(heap->GetDesc().Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) ||
       !scene_srv.ptr || !output_uav.ptr || !desc.RayGenerationShaderRecord.StartAddress ||
       !desc.MissShaderTable.StartAddress || !desc.HitGroupTable.StartAddress ||
       !desc.MissShaderTable.StrideInBytes || !desc.HitGroupTable.StrideInBytes ||
       !desc.Width || !desc.Height || !desc.Depth) return E_INVALIDARG;
    ID3D12DescriptorHeap* heaps[]={heap};
    list->SetDescriptorHeaps(1,heaps);
    list->SetComputeRootSignature(root);
    list->SetComputeRootDescriptorTable(0,scene_srv);
    list->SetComputeRootDescriptorTable(1,output_uav);
    list->SetPipelineState1(pipeline);
    list->DispatchRays(&desc);
    return S_OK;
}
}
