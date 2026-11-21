#include "XWe/DX/DX12/DX12PiplineStateObject.hpp"
namespace XWe::DX::DX12 {
HRESULT DX12PiplineStateObject::CreateGraphics(ID3D12Device* d,const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc) noexcept {
    if(!d) return E_INVALIDARG; state_.Reset(); return d->CreateGraphicsPipelineState(&desc,IID_PPV_ARGS(&state_));
}
HRESULT DX12PiplineStateObject::CreateCompute(ID3D12Device* d,const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc) noexcept {
    if(!d) return E_INVALIDARG; state_.Reset(); return d->CreateComputePipelineState(&desc,IID_PPV_ARGS(&state_));
}
HRESULT DX12PiplineStateObject::CreateStream(ID3D12Device2* d,const D3D12_PIPELINE_STATE_STREAM_DESC& desc) noexcept {
    if(!d) return E_INVALIDARG; state_.Reset(); return d->CreatePipelineState(&desc,IID_PPV_ARGS(&state_));
}
}
