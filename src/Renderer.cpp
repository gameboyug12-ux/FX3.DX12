#include "fx3/Renderer.hpp"
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl.h>
#include <array>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstring>

using Microsoft::WRL::ComPtr;
namespace fx3 {
namespace {
constexpr UINT FrameCount=2;
constexpr DXGI_FORMAT ColorFormat=DXGI_FORMAT_R8G8B8A8_UNORM;
void Check(HRESULT hr, const char* what) { if(FAILED(hr)) throw std::runtime_error(what); }
LRESULT CALLBACK WindowProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if(m==WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(h,m,w,l);
}
ComPtr<ID3D12Resource> Buffer(ID3D12Device* d, UINT64 size, D3D12_HEAP_TYPE heap) {
    D3D12_HEAP_PROPERTIES hp{}; hp.Type=heap;
    D3D12_RESOURCE_DESC rd{}; rd.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER; rd.Width=size;
    rd.Height=1; rd.DepthOrArraySize=1; rd.MipLevels=1; rd.SampleDesc.Count=1;
    rd.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> r;
    Check(d->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&rd,
        heap==D3D12_HEAP_TYPE_UPLOAD?D3D12_RESOURCE_STATE_GENERIC_READ:D3D12_RESOURCE_STATE_COMMON,
        nullptr,IID_PPV_ARGS(&r)),"Create buffer failed");
    return r;
}
}
int RunRenderer(HINSTANCE instance, int show, const Mesh& mesh, const Texture& texture) {
 try {
    WNDCLASSEXW wc{sizeof(wc)}; wc.lpfnWndProc=WindowProc; wc.hInstance=instance;
    wc.lpszClassName=L"FX3DX12Window"; wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
    RegisterClassExW(&wc);
    RECT wr{0,0,1280,720}; AdjustWindowRect(&wr,WS_OVERLAPPEDWINDOW,FALSE);
    HWND hwnd=CreateWindowExW(0,wc.lpszClassName,L"FX3 Engine v0.4 | Direct3D 12 Renderer",
        WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,wr.right-wr.left,wr.bottom-wr.top,
        nullptr,nullptr,instance,nullptr);
    if(!hwnd) throw std::runtime_error("CreateWindowExW failed");
    ShowWindow(hwnd,show);

#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debug;
    if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) debug->EnableDebugLayer();
#endif
    ComPtr<IDXGIFactory6> factory; Check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)),"CreateDXGIFactory2 failed");
    ComPtr<IDXGIAdapter1> adapter;
    for(UINT i=0;factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter))!=DXGI_ERROR_NOT_FOUND;++i) {
        DXGI_ADAPTER_DESC1 desc{}; adapter->GetDesc1(&desc);
        if(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE) { adapter.Reset(); continue; }
        if(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,__uuidof(ID3D12Device),nullptr))) break;
        adapter.Reset();
    }
    if(!adapter) Check(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)),"No D3D12 adapter or WARP");
    ComPtr<ID3D12Device> device; Check(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)),"D3D12CreateDevice failed");
    D3D12_COMMAND_QUEUE_DESC qd{}; qd.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue; Check(device->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue)),"CreateCommandQueue failed");

    DXGI_SWAP_CHAIN_DESC1 sd{}; sd.Width=1280; sd.Height=720; sd.Format=ColorFormat;
    sd.SampleDesc.Count=1; sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.BufferCount=FrameCount;
    sd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
    ComPtr<IDXGISwapChain1> sc1; Check(factory->CreateSwapChainForHwnd(queue.Get(),hwnd,&sd,nullptr,nullptr,&sc1),"CreateSwapChain failed");
    ComPtr<IDXGISwapChain3> swap; Check(sc1.As(&swap),"Query IDXGISwapChain3 failed");
    factory->MakeWindowAssociation(hwnd,DXGI_MWA_NO_ALT_ENTER);

    D3D12_DESCRIPTOR_HEAP_DESC rh{}; rh.NumDescriptors=FrameCount; rh.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    ComPtr<ID3D12DescriptorHeap> rtvHeap; Check(device->CreateDescriptorHeap(&rh,IID_PPV_ARGS(&rtvHeap)),"Create RTV heap failed");
    const UINT rtvStep=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    std::array<ComPtr<ID3D12Resource>,FrameCount> backBuffers;
    auto rtv=rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for(UINT i=0;i<FrameCount;++i) {
        Check(swap->GetBuffer(i,IID_PPV_ARGS(&backBuffers[i])),"Get swapchain buffer failed");
        auto h=rtv; h.ptr+=static_cast<SIZE_T>(i)*rtvStep; device->CreateRenderTargetView(backBuffers[i].Get(),nullptr,h);
    }

    D3D12_DESCRIPTOR_HEAP_DESC dh{}; dh.NumDescriptors=1; dh.Type=D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    ComPtr<ID3D12DescriptorHeap> dsvHeap; Check(device->CreateDescriptorHeap(&dh,IID_PPV_ARGS(&dsvHeap)),"Create DSV heap failed");
    D3D12_RESOURCE_DESC depthDesc{}; depthDesc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D; depthDesc.Width=1280; depthDesc.Height=720;
    depthDesc.DepthOrArraySize=1; depthDesc.MipLevels=1; depthDesc.Format=DXGI_FORMAT_D32_FLOAT; depthDesc.SampleDesc.Count=1; depthDesc.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    D3D12_CLEAR_VALUE depthClear{}; depthClear.Format=DXGI_FORMAT_D32_FLOAT; depthClear.DepthStencil.Depth=1.0f;
    D3D12_HEAP_PROPERTIES defaultHeap{}; defaultHeap.Type=D3D12_HEAP_TYPE_DEFAULT;
    ComPtr<ID3D12Resource> depth;
    Check(device->CreateCommittedResource(&defaultHeap,D3D12_HEAP_FLAG_NONE,&depthDesc,D3D12_RESOURCE_STATE_DEPTH_WRITE,&depthClear,IID_PPV_ARGS(&depth)),"Create depth buffer failed");
    device->CreateDepthStencilView(depth.Get(),nullptr,dsvHeap->GetCPUDescriptorHandleForHeapStart());

    std::array<ComPtr<ID3D12CommandAllocator>,FrameCount> allocators;
    for(auto& a:allocators) Check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&a)),"Create command allocator failed");
    ComPtr<ID3D12GraphicsCommandList> list;
    Check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocators[0].Get(),nullptr,IID_PPV_ARGS(&list)),"Create command list failed");

    D3D12_DESCRIPTOR_HEAP_DESC sh{}; sh.NumDescriptors=1; sh.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; sh.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ComPtr<ID3D12DescriptorHeap> srvHeap; Check(device->CreateDescriptorHeap(&sh,IID_PPV_ARGS(&srvHeap)),"Create SRV heap failed");

    D3D12_DESCRIPTOR_RANGE range{}; range.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV; range.NumDescriptors=1; range.BaseShaderRegister=0; range.OffsetInDescriptorsFromTableStart=0;
    D3D12_ROOT_PARAMETER param{}; param.ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; param.DescriptorTable.NumDescriptorRanges=1; param.DescriptorTable.pDescriptorRanges=&range; param.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{}; sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR; sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.ShaderRegister=0; sampler.MaxAnisotropy=1; sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS; sampler.MinLOD=0.0f; sampler.MaxLOD=D3D12_FLOAT32_MAX; sampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC rootDesc{}; rootDesc.NumParameters=1; rootDesc.pParameters=&param; rootDesc.NumStaticSamplers=1; rootDesc.pStaticSamplers=&sampler;
    rootDesc.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> rootBlob,rootError; Check(D3D12SerializeRootSignature(&rootDesc,D3D_ROOT_SIGNATURE_VERSION_1,&rootBlob,&rootError),"Serialize root signature failed");
    ComPtr<ID3D12RootSignature> root; Check(device->CreateRootSignature(0,rootBlob->GetBufferPointer(),rootBlob->GetBufferSize(),IID_PPV_ARGS(&root)),"Create root signature failed");

    const char* shader=R"(
Texture2D Albedo : register(t0); SamplerState LinearWrap : register(s0);
struct V { float3 p:POSITION; float2 uv:TEXCOORD0; float3 n:NORMAL; };
struct P { float4 p:SV_POSITION; float2 uv:TEXCOORD0; float3 n:NORMAL; };
P VSMain(V v){ P o; o.p=float4(v.p,1); o.uv=v.uv; o.n=v.n; return o; }
float4 PSMain(P i):SV_TARGET { float l=0.25+0.75*saturate(dot(normalize(i.n),normalize(float3(-0.4,0.8,-0.5)))); return float4(Albedo.Sample(LinearWrap,i.uv).rgb*l,1); }
)";
    ComPtr<ID3DBlob> vs,ps,compileError;
    Check(D3DCompile(shader,std::strlen(shader),nullptr,nullptr,nullptr,"VSMain","vs_5_1",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vs,&compileError),"Compile vertex shader failed");
    compileError.Reset();
    Check(D3DCompile(shader,std::strlen(shader),nullptr,nullptr,nullptr,"PSMain","ps_5_1",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&ps,&compileError),"Compile pixel shader failed");
    D3D12_INPUT_ELEMENT_DESC input[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,20,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{}; pd.pRootSignature=root.Get(); pd.VS={vs->GetBufferPointer(),vs->GetBufferSize()}; pd.PS={ps->GetBufferPointer(),ps->GetBufferSize()};
    pd.BlendState.AlphaToCoverageEnable=FALSE; pd.BlendState.IndependentBlendEnable=FALSE;
    auto& rt=pd.BlendState.RenderTarget[0]; rt.BlendEnable=FALSE; rt.LogicOpEnable=FALSE; rt.SrcBlend=D3D12_BLEND_ONE; rt.DestBlend=D3D12_BLEND_ZERO; rt.BlendOp=D3D12_BLEND_OP_ADD; rt.SrcBlendAlpha=D3D12_BLEND_ONE; rt.DestBlendAlpha=D3D12_BLEND_ZERO; rt.BlendOpAlpha=D3D12_BLEND_OP_ADD; rt.LogicOp=D3D12_LOGIC_OP_NOOP; rt.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
    pd.SampleMask=UINT_MAX; pd.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID; pd.RasterizerState.CullMode=D3D12_CULL_MODE_NONE; pd.RasterizerState.DepthClipEnable=TRUE;
    pd.DepthStencilState.DepthEnable=TRUE; pd.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ALL; pd.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_LESS;
    pd.DepthStencilState.StencilEnable=FALSE; pd.InputLayout={input,3}; pd.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pd.NumRenderTargets=1; pd.RTVFormats[0]=ColorFormat; pd.DSVFormat=DXGI_FORMAT_D32_FLOAT; pd.SampleDesc.Count=1;
    ComPtr<ID3D12PipelineState> pso; Check(device->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&pso)),"Create graphics pipeline failed");

    const UINT64 vbSize=static_cast<UINT64>(mesh.vertices.size()*sizeof(Vertex));
    const UINT64 ibSize=static_cast<UINT64>(mesh.indices.size()*sizeof(std::uint32_t));
    auto vb=Buffer(device.Get(),vbSize,D3D12_HEAP_TYPE_UPLOAD); auto ib=Buffer(device.Get(),ibSize,D3D12_HEAP_TYPE_UPLOAD);
    void* mapped=nullptr; Check(vb->Map(0,nullptr,&mapped),"Map vertex buffer failed"); std::memcpy(mapped,mesh.vertices.data(),static_cast<size_t>(vbSize)); vb->Unmap(0,nullptr);
    Check(ib->Map(0,nullptr,&mapped),"Map index buffer failed"); std::memcpy(mapped,mesh.indices.data(),static_cast<size_t>(ibSize)); ib->Unmap(0,nullptr);
    D3D12_VERTEX_BUFFER_VIEW vbv{vb->GetGPUVirtualAddress(),static_cast<UINT>(vbSize),sizeof(Vertex)};
    D3D12_INDEX_BUFFER_VIEW ibv{ib->GetGPUVirtualAddress(),static_cast<UINT>(ibSize),DXGI_FORMAT_R32_UINT};

    D3D12_RESOURCE_DESC td{}; td.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D; td.Width=texture.width; td.Height=texture.height;
    td.DepthOrArraySize=1; td.MipLevels=1; td.Format=ColorFormat; td.SampleDesc.Count=1; td.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;
    ComPtr<ID3D12Resource> gpuTexture; Check(device->CreateCommittedResource(&defaultHeap,D3D12_HEAP_FLAG_NONE,&td,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&gpuTexture)),"Create GPU texture failed");
    UINT64 uploadSize=0; D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{}; UINT rows=0; UINT64 rowBytes=0;
    device->GetCopyableFootprints(&td,0,1,0,&footprint,&rows,&rowBytes,&uploadSize);
    auto texUpload=Buffer(device.Get(),uploadSize,D3D12_HEAP_TYPE_UPLOAD);
    Check(texUpload->Map(0,nullptr,&mapped),"Map texture upload failed");
    auto* dst=static_cast<std::uint8_t*>(mapped)+footprint.Offset;
    for(UINT y=0;y<texture.height;++y) std::memcpy(dst+static_cast<size_t>(y)*footprint.Footprint.RowPitch,texture.rgba.data()+static_cast<size_t>(y)*texture.width*4,static_cast<size_t>(texture.width)*4);
    texUpload->Unmap(0,nullptr);

    D3D12_TEXTURE_COPY_LOCATION src{}; src.pResource=texUpload.Get(); src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; src.PlacedFootprint=footprint;
    D3D12_TEXTURE_COPY_LOCATION dstLoc{}; dstLoc.pResource=gpuTexture.Get(); dstLoc.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; dstLoc.SubresourceIndex=0;
    list->CopyTextureRegion(&dstLoc,0,0,0,&src,nullptr);
    D3D12_RESOURCE_BARRIER tb{}; tb.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; tb.Transition.pResource=gpuTexture.Get(); tb.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    tb.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_DEST; tb.Transition.StateAfter=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE; list->ResourceBarrier(1,&tb);
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{}; srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; srv.Format=ColorFormat;
    srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D; srv.Texture2D.MipLevels=1; device->CreateShaderResourceView(gpuTexture.Get(),&srv,srvHeap->GetCPUDescriptorHandleForHeapStart());
    Check(list->Close(),"Close upload list failed"); ID3D12CommandList* uploadLists[]={list.Get()}; queue->ExecuteCommandLists(1,uploadLists);

    ComPtr<ID3D12Fence> fence; Check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"Create fence failed");
    HANDLE fenceEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr); if(!fenceEvent) throw std::runtime_error("CreateEvent failed");
    UINT64 fenceValue=1; auto waitGpu=[&](){ const UINT64 v=fenceValue++; Check(queue->Signal(fence.Get(),v),"Queue signal failed"); if(fence->GetCompletedValue()<v){Check(fence->SetEventOnCompletion(v,fenceEvent),"Fence event failed"); WaitForSingleObject(fenceEvent,INFINITE);} };
    waitGpu();
    Check(allocators[0]->Reset(),"Reset allocator failed"); Check(list->Reset(allocators[0].Get(),pso.Get()),"Reset command list failed");

    D3D12_VIEWPORT viewport{0,0,1280.0f,720.0f,0,1}; D3D12_RECT scissor{0,0,1280,720};
    MSG msg{}; UINT frame=swap->GetCurrentBackBufferIndex();
    while(msg.message!=WM_QUIT) {
        if(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);continue;}
        Check(allocators[frame]->Reset(),"Frame allocator reset failed"); Check(list->Reset(allocators[frame].Get(),pso.Get()),"Frame command list reset failed");
        D3D12_RESOURCE_BARRIER barrier{}; barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; barrier.Transition.pResource=backBuffers[frame].Get();
        barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_PRESENT; barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_RENDER_TARGET; barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        list->ResourceBarrier(1,&barrier);
        auto rhnd=rtvHeap->GetCPUDescriptorHandleForHeapStart(); rhnd.ptr+=static_cast<SIZE_T>(frame)*rtvStep;
        const auto dsvHandle=dsvHeap->GetCPUDescriptorHandleForHeapStart();
        list->OMSetRenderTargets(1,&rhnd,FALSE,&dsvHandle);
        const float clear[]={0.035f,0.045f,0.065f,1}; list->ClearRenderTargetView(rhnd,clear,0,nullptr);
        list->ClearDepthStencilView(dsvHeap->GetCPUDescriptorHandleForHeapStart(),D3D12_CLEAR_FLAG_DEPTH,1.0f,0,0,nullptr);
        list->RSSetViewports(1,&viewport); list->RSSetScissorRects(1,&scissor);
        list->SetGraphicsRootSignature(root.Get()); ID3D12DescriptorHeap* heaps[]={srvHeap.Get()}; list->SetDescriptorHeaps(1,heaps);
        list->SetGraphicsRootDescriptorTable(0,srvHeap->GetGPUDescriptorHandleForHeapStart());
        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); list->IASetVertexBuffers(0,1,&vbv); list->IASetIndexBuffer(&ibv);
        list->DrawIndexedInstanced(static_cast<UINT>(mesh.indices.size()),1,0,0,0);
        barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_RENDER_TARGET; barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_PRESENT; list->ResourceBarrier(1,&barrier);
        Check(list->Close(),"Close frame command list failed"); ID3D12CommandList* lists[]={list.Get()}; queue->ExecuteCommandLists(1,lists);
        Check(swap->Present(1,0),"Present failed"); waitGpu(); frame=swap->GetCurrentBackBufferIndex();
    }
    waitGpu(); CloseHandle(fenceEvent); DestroyWindow(hwnd); return 0;
 } catch(const std::exception& e) {
    MessageBoxA(nullptr,e.what(),"FX3 Engine v0.4 - Renderer error",MB_ICONERROR); return 1;
 }
}
}
