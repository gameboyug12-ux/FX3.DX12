#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>

namespace XWe::DX::DX12 {
class DX12Ctx final {
public:
    HRESULT Initialize(ID3D12Device* device, ID3D12CommandQueue* queue) noexcept;
    HRESULT CreateCommandObjects(D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT) noexcept;
    ID3D12Device* Device() const noexcept { return device_.Get(); }
    ID3D12CommandQueue* Queue() const noexcept { return queue_.Get(); }
    ID3D12CommandAllocator* Allocator() const noexcept { return allocator_.Get(); }
    ID3D12GraphicsCommandList* CommandList() const noexcept { return command_list_.Get(); }
    bool IsInitialized() const noexcept { return device_ && queue_; }
private:
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> command_list_;
};
}
