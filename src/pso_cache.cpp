#include "XWe/DX12/PipelineStateCache.hpp"

#include <stdexcept>
#include <utility>

namespace XWe::DX12 {
ID3D12PipelineState* PipelineStateCache::GetOrCreateGraphics(
    ID3D12Device* device, const std::string& key,
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc) {
    if (!device || key.empty())
        throw std::invalid_argument("Graphics PSO cache requires a device and stable key");
    if (const auto found = graphics_.find(key); found != graphics_.end())
        return found->second.Get();
    Microsoft::WRL::ComPtr<ID3D12PipelineState> state;
    const HRESULT hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&state));
    if (FAILED(hr)) throw std::runtime_error("CreateGraphicsPipelineState failed for PSO key: " + key);
    auto* result = state.Get();
    graphics_.emplace(key, std::move(state));
    return result;
}
ID3D12PipelineState* PipelineStateCache::GetOrCreateCompute(
    ID3D12Device* device, const std::string& key,
    const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc) {
    if (!device || key.empty())
        throw std::invalid_argument("Compute PSO cache requires a device and stable key");
    if (const auto found = compute_.find(key); found != compute_.end())
        return found->second.Get();
    Microsoft::WRL::ComPtr<ID3D12PipelineState> state;
    const HRESULT hr = device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&state));
    if (FAILED(hr)) throw std::runtime_error("CreateComputePipelineState failed for PSO key: " + key);
    auto* result = state.Get();
    compute_.emplace(key, std::move(state));
    return result;
}
ID3D12PipelineState* PipelineStateCache::GetOrCreateStream(
    ID3D12Device2* device, const std::string& key,
    const D3D12_PIPELINE_STATE_STREAM_DESC& desc) {
    if (!device || key.empty())
        throw std::invalid_argument("Stream PSO cache requires a device and stable key");
    if (const auto found = streams_.find(key); found != streams_.end())
        return found->second.Get();
    Microsoft::WRL::ComPtr<ID3D12PipelineState> state;
    const HRESULT hr = device->CreatePipelineState(&desc, IID_PPV_ARGS(&state));
    if (FAILED(hr)) throw std::runtime_error("CreatePipelineState(stream) failed for PSO key: " + key);
    auto* result = state.Get();
    streams_.emplace(key, std::move(state));
    return result;
}
void PipelineStateCache::Clear() noexcept {
    graphics_.clear();
    compute_.clear();
    streams_.clear();
}

} // namespace XWe::DX12
