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
    return context_.CommandList()->Reset(context_.Allocator(),pipeline_.Get());
}
HRESULT DX12RendererApi::EndFrame() noexcept { return context_.ExecuteAndSignal(); }
HRESULT DX12RendererApi::CreateGraphicsPipeline(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc) noexcept {
    return pipeline_.CreateGraphics(context_.Device(),desc);
}
HRESULT DX12RendererApi::InitializeRaytracing() noexcept {
    return raytracing_.Initialize(context_.Device());
}
HRESULT DX12RendererApi::CreateRaytracingPipeline(const std::wstring& path) noexcept {
    return raytracing_.CreatePipeline(path);
}
}
