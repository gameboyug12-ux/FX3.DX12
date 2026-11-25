#pragma once
#include <d3d12.h>
#include <array>
#include <cstdint>

namespace XWe::DX::DX12 {
struct DX12RendererStateDesc {
    std::array<DXGI_FORMAT, 8> render_target_formats{};
    UINT render_target_count = 1;
    DXGI_FORMAT depth_format = DXGI_FORMAT_D32_FLOAT;
    DXGI_SAMPLE_DESC sample_desc{1, 0};
    D3D12_VIEWPORT viewport{};
    D3D12_RECT scissor{};
};
class DX12RendererStateObject final {
public:
    HRESULT Configure(const DX12RendererStateDesc& desc) noexcept;
    HRESULT Apply(ID3D12GraphicsCommandList* command_list) const noexcept;
    const DX12RendererStateDesc& Description() const noexcept { return desc_; }
    bool IsConfigured() const noexcept { return configured_; }
private:
    DX12RendererStateDesc desc_{};
    bool configured_ = false;
};
}
