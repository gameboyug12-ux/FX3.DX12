#pragma once
#include "DX12Ctx.hpp"
#include "DX12Texture.hpp"
#include <array>

namespace XWe::DX::DX12 {
enum class DX12GBufferSlot : unsigned { Albedo, Normal, Material, Velocity, Emissive, Count };
class DX12GBuffer final {
public:
    HRESULT Create(DX12Ctx& context, UINT width, UINT height) noexcept;
    void Reset() noexcept;
    DX12Texture* Get(DX12GBufferSlot slot) noexcept;
    const DX12Texture* Get(DX12GBufferSlot slot) const noexcept;
    UINT Width() const noexcept { return width_; }
    UINT Height() const noexcept { return height_; }
private:
    std::array<DX12Texture, static_cast<unsigned>(DX12GBufferSlot::Count)> targets_;
    UINT width_ = 0, height_ = 0;
};
}
