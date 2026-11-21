#pragma once
#include <d3d12.h>

namespace XWe::DX::DX12 {
class DX12RaytracingDispatch final {
public:
    HRESULT Dispatch(ID3D12GraphicsCommandList4* command_list, ID3D12StateObject* pipeline,
                     const D3D12_DISPATCH_RAYS_DESC& description) const noexcept;
};
}
