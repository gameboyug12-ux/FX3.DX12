#include "XWe/DX/DX12/DX12Ctx.hpp"
namespace XWe::DX::DX12 {
DX12Ctx::~DX12Ctx() {
    if (fence_ && fence_event_) WaitForPreviousSubmission();
    if (fence_event_) CloseHandle(fence_event_);
}
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
    hr = command_list_->Close(); if (FAILED(hr)) return hr;
    hr = device_->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence_));
    if (FAILED(hr)) return hr;
    fence_event_=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    return fence_event_ ? S_OK : HRESULT_FROM_WIN32(GetLastError());
}
HRESULT DX12Ctx::WaitForPreviousSubmission() noexcept {
    if(!fence_ || !fence_event_) return E_UNEXPECTED;
    if(fence_->GetCompletedValue()>=fence_value_) return S_OK;
    HRESULT hr=fence_->SetEventOnCompletion(fence_value_,fence_event_);
    if(FAILED(hr)) return hr;
    return WaitForSingleObject(fence_event_,INFINITE)==WAIT_OBJECT_0?S_OK:E_FAIL;
}
HRESULT DX12Ctx::ExecuteAndSignal() noexcept {
    if(!command_list_ || !queue_ || !fence_) return E_UNEXPECTED;
    HRESULT hr=command_list_->Close(); if(FAILED(hr)) return hr;
    ID3D12CommandList* lists[]={command_list_.Get()};
    queue_->ExecuteCommandLists(1,lists);
    return queue_->Signal(fence_.Get(),++fence_value_);
}
}
