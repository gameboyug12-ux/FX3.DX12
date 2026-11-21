#include "XWe/DX/DX12/DX12Texture.hpp"
namespace XWe::DX::DX12 {
HRESULT DX12Texture::Create2D(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format,
    D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initial_state) noexcept {
    if (!device || !width || !height || format == DXGI_FORMAT_UNKNOWN) return E_INVALIDARG;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width; desc.Height = height; desc.DepthOrArraySize = 1; desc.MipLevels = 1;
    desc.Format = format; desc.SampleDesc.Count = 1; desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = flags;
    D3D12_HEAP_PROPERTIES heap{}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    HRESULT hr = device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,initial_state,
        nullptr,IID_PPV_ARGS(&resource));
    if (SUCCEEDED(hr)) { resource_ = std::move(resource); desc_ = desc; }
    return hr;
}
}
