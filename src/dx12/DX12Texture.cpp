#include "XWe/DX/DX12/DX12Texture.hpp"
#include <utility>
#include <utility>
namespace XWe::DX::DX12 {
HRESULT DX12Texture::Create2D(ID3D12Device* device,UINT width,UINT height,DXGI_FORMAT format,
    D3D12_RESOURCE_FLAGS flags,D3D12_RESOURCE_STATES initial_state) noexcept {
    if(!device || !width || !height || format==DXGI_FORMAT_UNKNOWN) return E_INVALIDARG;
    D3D12_RESOURCE_DESC desc{}; desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width=width; desc.Height=height; desc.DepthOrArraySize=1; desc.MipLevels=1;
    desc.Format=format; desc.SampleDesc.Count=1; desc.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN; desc.Flags=flags;
    D3D12_HEAP_PROPERTIES heap{}; heap.Type=D3D12_HEAP_TYPE_DEFAULT;
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    HRESULT hr=device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,initial_state,
        nullptr,IID_PPV_ARGS(&resource));
    if(SUCCEEDED(hr)){resource_=std::move(resource);desc_=desc;} return hr;
}
void DX12Texture::CreateSRV(ID3D12Device* d,D3D12_CPU_DESCRIPTOR_HANDLE h,const D3D12_SHADER_RESOURCE_VIEW_DESC* v) const noexcept {
    if(d && resource_) d->CreateShaderResourceView(resource_.Get(),v,h);
}
void DX12Texture::CreateRTV(ID3D12Device* d,D3D12_CPU_DESCRIPTOR_HANDLE h,const D3D12_RENDER_TARGET_VIEW_DESC* v) const noexcept {
    if(d && resource_) d->CreateRenderTargetView(resource_.Get(),v,h);
}
void DX12Texture::CreateDSV(ID3D12Device* d,D3D12_CPU_DESCRIPTOR_HANDLE h,const D3D12_DEPTH_STENCIL_VIEW_DESC* v) const noexcept {
    if(d && resource_) d->CreateDepthStencilView(resource_.Get(),v,h);
}
void DX12Texture::CreateUAV(ID3D12Device* d,D3D12_CPU_DESCRIPTOR_HANDLE h,const D3D12_UNORDERED_ACCESS_VIEW_DESC* v) const noexcept {
    if(d && resource_) d->CreateUnorderedAccessView(resource_.Get(),nullptr,v,h);
}
HRESULT DX12Texture::Transition(ID3D12GraphicsCommandList* list,D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after) const noexcept {
    if(!list || !resource_) return E_INVALIDARG;
    if(before==after) return S_FALSE;
    D3D12_RESOURCE_BARRIER barrier{}; barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource=resource_.Get();
    barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore=before; barrier.Transition.StateAfter=after;
    list->ResourceBarrier(1,&barrier); return S_OK;
}
}
