#include "XWe/DX/DX12/DX12ShaderBindingTable.hpp"
#include <algorithm>
#include <cstring>

namespace XWe::DX::DX12 {
HRESULT DX12ShaderBindingTable::Create(ID3D12Device* device,ID3D12StateObjectProperties* props,
    const wchar_t* raygen,const wchar_t* miss,const wchar_t* hit,UINT requested_stride) noexcept {
    if(!device || !props || !raygen || !miss || !hit) return E_INVALIDARG;
    const UINT stride=(std::max)(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES,
        (requested_stride + D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT-1) &
        ~(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT-1));
    const UINT section=(stride + D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT-1) &
        ~(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT-1);
    const UINT64 total=static_cast<UINT64>(section)*3;
    D3D12_HEAP_PROPERTIES heap{}; heap.Type=D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC desc{}; desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width=total; desc.Height=1; desc.DepthOrArraySize=1; desc.MipLevels=1;
    desc.SampleDesc.Count=1; desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
    HRESULT hr=device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&buffer));
    if(FAILED(hr)) return hr;
    const void* ids[]={props->GetShaderIdentifier(raygen),props->GetShaderIdentifier(miss),
                       props->GetShaderIdentifier(hit)};
    if(!ids[0] || !ids[1] || !ids[2]) return E_INVALIDARG;
    std::byte* mapped=nullptr; D3D12_RANGE read_range{0,0};
    hr=buffer->Map(0,&read_range,reinterpret_cast<void**>(&mapped)); if(FAILED(hr)) return hr;
    for(UINT i=0;i<3;++i) std::memcpy(mapped+static_cast<std::size_t>(i)*section,ids[i],
        D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    buffer->Unmap(0,nullptr);
    const auto base=buffer->GetGPUVirtualAddress();
    raygen_={base,stride};
    miss_={base+section,stride,stride};
    hit_={base+static_cast<UINT64>(section)*2,stride,stride};
    buffer_=std::move(buffer); return S_OK;
}
const D3D12_DISPATCH_RAYS_DESC& DX12ShaderBindingTable::DispatchDescription(
    UINT width,UINT height,UINT depth) const noexcept {
    dispatch_={}; dispatch_.RayGenerationShaderRecord=raygen_;
    dispatch_.MissShaderTable=miss_; dispatch_.HitGroupTable=hit_;
    dispatch_.Width=width; dispatch_.Height=height; dispatch_.Depth=depth;
    return dispatch_;
}
}
