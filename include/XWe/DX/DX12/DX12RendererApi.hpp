#pragma once
#include "DX12Ctx.hpp"
#include "DX12RendererStateObject.hpp"
#include "DX12PiplineStateObject.hpp"
#include "DX12RaytracingApi.hpp"
#include <filesystem>
#include <string>

namespace XWe::DX::DX12 {
class DX12RendererApi final {
public:
    HRESULT Initialize(ID3D12Device* device, ID3D12CommandQueue* queue) noexcept;
    HRESULT BeginFrame() noexcept;
    HRESULT EndFrame() noexcept;
    HRESULT CreateGraphicsPipeline(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& description) noexcept;
    HRESULT CreateComputePipeline(const D3D12_COMPUTE_PIPELINE_STATE_DESC& description) noexcept;
    HRESULT CreatePipelineStream(ID3D12Device2* device, const D3D12_PIPELINE_STATE_STREAM_DESC& description) noexcept;
    HRESULT ApplyRendererState() noexcept;
    HRESULT InitializeRaytracing() noexcept;
    HRESULT CreateRaytracingPipeline(const std::wstring& dxil_library_path) noexcept;
    DX12Ctx& Context() noexcept { return context_; }
    const DX12Ctx& Context() const noexcept { return context_; }
    DX12RendererStateObject& State() noexcept { return state_; }
    DX12PiplineStateObject& PipelineState() noexcept { return graphics_pipeline_; }
    DX12PiplineStateObject& ComputePipelineState() noexcept { return compute_pipeline_; }
    DX12PiplineStateObject& StreamPipelineState() noexcept { return stream_pipeline_; }
    DX12RaytracingApi& Raytracing() noexcept { return raytracing_; }
private:
    DX12Ctx context_;
    DX12RendererStateObject state_;
    DX12PiplineStateObject graphics_pipeline_;
    DX12PiplineStateObject compute_pipeline_;
    DX12PiplineStateObject stream_pipeline_;
    DX12RaytracingApi raytracing_;
};
}
