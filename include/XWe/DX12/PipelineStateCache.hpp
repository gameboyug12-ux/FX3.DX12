#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <cstddef>
#include <string>
#include <unordered_map>

namespace XWe::DX12 {

/**
 * Device-scoped D3D12 PSO cache.
 *
 * Stable keys are application-defined and must encode every state/shader field
 * that affects the PSO. Use a fresh key whenever any descriptor changes.
 * The cache is intended to be owned by one D3D12 device/render device.
 */
class PipelineStateCache final {
public:
    ID3D12PipelineState* GetOrCreateGraphics(
        ID3D12Device* device, const std::string& stable_key,
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC& description);

    ID3D12PipelineState* GetOrCreateCompute(
        ID3D12Device* device, const std::string& stable_key,
        const D3D12_COMPUTE_PIPELINE_STATE_DESC& description);

    /** Supports pipeline-state streams (including advanced/mesh PSO subobjects). */
    ID3D12PipelineState* GetOrCreateStream(
        ID3D12Device2* device, const std::string& stable_key,
        const D3D12_PIPELINE_STATE_STREAM_DESC& description);

    void Clear() noexcept;
    std::size_t GraphicsSize() const noexcept { return graphics_.size(); }
    std::size_t ComputeSize() const noexcept { return compute_.size(); }
    std::size_t StreamSize() const noexcept { return streams_.size(); }
    std::size_t Size() const noexcept {
        return graphics_.size() + compute_.size() + streams_.size();
    }

private:
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> graphics_;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> compute_;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> streams_;
};

} // namespace XWe::DX12
