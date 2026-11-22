#pragma once
#include "DX12Ctx.hpp"
#include "DX12Texture.hpp"
#include <array>

namespace XWe::DX::DX12 {
enum class DX12GBufferSlot : unsigned { Albedo, Normal, Material, Velocity, Emissive, Count };
class DX12GBuffer final {
public:
    HRESULT Create(DX12Ctx& context, UINT width, UINT height) noexcept;
    HRESULT CreateViews(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE rtv_start,
        UINT rtv_increment, D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle) const noexcept;
    HRESULT CreateShaderViews(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE srv_start,
        UINT srv_increment) const noexcept;
    HRESULT TransitionTargets(ID3D12GraphicsCommandList* command_list,
        D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) const noexcept;
    HRESULT TransitionDepth(ID3D12GraphicsCommandList* command_list,
        D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) const noexcept;
    void Reset() noexcept;
    DX12Texture* Get(DX12GBufferSlot slot) noexcept;
    const DX12Texture* Get(DX12GBufferSlot slot) const noexcept;
    DX12Texture* Depth() noexcept { return &depth_; }
    const DX12Texture* Depth() const noexcept { return &depth_; }
    UINT Width() const noexcept { return width_; }
    UINT Height() const noexcept { return height_; }
private:
    std::array<DX12Texture, static_cast<unsigned>(DX12GBufferSlot::Count)> targets_;
    DX12Texture depth_;
    UINT width_ = 0, height_ = 0;
};
}
