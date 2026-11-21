#include "XWe/DX/DX12/DX12RendererApi.hpp"
namespace XWe::DX::DX12 {
HRESULT DX12RendererApi::Initialize(ID3D12Device* device, ID3D12CommandQueue* queue) noexcept {
    HRESULT hr = context_.Initialize(device, queue);
    if (FAILED(hr)) return hr;
    return context_.CreateCommandObjects(D3D12_COMMAND_LIST_TYPE_DIRECT);
}
HRESULT DX12RendererApi::BeginFrame() noexcept {
    if (!context_.IsInitialized() || !context_.Allocator() || !context_.CommandList()) return E_UNEXPECTED;
    HRESULT hr = context_.Allocator()->Reset();
    if (FAILED(hr)) return hr;
    return context_.CommandList()->Reset(context_.Allocator(), nullptr);
}
HRESULT DX12RendererApi::EndFrame() noexcept {
    auto* list = context_.CommandList();
    auto* queue = context_.Queue();
    if (!list || !queue) return E_UNEXPECTED;
    HRESULT hr = list->Close();
    if (FAILED(hr)) return hr;
    ID3D12CommandList* lists[] = {list};
    queue->ExecuteCommandLists(1, lists);
    return S_OK;
}
}
