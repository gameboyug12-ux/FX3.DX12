#pragma once
#include "DX12Ctx.hpp"
#include "DX12RendererStateObject.hpp"
#include "DX12PiplineStateObject.hpp"

namespace XWe::DX::DX12 {
class DX12RendererApi final {
public:
    HRESULT Initialize(ID3D12Device* device, ID3D12CommandQueue* queue) noexcept;
    HRESULT BeginFrame() noexcept;
    HRESULT EndFrame() noexcept;
    DX12Ctx& Context() noexcept { return context_; }
    const DX12Ctx& Context() const noexcept { return context_; }
    DX12RendererStateObject& State() noexcept { return state_; }
private:
    DX12Ctx context_;
    DX12RendererStateObject state_;
};
}
