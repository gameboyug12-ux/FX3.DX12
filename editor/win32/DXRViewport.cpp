#include "DXRViewport.hpp"

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <array>
#include <cstring>
#include <stdexcept>
#include <string>
#include <cstddef>
#include <algorithm>

using Microsoft::WRL::ComPtr;

namespace {
void check(HRESULT hr, const char* what) {
    if (FAILED(hr)) throw std::runtime_error(std::string(what) + " failed (HRESULT " +
        std::to_string(static_cast<unsigned long>(hr)) + ")");
}
struct Vertex { float p[3]; };
constexpr UINT kFrameCount = 2;
constexpr UINT kDescriptorCount = 2;

ComPtr<ID3D12Resource> makeBuffer(ID3D12Device* device, UINT64 bytes,
    D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
    D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
    D3D12_HEAP_PROPERTIES heap{}; heap.Type = heapType;
    heap.CreationNodeMask = 1; heap.VisibleNodeMask = 1;
    D3D12_RESOURCE_DESC desc{}; desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = bytes; desc.Height = 1; desc.DepthOrArraySize = 1; desc.MipLevels = 1;
    desc.SampleDesc.Count = 1; desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; desc.Flags = flags;
    ComPtr<ID3D12Resource> resource;
    check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
        heapType == D3D12_HEAP_TYPE_UPLOAD ? D3D12_RESOURCE_STATE_GENERIC_READ :
        initialState, nullptr, IID_PPV_ARGS(&resource)), "Create buffer");
    return resource;
}
void upload(ID3D12Resource* resource, const void* data, size_t bytes) {
    void* mapped = nullptr; D3D12_RANGE read{0,0};
    check(resource->Map(0, &read, &mapped), "Map upload buffer");
    std::memcpy(mapped, data, bytes); resource->Unmap(0, nullptr);
}
std::vector<std::byte> readBinary(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("DXIL library not found: " + path.string());
    const auto n = file.tellg(); if (n <= 0) throw std::runtime_error("DXIL library is empty");
    std::vector<std::byte> bytes(static_cast<size_t>(n));
    file.seekg(0); file.read(reinterpret_cast<char*>(bytes.data()), n);
    if (!file) throw std::runtime_error("Failed to read DXIL library");
    return bytes;
}
}

class DXRViewport::Renderer final {
public:
    ~Renderer() { waitGpu(); if (fenceEvent_) CloseHandle(fenceEvent_); }
    void initialize(HWND hwnd, UINT width, UINT height) {
        if (ready_) return;
        UINT factoryFlags = 0;
#if defined(_DEBUG)
        ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
            debug->EnableDebugLayer(); factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
#endif
        check(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory_)), "CreateDXGIFactory2");
        ComPtr<IDXGIAdapter1> adapter;
        for (UINT i=0; factory_->EnumAdapters1(i,&adapter)!=DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 d{}; adapter->GetDesc1(&d);
            if (d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) { adapter.Reset(); continue; }
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,
                __uuidof(ID3D12Device),nullptr))) break;
            adapter.Reset();
        }
        if (!adapter) throw std::runtime_error("No D3D12 hardware adapter was found");
        check(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device_)),
              "D3D12CreateDevice");
        D3D12_FEATURE_DATA_D3D12_OPTIONS5 options{};
        check(device_->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,&options,sizeof(options)),
              "Check DXR support");
        if (options.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
            throw std::runtime_error("This GPU/driver does not support DirectX Raytracing (DXR).");

        D3D12_COMMAND_QUEUE_DESC q{}; q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
        check(device_->CreateCommandQueue(&q,IID_PPV_ARGS(&queue_)),"Create command queue");
        DXGI_SWAP_CHAIN_DESC1 sd{}; sd.Width=std::max(1u,width); sd.Height=std::max(1u,height);
        sd.Format=DXGI_FORMAT_R8G8B8A8_UNORM; sd.SampleDesc.Count=1; sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount=kFrameCount; sd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
        ComPtr<IDXGISwapChain1> sc;
        check(factory_->CreateSwapChainForHwnd(queue_.Get(),hwnd,&sd,nullptr,nullptr,&sc),"Create DXGI swap chain");
        check(sc.As(&swapchain_),"Query IDXGISwapChain3");
        factory_->MakeWindowAssociation(hwnd,DXGI_MWA_NO_ALT_ENTER);
        for (UINT i=0;i<kFrameCount;++i)
            check(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocators_[i])),
                  "Create command allocator");
        check(device_->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocators_[0].Get(),
              nullptr,IID_PPV_ARGS(&list_)),"Create command list");
        check(list_->Close(),"Close initial command list");
        check(device_->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence_)),"Create fence");
        fenceEvent_=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        if (!fenceEvent_) throw std::runtime_error("CreateEventW failed");

        D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        hd.NumDescriptors=kDescriptorCount; hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        check(device_->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap_)),"Create shader-visible descriptor heap");
        descriptorIncrement_=device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        createRaytracingPipeline();
        createSceneAccelerationStructures();
        createShaderBindingTable();
        resize(width,height);
        ready_=true;
    }
    void resize(UINT width, UINT height) {
        width=std::max(1u,width); height=std::max(1u,height);
        if (!swapchain_ || (width==width_ && height==height_)) return;
        waitGpu();
        for(auto& b:backbuffers_) b.Reset();
        output_.Reset();
        check(swapchain_->ResizeBuffers(kFrameCount,width,height,DXGI_FORMAT_R8G8B8A8_UNORM,0),
              "Resize swap chain");
        for(UINT i=0;i<kFrameCount;++i)
            check(swapchain_->GetBuffer(i,IID_PPV_ARGS(&backbuffers_[i])),"Get swap-chain buffer");
        D3D12_RESOURCE_DESC td{}; td.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        td.Width=width; td.Height=height; td.DepthOrArraySize=1; td.MipLevels=1;
        td.Format=DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count=1;
        td.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN; td.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        D3D12_HEAP_PROPERTIES hp{}; hp.Type=D3D12_HEAP_TYPE_DEFAULT; hp.CreationNodeMask=1; hp.VisibleNodeMask=1;
        check(device_->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&td,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&output_)),"Create DXR output texture");
        auto cpu=heap_->GetCPUDescriptorHandleForHeapStart(); cpu.ptr+=descriptorIncrement_;
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav{}; uav.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        uav.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;
        device_->CreateUnorderedAccessView(output_.Get(),nullptr,&uav,cpu);
        width_=width; height_=height;
    }
    void render() {
        if(!ready_ || !output_) return;
        const UINT frame=swapchain_->GetCurrentBackBufferIndex(); waitFrame(frame);
        check(allocators_[frame]->Reset(),"Reset frame allocator");
        check(list_->Reset(allocators_[frame].Get(),nullptr),"Reset command list");
        ID3D12DescriptorHeap* heaps[]={heap_.Get()}; list_->SetDescriptorHeaps(1,heaps);
        list_->SetComputeRootSignature(rootSignature_.Get());
        auto gpu=heap_->GetGPUDescriptorHandleForHeapStart();
        list_->SetComputeRootDescriptorTable(0,gpu);
        D3D12_GPU_DESCRIPTOR_HANDLE uav=gpu; uav.ptr+=descriptorIncrement_;
        list_->SetComputeRootDescriptorTable(1,uav);
        list_->SetPipelineState1(stateObject_.Get());
        D3D12_DISPATCH_RAYS_DESC dispatch{};
        dispatch.RayGenerationShaderRecord={sbt_->GetGPUVirtualAddress(),D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES};
        const UINT64 section= D3D12_SHADER_TABLE_BYTE_ALIGNMENT;
        dispatch.MissShaderTable={sbt_->GetGPUVirtualAddress()+section,D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES,
                                  D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES};
        dispatch.HitGroupTable={sbt_->GetGPUVirtualAddress()+section*2,D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES,
                                D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES};
        dispatch.Width=width_; dispatch.Height=height_; dispatch.Depth=1;
        list_->DispatchRays(&dispatch);

        D3D12_RESOURCE_BARRIER barriers[2]{};
        barriers[0].Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[0].Transition={output_.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE};
        barriers[1].Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[1].Transition={backbuffers_[frame].Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_COPY_DEST};
        list_->ResourceBarrier(2,barriers);
        list_->CopyResource(backbuffers_[frame].Get(),output_.Get());
        barriers[0].Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_SOURCE;
        barriers[0].Transition.StateAfter=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        barriers[1].Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_DEST;
        barriers[1].Transition.StateAfter=D3D12_RESOURCE_STATE_PRESENT;
        list_->ResourceBarrier(2,barriers);
        check(list_->Close(),"Close command list");
        ID3D12CommandList* lists[]={list_.Get()}; queue_->ExecuteCommandLists(1,lists);
        check(swapchain_->Present(1,0),"Present DXR frame");
        const UINT64 value=++fenceValue_; check(queue_->Signal(fence_.Get(),value),"Signal frame fence");
        frameFence_[frame]=value;
    }
    std::wstring status() const { return ready_ ? (L"DXR active | " + std::to_wstring(width_) + L" x " + std::to_wstring(height_) + L" | Hardware ray tracing") : L"DXR not initialized"; }
private:
    void waitFrame(UINT i) {
        const UINT64 v=frameFence_[i];
        if(v && fence_->GetCompletedValue()<v) {
            check(fence_->SetEventOnCompletion(v,fenceEvent_),"Set frame fence event");
            WaitForSingleObject(fenceEvent_,INFINITE);
        }
    }
    void waitGpu() {
        if(!queue_ || !fence_ || !fenceEvent_) return;
        const UINT64 v=++fenceValue_;
        if(SUCCEEDED(queue_->Signal(fence_.Get(),v)) && fence_->GetCompletedValue()<v) {
            fence_->SetEventOnCompletion(v,fenceEvent_); WaitForSingleObject(fenceEvent_,INFINITE);
        }
    }
    void createRaytracingPipeline() {
#ifndef XWE_SHADER_DIR
#define XWE_SHADER_DIR L"out/editor/shaders"
#endif
        auto dxil=readBinary(std::filesystem::path(XWE_SHADER_DIR)/L"pathtrace.dxil");
        D3D12_DESCRIPTOR_RANGE ranges[2]{};
        ranges[0].RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV; ranges[0].NumDescriptors=1;
        ranges[0].BaseShaderRegister=0; ranges[0].OffsetInDescriptorsFromTableStart=0;
        ranges[1].RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_UAV; ranges[1].NumDescriptors=1;
        ranges[1].BaseShaderRegister=0; ranges[1].OffsetInDescriptorsFromTableStart=0;
        D3D12_ROOT_PARAMETER params[2]{};
        for(UINT i=0;i<2;++i) { params[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            params[i].DescriptorTable.NumDescriptorRanges=1; params[i].DescriptorTable.pDescriptorRanges=&ranges[i];
            params[i].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL; }
        D3D12_ROOT_SIGNATURE_DESC rd{}; rd.NumParameters=2; rd.pParameters=params;
        ComPtr<ID3DBlob> blob,errors;
        check(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors),"Serialize DXR root signature");
        check(device_->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature_)),"Create DXR root signature");

        D3D12_EXPORT_DESC exports[]={{L"RayGen",nullptr,D3D12_EXPORT_FLAG_NONE},
            {L"Miss",nullptr,D3D12_EXPORT_FLAG_NONE},{L"ClosestHit",nullptr,D3D12_EXPORT_FLAG_NONE}};
        D3D12_DXIL_LIBRARY_DESC lib{}; lib.DXILLibrary={dxil.data(),dxil.size()};
        lib.NumExports=3; lib.pExports=exports;
        D3D12_HIT_GROUP_DESC hit{}; hit.HitGroupExport=L"TriangleHitGroup";
        hit.ClosestHitShaderImport=L"ClosestHit"; hit.Type=D3D12_HIT_GROUP_TYPE_TRIANGLES;
        D3D12_RAYTRACING_SHADER_CONFIG config{16,8};
        D3D12_RAYTRACING_PIPELINE_CONFIG pipeline{1};
        ID3D12RootSignature* root=rootSignature_.Get();
        const wchar_t* names[]={L"RayGen",L"Miss",L"ClosestHit",L"TriangleHitGroup"};
        D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION assoc{};
        D3D12_STATE_SUBOBJECT sub[6]{};
        sub[0]={D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY,&lib};
        sub[1]={D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP,&hit};
        sub[2]={D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG,&config};
        sub[3]={D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE,&root};
        sub[4]={D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG,&pipeline};
        assoc.pSubobjectToAssociate=&sub[2]; assoc.NumExports=4; assoc.pExports=names;
        sub[5]={D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION,&assoc};
        D3D12_STATE_OBJECT_DESC sd{}; sd.Type=D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
        sd.NumSubobjects=6; sd.pSubobjects=sub;
        check(device_->CreateStateObject(&sd,IID_PPV_ARGS(&stateObject_)),"Create DXR state object");
        check(stateObject_.As(&stateProperties_),"Query state object properties");
    }
    void createSceneAccelerationStructures() {
        const Vertex vertices[]={{{-1.2f,-1.0f,0.0f}},{{0.0f,1.2f,0.0f}},{{1.2f,-1.0f,0.0f}}};
        const UINT indices[]={0,1,2};
        vertexBuffer_=makeBuffer(device_.Get(),sizeof(vertices),D3D12_HEAP_TYPE_UPLOAD);
        indexBuffer_=makeBuffer(device_.Get(),sizeof(indices),D3D12_HEAP_TYPE_UPLOAD);
        upload(vertexBuffer_.Get(),vertices,sizeof(vertices)); upload(indexBuffer_.Get(),indices,sizeof(indices));
        D3D12_RAYTRACING_GEOMETRY_DESC geo{}; geo.Type=D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geo.Flags=D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
        geo.Triangles.VertexBuffer={vertexBuffer_->GetGPUVirtualAddress(),sizeof(Vertex)};
        geo.Triangles.VertexCount=3; geo.Triangles.VertexFormat=DXGI_FORMAT_R32G32B32_FLOAT;
        geo.Triangles.IndexBuffer=indexBuffer_->GetGPUVirtualAddress(); geo.Triangles.IndexCount=3;
        geo.Triangles.IndexFormat=DXGI_FORMAT_R32_UINT;
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
        inputs.Type=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        inputs.DescsLayout=D3D12_ELEMENTS_LAYOUT_ARRAY; inputs.NumDescs=1; inputs.pGeometryDescs=&geo;
        inputs.Flags=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};
        device_->GetRaytracingAccelerationStructurePrebuildInfo(&inputs,&info);
        blas_=makeBuffer(device_.Get(),info.ResultDataMaxSizeInBytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
        auto scratch=makeBuffer(device_.Get(),info.ScratchDataSizeInBytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        check(list_->Reset(allocators_[0].Get(),nullptr),"Reset AS command list");
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build{};
        build.Inputs=inputs; build.DestAccelerationStructureData=blas_->GetGPUVirtualAddress();
        build.ScratchAccelerationStructureData=scratch->GetGPUVirtualAddress();
        list_->BuildRaytracingAccelerationStructure(&build,0,nullptr);
        D3D12_RESOURCE_BARRIER uav{}; uav.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV; uav.UAV.pResource=blas_.Get();
        list_->ResourceBarrier(1,&uav);

        D3D12_RAYTRACING_INSTANCE_DESC instance{}; instance.Transform[0][0]=1;
        instance.Transform[1][1]=1; instance.Transform[2][2]=1;
        instance.InstanceMask=0xFF; instance.AccelerationStructure=blas_->GetGPUVirtualAddress();
        auto instanceBuffer=makeBuffer(device_.Get(),sizeof(instance),D3D12_HEAP_TYPE_UPLOAD);
        upload(instanceBuffer.Get(),&instance,sizeof(instance));
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS top{};
        top.Type=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        top.DescsLayout=D3D12_ELEMENTS_LAYOUT_ARRAY; top.NumDescs=1;
        top.InstanceDescs=instanceBuffer->GetGPUVirtualAddress();
        top.Flags=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ti{};
        device_->GetRaytracingAccelerationStructurePrebuildInfo(&top,&ti);
        tlas_=makeBuffer(device_.Get(),ti.ResultDataMaxSizeInBytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
        auto topScratch=makeBuffer(device_.Get(),ti.ScratchDataSizeInBytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        build={}; build.Inputs=top; build.DestAccelerationStructureData=tlas_->GetGPUVirtualAddress();
        build.ScratchAccelerationStructureData=topScratch->GetGPUVirtualAddress();
        list_->BuildRaytracingAccelerationStructure(&build,0,nullptr);
        uav.UAV.pResource=tlas_.Get(); list_->ResourceBarrier(1,&uav);
        check(list_->Close(),"Close AS command list");
        ID3D12CommandList* cmds[]={list_.Get()}; queue_->ExecuteCommandLists(1,cmds); waitGpu();
        instanceBuffer_=std::move(instanceBuffer); scratch_=std::move(scratch); topScratch_=std::move(topScratch);

        auto cpu=heap_->GetCPUDescriptorHandleForHeapStart();
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{}; srv.ViewDimension=D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
        srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.RaytracingAccelerationStructure.Location=tlas_->GetGPUVirtualAddress();
        device_->CreateShaderResourceView(nullptr,&srv,cpu);
    }
    void createShaderBindingTable() {
        constexpr UINT stride=D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
        constexpr UINT section=D3D12_SHADER_TABLE_BYTE_ALIGNMENT;
        auto sbt=makeBuffer(device_.Get(),section*3,D3D12_HEAP_TYPE_UPLOAD);
        void* mapped=nullptr; D3D12_RANGE read{0,0}; check(sbt->Map(0,&read,&mapped),"Map SBT");
        auto* bytes=static_cast<std::byte*>(mapped);
        const wchar_t* exports[]={L"RayGen",L"Miss",L"TriangleHitGroup"};
        for(UINT i=0;i<3;++i) {
            const void* id=stateProperties_->GetShaderIdentifier(exports[i]);
            if(!id) throw std::runtime_error("DXR shader identifier is missing");
            std::memcpy(bytes+i*section,id,stride);
        }
        sbt->Unmap(0,nullptr); sbt_=std::move(sbt);
    }

    ComPtr<IDXGIFactory4> factory_; ComPtr<ID3D12Device5> device_;
    ComPtr<ID3D12CommandQueue> queue_; ComPtr<IDXGISwapChain3> swapchain_;
    std::array<ComPtr<ID3D12CommandAllocator>,kFrameCount> allocators_{};
    ComPtr<ID3D12GraphicsCommandList4> list_; ComPtr<ID3D12Fence> fence_;
    HANDLE fenceEvent_=nullptr; UINT64 fenceValue_=0; std::array<UINT64,kFrameCount> frameFence_{};
    ComPtr<ID3D12DescriptorHeap> heap_; UINT descriptorIncrement_=0;
    ComPtr<ID3D12RootSignature> rootSignature_; ComPtr<ID3D12StateObject> stateObject_;
    ComPtr<ID3D12StateObjectProperties> stateProperties_;
    ComPtr<ID3D12Resource> vertexBuffer_,indexBuffer_,blas_,tlas_,instanceBuffer_,scratch_,topScratch_,sbt_,output_;
    std::array<ComPtr<ID3D12Resource>,kFrameCount> backbuffers_{};
    UINT width_=0,height_=0; bool ready_=false;
};


DXRViewport::DXRViewport() : renderer_(std::make_unique<Renderer>()) {}
DXRViewport::~DXRViewport() = default;
void DXRViewport::initialize(HWND hwnd, UINT width, UINT height) { renderer_->initialize(hwnd,width,height); }
void DXRViewport::resize(UINT width, UINT height) { if(renderer_) renderer_->resize(width,height); }
void DXRViewport::render() { if(renderer_) renderer_->render(); }
std::wstring DXRViewport::status() const { return renderer_ ? renderer_->status() : L"DXR not initialized"; }
