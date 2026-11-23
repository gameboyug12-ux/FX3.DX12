#include "XWe/DX/DX12/DX12Ctx.hpp"
#include <dxgi.h>
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
    if(queue_->GetDesc().Type!=type) return E_INVALIDARG;
    if(fence_ && fence_event_) {
        HRESULT wait_hr=WaitForPreviousSubmission();
        if(FAILED(wait_hr)) return wait_hr;
    }
    command_list4_.Reset(); allocator_.Reset(); command_list_.Reset();
    if(fence_event_) { CloseHandle(fence_event_); fence_event_=nullptr; }
    fence_.Reset(); fence_value_=0;
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
    const UINT64 completed=fence_->GetCompletedValue();
    if(completed==UINT64_MAX) return DXGI_ERROR_DEVICE_REMOVED;
    if(completed>=fence_value_) return S_OK;
    HRESULT hr=fence_->SetEventOnCompletion(fence_value_,fence_event_);
    if(FAILED(hr)) return hr;
    return WaitForSingleObject(fence_event_,INFINITE)==WAIT_OBJECT_0?S_OK:E_FAIL;
}
HRESULT DX12Ctx::QueryRaytracingCommandList() noexcept {
    if(!command_list_) return E_UNEXPECTED;
    command_list4_.Reset();
    return command_list_.As(&command_list4_);
}
HRESULT DX12Ctx::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type,UINT count,
    D3D12_DESCRIPTOR_HEAP_FLAGS flags,ID3D12DescriptorHeap** output) noexcept {
    if(!device_ || !output || !count) return E_INVALIDARG;
    *output=nullptr;
    D3D12_DESCRIPTOR_HEAP_DESC desc{}; desc.Type=type; desc.NumDescriptors=count; desc.Flags=flags;
    return device_->CreateDescriptorHeap(&desc,IID_PPV_ARGS(output));
}
UINT DX12Ctx::DescriptorIncrement(D3D12_DESCRIPTOR_HEAP_TYPE type) const noexcept {
    return device_?device_->GetDescriptorHandleIncrementSize(type):0;
}
HRESULT DX12Ctx::ExecuteAndSignal() noexcept {
    if(!command_list_ || !queue_ || !fence_) return E_UNEXPECTED;
    HRESULT hr=command_list_->Close(); if(FAILED(hr)) return hr;
    ID3D12CommandList* lists[]={command_list_.Get()};
    queue_->ExecuteCommandLists(1,lists);
    const UINT64 next_value=fence_value_+1;
    HRESULT signal_hr=queue_->Signal(fence_.Get(),next_value);
    if(SUCCEEDED(signal_hr)) fence_value_=next_value;
    return signal_hr;
}
}
