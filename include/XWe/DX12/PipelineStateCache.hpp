#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <cstddef>
#include <string>
#include <unordered_map>

namespace XWe::DX12 {

/**
 * Device-scoped pipeline-state cache.
 *
 * Keys are stable application-defined identifiers. A key must uniquely encode
 * every shader, root signature, format, sample, blend, rasterizer and depth
 * state field that affects the descriptor. Use a new key when any field changes.
 */
class PipelineStateCache final {
public:
    ID3D12PipelineState* GetOrCreateGraphics(
        ID3D12Device* device, const std::string& stable_key,
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC& description);

    ID3D12PipelineState* GetOrCreateCompute(
        ID3D12Device* device, const std::string& stable_key,
        const D3D12_COMPUTE_PIPELINE_STATE_DESC& description);

    void Clear() noexcept;
    std::size_t GraphicsSize() const noexcept { return graphics_.size(); }
    std::size_t ComputeSize() const noexcept { return compute_.size(); }
    std::size_t Size() const noexcept { return graphics_.size() + compute_.size(); }

private:
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> graphics_;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> compute_;
};

} // namespace XWe::DX12
