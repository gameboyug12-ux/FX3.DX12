#include "XWe/DX/DX12/DX12AccelerationStructure.hpp"
#include <cstring>
#include <cstddef>

namespace XWe::DX::DX12 {
HRESULT DX12AccelerationStructure::AllocateBuffer(ID3D12Device* device, UINT64 size,
    D3D12_RESOURCE_STATES state, Microsoft::WRL::ComPtr<ID3D12Resource>& output) noexcept {
    if (!device || !size) return E_INVALIDARG;
    D3D12_HEAP_PROPERTIES heap{}; heap.Type=D3D12_HEAP_TYPE_DEFAULT; heap.CreationNodeMask=1; heap.VisibleNodeMask=1;
    D3D12_RESOURCE_DESC desc{}; desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width=size; desc.Height=1; desc.DepthOrArraySize=1; desc.MipLevels=1;
    desc.SampleDesc.Count=1; desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    return device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,state,nullptr,IID_PPV_ARGS(&output));
}
HRESULT DX12AccelerationStructure::BuildBottomLevel(ID3D12Device5* device,
    ID3D12GraphicsCommandList4* list,const D3D12_RAYTRACING_GEOMETRY_DESC* geometries,
    UINT count,D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags) noexcept {
    if(!device || !list || !geometries || !count) return E_INVALIDARG;
    result_.Reset(); scratch_.Reset();
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
    inputs.Type=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    inputs.DescsLayout=D3D12_ELEMENTS_LAYOUT_ARRAY; inputs.NumDescs=count;
    inputs.pGeometryDescs=geometries; inputs.Flags=flags;
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};
    device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs,&info);
    if(!info.ResultDataMaxSizeInBytes || !info.ScratchDataSizeInBytes) return E_FAIL;
    HRESULT hr=AllocateBuffer(device,info.ResultDataMaxSizeInBytes,D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,result_);
    if(FAILED(hr)) return hr;
    hr=AllocateBuffer(device,info.ScratchDataSizeInBytes,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,scratch_);
    if(FAILED(hr)) { result_.Reset(); return hr; }
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build{};
    build.Inputs=inputs; build.DestAccelerationStructureData=result_->GetGPUVirtualAddress();
    build.ScratchAccelerationStructureData=scratch_->GetGPUVirtualAddress();
    list->BuildRaytracingAccelerationStructure(&build,0,nullptr);
    D3D12_RESOURCE_BARRIER barrier{}; barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource=result_.Get(); list->ResourceBarrier(1,&barrier);
    return S_OK;
}
HRESULT DX12AccelerationStructure::BuildTopLevel(ID3D12Device5* device,
    ID3D12GraphicsCommandList4* list,const D3D12_RAYTRACING_INSTANCE_DESC* instances,
    UINT count,D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags) noexcept {
    if(!device || !list || !instances || !count) return E_INVALIDARG;
    result_.Reset(); scratch_.Reset(); instance_upload_.Reset();
    const UINT64 bytes=static_cast<UINT64>(sizeof(D3D12_RAYTRACING_INSTANCE_DESC))*count;
    D3D12_HEAP_PROPERTIES upload{}; upload.Type=D3D12_HEAP_TYPE_UPLOAD; upload.CreationNodeMask=1; upload.VisibleNodeMask=1;
    D3D12_RESOURCE_DESC buffer{}; buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width=bytes; buffer.Height=1; buffer.DepthOrArraySize=1; buffer.MipLevels=1;
    buffer.SampleDesc.Count=1; buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    instance_upload_.Reset();
    HRESULT hr=device->CreateCommittedResource(&upload,D3D12_HEAP_FLAG_NONE,&buffer,
        D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&instance_upload_));
    if(FAILED(hr)) return hr;
    void* mapped=nullptr; D3D12_RANGE read_range{0,0};
    hr=instance_upload_->Map(0,&read_range,&mapped); if(FAILED(hr)) return hr;
    std::memcpy(mapped,instances,static_cast<std::size_t>(bytes)); instance_upload_->Unmap(0,nullptr);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
    inputs.Type=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    inputs.DescsLayout=D3D12_ELEMENTS_LAYOUT_ARRAY; inputs.NumDescs=count;
    inputs.InstanceDescs=instance_upload_->GetGPUVirtualAddress(); inputs.Flags=flags;
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};
    device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs,&info);
    if(!info.ResultDataMaxSizeInBytes || !info.ScratchDataSizeInBytes) return E_FAIL;
    hr=AllocateBuffer(device,info.ResultDataMaxSizeInBytes,D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,result_);
    if(FAILED(hr)) return hr;
    hr=AllocateBuffer(device,info.ScratchDataSizeInBytes,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,scratch_);
    if(FAILED(hr)) { result_.Reset(); return hr; }
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build{};
    build.Inputs=inputs; build.DestAccelerationStructureData=result_->GetGPUVirtualAddress();
    build.ScratchAccelerationStructureData=scratch_->GetGPUVirtualAddress();
    list->BuildRaytracingAccelerationStructure(&build,0,nullptr);
    D3D12_RESOURCE_BARRIER barrier{}; barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource=result_.Get(); list->ResourceBarrier(1,&barrier);
    return S_OK;
}
}
