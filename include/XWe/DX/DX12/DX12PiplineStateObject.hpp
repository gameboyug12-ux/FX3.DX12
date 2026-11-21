#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <string>

namespace XWe::DX::DX12 {
class DX12PiplineStateObject final {
public:
    HRESULT CreateGraphics(ID3D12Device* device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc) noexcept;
    HRESULT CreateCompute(ID3D12Device* device, const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc) noexcept;
    HRESULT CreateStream(ID3D12Device2* device, const D3D12_PIPELINE_STATE_STREAM_DESC& desc) noexcept;
    ID3D12PipelineState* Get() const noexcept { return state_.Get(); }
    void Reset() noexcept { state_.Reset(); }
    explicit operator bool() const noexcept { return state_ != nullptr; }
private:
    Microsoft::WRL::ComPtr<ID3D12PipelineState> state_;
};
using DX12PipelineStateObject = DX12PiplineStateObject;
}
