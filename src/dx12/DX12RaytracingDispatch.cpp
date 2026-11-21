#include "XWe/DX/DX12/DX12RaytracingDispatch.hpp"
namespace XWe::DX::DX12 {
HRESULT DX12RaytracingDispatch::Dispatch(ID3D12GraphicsCommandList4* list,
    ID3D12StateObject* pipeline,const D3D12_DISPATCH_RAYS_DESC& desc) const noexcept {
    if(!list || !pipeline || !desc.RayGenerationShaderRecord.StartAddress ||
       !desc.Width || !desc.Height || !desc.Depth) return E_INVALIDARG;
    list->SetPipelineState1(pipeline);
    list->DispatchRays(&desc);
    return S_OK;
}
}
