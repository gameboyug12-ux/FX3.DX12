#include "XWe/DX/DX12/DX12GBuffer.hpp"
#include <utility>
namespace XWe::DX::DX12 {
HRESULT DX12GBuffer::Create(DX12Ctx& context,UINT width,UINT height) noexcept {
    if(!context.Device() || !width || !height) return E_INVALIDARG;
    const DXGI_FORMAT formats[]={DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R16G16B16A16_FLOAT,
        DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R16G16_FLOAT,DXGI_FORMAT_R16G16B16A16_FLOAT};
    std::array<DX12Texture,static_cast<unsigned>(DX12GBufferSlot::Count)> created;
    for(unsigned i=0;i<created.size();++i) {
        HRESULT hr=created[i].Create2D(context.Device(),width,height,formats[i],
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,D3D12_RESOURCE_STATE_RENDER_TARGET);
        if(FAILED(hr)) return hr;
    }
    DX12Texture depth;
    HRESULT hr=depth.Create2D(context.Device(),width,height,DXGI_FORMAT_D32_FLOAT,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,D3D12_RESOURCE_STATE_DEPTH_WRITE);
    if(FAILED(hr)) return hr;
    targets_=std::move(created); depth_=std::move(depth); width_=width; height_=height;
    return S_OK;
}
HRESULT DX12GBuffer::CreateViews(ID3D12Device* device,D3D12_CPU_DESCRIPTOR_HANDLE rtv,
    UINT increment,D3D12_CPU_DESCRIPTOR_HANDLE dsv) const noexcept {
    if(!device || !increment || !width_ || !height_) return E_INVALIDARG;
    for(const auto& target:targets_) {
        if(!target) return E_UNEXPECTED;
        target.CreateRTV(device,rtv); rtv.ptr+=increment;
    }
    if(!depth_) return E_UNEXPECTED;
    depth_.CreateDSV(device,dsv); return S_OK;
}
HRESULT DX12GBuffer::TransitionTargets(ID3D12GraphicsCommandList* list,
    D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after) const noexcept {
    if(!list || !width_ || !height_) return E_INVALIDARG;
    for(const auto& target:targets_) {
        HRESULT hr=target.Transition(list,before,after);
        if(FAILED(hr)) return hr;
    }
    return S_OK;
}
HRESULT DX12GBuffer::TransitionDepth(ID3D12GraphicsCommandList* list,
    D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after) const noexcept {
    if(!list || !depth_) return E_INVALIDARG;
    return depth_.Transition(list,before,after);
}
HRESULT DX12GBuffer::CreateShaderViews(ID3D12Device* device,D3D12_CPU_DESCRIPTOR_HANDLE srv,
    UINT increment) const noexcept {
    if(!device || !increment || !width_ || !height_) return E_INVALIDARG;
    for(const auto& target:targets_) {
        if(!target) return E_UNEXPECTED;
        D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
        desc.Format=target.Description().Format; desc.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
        desc.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        desc.Texture2D.MipLevels=1; target.CreateSRV(device,srv,&desc); srv.ptr+=increment;
    }
    if(!depth_) return E_UNEXPECTED;
    D3D12_SHADER_RESOURCE_VIEW_DESC depth_srv{};
    depth_srv.Format=DXGI_FORMAT_R32_FLOAT;
    depth_srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
    depth_srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    depth_srv.Texture2D.MipLevels=1;
    depth_.CreateSRV(device,srv,&depth_srv);
    return S_OK;
}
void DX12GBuffer::Reset() noexcept {
    for(auto& target:targets_) target.Reset(); depth_.Reset(); width_=height_=0;
}
DX12Texture* DX12GBuffer::Get(DX12GBufferSlot slot) noexcept {
    const auto i=static_cast<unsigned>(slot); return i<targets_.size()?&targets_[i]:nullptr;
}
const DX12Texture* DX12GBuffer::Get(DX12GBufferSlot slot) const noexcept {
    const auto i=static_cast<unsigned>(slot); return i<targets_.size()?&targets_[i]:nullptr;
}
}
