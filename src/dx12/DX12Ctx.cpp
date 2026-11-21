#include "XWe/DX/DX12/DX12Ctx.hpp"
namespace XWe::DX::DX12 {
HRESULT DX12Ctx::Initialize(ID3D12Device* device, ID3D12CommandQueue* queue) noexcept {
    if (!device || !queue) return E_INVALIDARG;
    device_ = device; queue_ = queue; return S_OK;
}
HRESULT DX12Ctx::CreateCommandObjects(D3D12_COMMAND_LIST_TYPE type) noexcept {
    if (!IsInitialized()) return E_UNEXPECTED;
    allocator_.Reset(); command_list_.Reset();
    HRESULT hr = device_->CreateCommandAllocator(type, IID_PPV_ARGS(&allocator_));
    if (FAILED(hr)) return hr;
    hr = device_->CreateCommandList(0, type, allocator_.Get(), nullptr, IID_PPV_ARGS(&command_list_));
    if (FAILED(hr)) { allocator_.Reset(); return hr; }
    return command_list_->Close();
}
}
