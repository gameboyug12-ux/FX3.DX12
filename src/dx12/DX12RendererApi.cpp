#include "XWe/DX/DX12/DX12RendererApi.hpp"
namespace XWe::DX::DX12 {
HRESULT DX12RendererApi::Initialize(ID3D12Device* device,ID3D12CommandQueue* queue) noexcept {
    HRESULT hr=context_.Initialize(device,queue); if(FAILED(hr)) return hr;
    return context_.CreateCommandObjects(D3D12_COMMAND_LIST_TYPE_DIRECT);
}
HRESULT DX12RendererApi::BeginFrame() noexcept {
    if(!context_.IsInitialized() || !context_.Allocator() || !context_.CommandList()) return E_UNEXPECTED;
    HRESULT hr=context_.WaitForPreviousSubmission(); if(FAILED(hr)) return hr;
    hr=context_.Allocator()->Reset(); if(FAILED(hr)) return hr;
    return context_.CommandList()->Reset(context_.Allocator(),graphics_pipeline_.Get());
}
HRESULT DX12RendererApi::EndFrame() noexcept { return context_.ExecuteAndSignal(); }
HRESULT DX12RendererApi::CreateGraphicsPipeline(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc) noexcept {
    return graphics_pipeline_.CreateGraphics(context_.Device(),desc);
}
HRESULT DX12RendererApi::CreateComputePipeline(const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc) noexcept {
    return compute_pipeline_.CreateCompute(context_.Device(),desc);
}
HRESULT DX12RendererApi::CreatePipelineStream(ID3D12Device2* device,const D3D12_PIPELINE_STATE_STREAM_DESC& desc) noexcept {
    return stream_pipeline_.CreateStream(device,desc);
}
HRESULT DX12RendererApi::ApplyRendererState() noexcept {
    return state_.Apply(context_.CommandList());
}
HRESULT DX12RendererApi::InitializeRaytracing() noexcept {
    return raytracing_.Initialize(context_.Device());
}
HRESULT DX12RendererApi::CreateRaytracingPipeline(const std::wstring& path) noexcept {
    return raytracing_.CreatePipeline(path);
}
}
