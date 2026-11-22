#include "XWe/Engine.hpp"
#include "XWe/DX12/Dx12Features.hpp"
#include "XWe/DX12/PipelineStateCache.hpp"
#include "XWe/DX12/DxrPipeline.hpp"
#include "XWe/Rendering/Upscaling/FSR3Loader.hpp"
#include "XWe/Rendering/Upscaling/UpscalingSettings.hpp"

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include <array>
#include <cstddef>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace XWe {
namespace {

constexpr UINT kFrameCount = 2;
constexpr UINT kConstantBufferAlignment = 256;

std::filesystem::path GetExecutableDirectory() {
    std::wstring executable_path(32768, L'\\0');
    const DWORD length = GetModuleFileNameW(
        nullptr, executable_path.data(), static_cast<DWORD>(executable_path.size()));
    if (length == 0 || length >= executable_path.size()) {
        return std::filesystem::current_path();
    }
    executable_path.resize(length);
    return std::filesystem::path(executable_path).parent_path();
}

[[noreturn]] void ThrowIfFailed(HRESULT result, const char* operation) {
    if (FAILED(result)) {
        throw std::runtime_error(std::string(operation) + " failed (HRESULT=" +
                                 std::to_string(static_cast<unsigned long>(result)) + ")");
    }
}

std::vector<std::byte> ReadBinaryFile(const std::wstring& path) {
    std::ifstream file{std::filesystem::path(path), std::ios::binary | std::ios::ate};
    if (!file) {
        throw std::runtime_error("Unable to open compiled shader: " +
                                 std::string(path.begin(), path.end()));
    }
    const auto size = static_cast<std::streamsize>(file.tellg());
    if (size <= 0) {
        throw std::runtime_error("Compiled shader is empty");
    }
    std::vector<std::byte> data(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(data.data()), size);
    if (!file) {
        throw std::runtime_error("Failed to read compiled shader");
    }
    return data;
}

struct Vertex {
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT4 color;
};

struct alignas(16) FrameConstants {
    XMFLOAT4X4 world;
    XMFLOAT4X4 view_projection;
};

constexpr UINT AlignConstantBufferSize(UINT size) {
    return (size + kConstantBufferAlignment - 1) & ~(kConstantBufferAlignment - 1);
}

class D3D12Renderer final {
public:
    void Initialize(HWND window, UINT width, UINT height, bool enable_debug) {
#ifdef FX3_ENABLE_FSR3
        const auto fsr3_directory =
            GetExecutableDirectory() / L"third_party" / L"FSR";
        std::error_code fsr3_path_error;
        if (std::filesystem::is_directory(fsr3_directory, fsr3_path_error) &&
            !fsr3_path_error) {
            if (fsr3_loader_.Initialize(fsr3_directory)) {
                OutputDebugStringA(
                    "[XWe] FSR3 runtime DLLs loaded and exports validated; "
                    "GPU dispatch is not enabled yet.\\n");
            } else {
                std::wstring warning = L"[XWe] FSR3 runtime unavailable: ";
                warning += fsr3_loader_.LastError();
                warning += L"\\n";
                OutputDebugStringW(warning.c_str());
            }
        }
#endif
        UINT factory_flags = 0;
#ifdef XWE_DEBUG
        if (enable_debug) {
            ComPtr<ID3D12Debug> debug_controller;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)))) {
                debug_controller->EnableDebugLayer();
                factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
            }
        }
#else
        (void)enable_debug;
#endif
        ThrowIfFailed(CreateDXGIFactory2(factory_flags, IID_PPV_ARGS(&factory_)),
                      "CreateDXGIFactory2");

        ComPtr<IDXGIAdapter1> adapter;
        for (UINT index = 0;
             factory_->EnumAdapters1(index, &adapter) != DXGI_ERROR_NOT_FOUND;
             ++index) {
            DXGI_ADAPTER_DESC1 description{};
            adapter->GetDesc1(&description);
            if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0) {
                adapter.Reset();
                continue;
            }
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                            __uuidof(ID3D12Device), nullptr))) {
                break;
            }
            adapter.Reset();
        }
        ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                        IID_PPV_ARGS(&device_)),
                      "D3D12CreateDevice");
        const Dx12FeatureReport features = QueryDx12Features(device_.Get());
        char feature_line[256]{};
        std::snprintf(feature_line, sizeof(feature_line),
            "[XWe] DXR tier=%u | Mesh shader tier=%u | Sampler feedback tier=%u | Enhanced barriers=%s | VRS=%s\n",
            static_cast<unsigned>(features.raytracing_tier),
            static_cast<unsigned>(features.mesh_shader_tier),
            static_cast<unsigned>(features.sampler_feedback_tier),
            features.enhanced_barriers ? "yes" : "no",
            features.variable_rate_shading ? "yes" : "no");
        OutputDebugStringA(feature_line);
        if (features.SupportsDXR()) {
            try {
                const std::wstring dxr_shader = std::wstring(XWE_SHADER_DIR) + L"\\pathtrace.dxil";
                if (dxr_pipeline_.Initialize(device_.Get(), dxr_shader)) {
                    OutputDebugStringA("[XWe] DXR state object created; dispatch is not enabled yet.\n");
                }
            } catch (const std::exception& error) {
                const std::string warning = std::string("[XWe] DXR setup failed; raster fallback: ") + error.what() + "\n";
                OutputDebugStringA(warning.c_str());
            }
        } else {
            OutputDebugStringA("[XWe] DXR unsupported; using raster fallback.\n");
        }

        D3D12_COMMAND_QUEUE_DESC queue_desc{};
        queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        ThrowIfFailed(device_->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue_)),
                      "CreateCommandQueue");

        DXGI_SWAP_CHAIN_DESC1 swap_desc{};
        swap_desc.Width = width;
        swap_desc.Height = height;
        swap_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swap_desc.SampleDesc.Count = 1;
        swap_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swap_desc.BufferCount = kFrameCount;
        swap_desc.Scaling = DXGI_SCALING_STRETCH;
        swap_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swap_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

        ComPtr<IDXGISwapChain1> swap_chain;
        ThrowIfFailed(factory_->CreateSwapChainForHwnd(
                          queue_.Get(), window, &swap_desc, nullptr, nullptr, &swap_chain),
                      "CreateSwapChainForHwnd");
        ThrowIfFailed(swap_chain.As(&swap_chain_), "Query IDXGISwapChain3");
        ThrowIfFailed(factory_->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER),
                      "MakeWindowAssociation");

        width_ = width;
        height_ = height;
        const auto render_size = XWe::Rendering::Upscaling::SelectRenderResolution(
            {width_, height_}, upscaling_config_);
        render_width_ = render_size.width;
        render_height_ = render_size.height;

        CreateDescriptorHeaps();
        CreateCommandObjects();
        CreateRenderTargets();
        CreateUpscaleResources();
        CreateDepthBuffer(render_width_, render_height_);
        CreateGeometry();
        CreateConstantBuffers();
        CreatePipeline();
        CreateUpscalePipeline();

        frame_index_ = swap_chain_->GetCurrentBackBufferIndex();
    }

    ~D3D12Renderer() {
        WaitForGpu();
        for (auto& constants : mapped_constants_) {
            if (constants != nullptr) {
                constants->Unmap(0, nullptr);
            }
        }
        if (fence_event_ != nullptr) {
            CloseHandle(fence_event_);
        }
    }

    void Render() {
        WaitForFrame(frame_index_);
        UpdateFrameConstants();

        auto& allocator = command_allocators_[frame_index_];
        ThrowIfFailed(allocator->Reset(), "Reset command allocator");
        ThrowIfFailed(command_list_->Reset(allocator.Get(), pipeline_.Get()),
                      "Reset command list");

        D3D12_RESOURCE_BARRIER scene_to_render{};
        scene_to_render.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        scene_to_render.Transition.pResource = scene_color_.Get();
        scene_to_render.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        scene_to_render.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        scene_to_render.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        command_list_->ResourceBarrier(1, &scene_to_render);

        const auto scene_rtv = upscale_rtv_heap_->GetCPUDescriptorHandleForHeapStart();
        const auto dsv = dsv_heap_->GetCPUDescriptorHandleForHeapStart();
        command_list_->OMSetRenderTargets(1, &scene_rtv, FALSE, &dsv);

        constexpr float clear_color[] = {0.018f, 0.025f, 0.045f, 1.0f};
        command_list_->ClearRenderTargetView(scene_rtv, clear_color, 0, nullptr);
        command_list_->ClearDepthStencilView(
            dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        D3D12_VIEWPORT scene_viewport{};
        scene_viewport.Width = static_cast<float>(render_width_);
        scene_viewport.Height = static_cast<float>(render_height_);
        scene_viewport.MaxDepth = 1.0f;
        D3D12_RECT scene_scissor{0, 0, static_cast<LONG>(render_width_),
                                static_cast<LONG>(render_height_)};
        command_list_->RSSetViewports(1, &scene_viewport);
        command_list_->RSSetScissorRects(1, &scene_scissor);

        command_list_->SetGraphicsRootSignature(root_signature_.Get());
        command_list_->SetGraphicsRootConstantBufferView(
            0, constant_buffers_[frame_index_]->GetGPUVirtualAddress());
        command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list_->IASetVertexBuffers(0, 1, &vertex_buffer_view_);
        command_list_->IASetIndexBuffer(&index_buffer_view_);
        command_list_->DrawIndexedInstanced(index_count_, 1, 0, 0, 0);

        D3D12_RESOURCE_BARRIER scene_to_sample = scene_to_render;
        scene_to_sample.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        scene_to_sample.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        command_list_->ResourceBarrier(1, &scene_to_sample);

        D3D12_RESOURCE_BARRIER output_to_render{};
        output_to_render.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        output_to_render.Transition.pResource = render_targets_[frame_index_].Get();
        output_to_render.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        output_to_render.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        output_to_render.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        command_list_->ResourceBarrier(1, &output_to_render);

        D3D12_CPU_DESCRIPTOR_HANDLE output_rtv = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
        output_rtv.ptr += static_cast<SIZE_T>(frame_index_) * rtv_increment_;
        command_list_->OMSetRenderTargets(1, &output_rtv, FALSE, nullptr);
        D3D12_VIEWPORT output_viewport{};
        output_viewport.Width = static_cast<float>(width_);
        output_viewport.Height = static_cast<float>(height_);
        output_viewport.MaxDepth = 1.0f;
        D3D12_RECT output_scissor{0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_)};
        command_list_->RSSetViewports(1, &output_viewport);
        command_list_->RSSetScissorRects(1, &output_scissor);

        ID3D12DescriptorHeap* upscale_heaps[] = {upscale_srv_heap_.Get()};
        command_list_->SetDescriptorHeaps(1, upscale_heaps);
        command_list_->SetGraphicsRootSignature(upscale_root_signature_.Get());
        command_list_->SetPipelineState(upscale_pipeline_.Get());
        command_list_->SetGraphicsRootDescriptorTable(
            0, upscale_srv_heap_->GetGPUDescriptorHandleForHeapStart());
        command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list_->DrawInstanced(3, 1, 0, 0);

        D3D12_RESOURCE_BARRIER output_to_present = output_to_render;
        output_to_present.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        output_to_present.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        command_list_->ResourceBarrier(1, &output_to_present);

        ThrowIfFailed(command_list_->Close(), "Close command list");
        ID3D12CommandList* command_lists[] = {command_list_.Get()};
        queue_->ExecuteCommandLists(1, command_lists);
        ThrowIfFailed(swap_chain_->Present(1, 0), "Present");

        const UINT submitted_frame = frame_index_;
        const UINT64 signal_value = ++next_fence_value_;
        ThrowIfFailed(queue_->Signal(fence_.Get(), signal_value), "Signal frame fence");
        frame_fence_values_[submitted_frame] = signal_value;
        frame_index_ = swap_chain_->GetCurrentBackBufferIndex();
    }

    void Resize(UINT width, UINT height) {
        if (!swap_chain_ || width == 0 || height == 0 ||
            (width == width_ && height == height_)) {
            return;
        }
        WaitForGpu();
        for (auto& target : render_targets_) {
            target.Reset();
        }
        depth_buffer_.Reset();

        ThrowIfFailed(swap_chain_->ResizeBuffers(
                          kFrameCount, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0),
                      "ResizeBuffers");
        width_ = width;
        height_ = height;
        const auto render_size = XWe::Rendering::Upscaling::SelectRenderResolution(
            {width_, height_}, upscaling_config_);
        render_width_ = render_size.width;
        render_height_ = render_size.height;
        frame_index_ = swap_chain_->GetCurrentBackBufferIndex();
        frame_fence_values_.fill(0);
        scene_color_.Reset();
        depth_buffer_.Reset();
        CreateRenderTargets();
        CreateUpscaleResources();
        CreateDepthBuffer(render_width_, render_height_);
    }

private:
    void CreateDescriptorHeaps() {
        D3D12_DESCRIPTOR_HEAP_DESC rtv_desc{};
        rtv_desc.NumDescriptors = kFrameCount;
        rtv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        ThrowIfFailed(device_->CreateDescriptorHeap(&rtv_desc, IID_PPV_ARGS(&rtv_heap_)),
                      "Create RTV descriptor heap");
        rtv_increment_ = device_->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        D3D12_DESCRIPTOR_HEAP_DESC upscale_rtv_desc{};
        upscale_rtv_desc.NumDescriptors = 1;
        upscale_rtv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        ThrowIfFailed(device_->CreateDescriptorHeap(
            &upscale_rtv_desc, IID_PPV_ARGS(&upscale_rtv_heap_)),
            "Create upscaling RTV heap");

        D3D12_DESCRIPTOR_HEAP_DESC upscale_srv_desc{};
        upscale_srv_desc.NumDescriptors = 1;
        upscale_srv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        upscale_srv_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(device_->CreateDescriptorHeap(
            &upscale_srv_desc, IID_PPV_ARGS(&upscale_srv_heap_)),
            "Create upscaling SRV heap");

        D3D12_DESCRIPTOR_HEAP_DESC dsv_desc{};
        dsv_desc.NumDescriptors = 1;
        dsv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        ThrowIfFailed(device_->CreateDescriptorHeap(&dsv_desc, IID_PPV_ARGS(&dsv_heap_)),
                      "Create DSV descriptor heap");
    }

    void CreateCommandObjects() {
        for (UINT index = 0; index < kFrameCount; ++index) {
            ThrowIfFailed(device_->CreateCommandAllocator(
                              D3D12_COMMAND_LIST_TYPE_DIRECT,
                              IID_PPV_ARGS(&command_allocators_[index])),
                          "CreateCommandAllocator");
        }
        ThrowIfFailed(device_->CreateCommandList(
                          0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                          command_allocators_[0].Get(), nullptr,
                          IID_PPV_ARGS(&command_list_)),
                      "CreateCommandList");
        ThrowIfFailed(command_list_->Close(), "Close initial command list");

        ThrowIfFailed(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                           IID_PPV_ARGS(&fence_)),
                      "CreateFence");
        fence_event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (fence_event_ == nullptr) {
            throw std::runtime_error("CreateEventW failed");
        }
    }

    void CreateRenderTargets() {
        auto handle = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
        for (UINT index = 0; index < kFrameCount; ++index) {
            ThrowIfFailed(swap_chain_->GetBuffer(index, IID_PPV_ARGS(&render_targets_[index])),
                          "Get swap-chain buffer");
            device_->CreateRenderTargetView(render_targets_[index].Get(), nullptr, handle);
            handle.ptr += rtv_increment_;
        }
    }

    void CreateUpscaleResources() {
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = render_width_;
        desc.Height = render_height_;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE clear{};
        clear.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        clear.Color[0] = 0.018f;
        clear.Color[1] = 0.025f;
        clear.Color[2] = 0.045f;
        clear.Color[3] = 1.0f;

        ThrowIfFailed(device_->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clear,
            IID_PPV_ARGS(&scene_color_)), "Create low-resolution scene color");

        D3D12_RENDER_TARGET_VIEW_DESC rtv{};
        rtv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        device_->CreateRenderTargetView(
            scene_color_.Get(), &rtv,
            upscale_rtv_heap_->GetCPUDescriptorHandleForHeapStart());

        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
        srv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Texture2D.MipLevels = 1;
        device_->CreateShaderResourceView(
            scene_color_.Get(), &srv,
            upscale_srv_heap_->GetCPUDescriptorHandleForHeapStart());
    }

    void CreateUpscalePipeline() {
        const std::wstring shader_directory = XWE_SHADER_DIR;
        const auto vertex_shader = ReadBinaryFile(shader_directory + L"\\upscale_vs.hlsl.cso");
        const auto pixel_shader = ReadBinaryFile(shader_directory + L"\\upscale_ps.hlsl.cso");

        D3D12_DESCRIPTOR_RANGE range{};
        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        range.NumDescriptors = 1;
        range.BaseShaderRegister = 0;
        range.RegisterSpace = 0;
        range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER parameter{};
        parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameter.DescriptorTable.NumDescriptorRanges = 1;
        parameter.DescriptorTable.pDescriptorRanges = &range;
        parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC sampler{};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.ShaderRegister = 0;
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;

        D3D12_ROOT_SIGNATURE_DESC root_desc{};
        root_desc.NumParameters = 1;
        root_desc.pParameters = &parameter;
        root_desc.NumStaticSamplers = 1;
        root_desc.pStaticSamplers = &sampler;

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> errors;
        ThrowIfFailed(D3D12SerializeRootSignature(
            &root_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors),
            "Serialize upscaling root signature");
        ThrowIfFailed(device_->CreateRootSignature(
            0, signature->GetBufferPointer(), signature->GetBufferSize(),
            IID_PPV_ARGS(&upscale_root_signature_)),
            "Create upscaling root signature");

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
        pso.pRootSignature = upscale_root_signature_.Get();
        pso.VS = {vertex_shader.data(), vertex_shader.size()};
        pso.PS = {pixel_shader.data(), pixel_shader.size()};
        pso.BlendState.AlphaToCoverageEnable = FALSE;
        pso.BlendState.IndependentBlendEnable = FALSE;
        const D3D12_RENDER_TARGET_BLEND_DESC blend = {
            FALSE, FALSE,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_LOGIC_OP_NOOP, D3D12_COLOR_WRITE_ENABLE_ALL
        };
        for (auto& target : pso.BlendState.RenderTarget) target = blend;
        pso.SampleMask = UINT_MAX;
        pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        pso.RasterizerState.DepthClipEnable = TRUE;
        pso.DepthStencilState.DepthEnable = FALSE;
        pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        pso.DepthStencilState.StencilEnable = FALSE;
        pso.InputLayout = {nullptr, 0};
        pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso.NumRenderTargets = 1;
        pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        pso.SampleDesc.Count = 1;
        ThrowIfFailed(device_->CreateGraphicsPipelineState(
            &pso, IID_PPV_ARGS(&upscale_pipeline_)),
            "Create spatial upscaling PSO");
    }

    void CreateDepthBuffer(UINT width, UINT height) {
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = width;
        desc.Height = height;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_D32_FLOAT;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE clear{};
        clear.Format = DXGI_FORMAT_D32_FLOAT;
        clear.DepthStencil.Depth = 1.0f;
        clear.DepthStencil.Stencil = 0;
        ThrowIfFailed(device_->CreateCommittedResource(
                          &heap, D3D12_HEAP_FLAG_NONE, &desc,
                          D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear,
                          IID_PPV_ARGS(&depth_buffer_)),
                      "Create depth buffer");

        D3D12_DEPTH_STENCIL_VIEW_DESC dsv{};
        dsv.Format = DXGI_FORMAT_D32_FLOAT;
        dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        device_->CreateDepthStencilView(
            depth_buffer_.Get(), &dsv, dsv_heap_->GetCPUDescriptorHandleForHeapStart());
    }

    void CreateGeometry() {
        constexpr std::array<Vertex, 24> vertices{{
            // Front (+Z)
            {{-1,-1, 1},{0,0,1},{1.00f,0.25f,0.20f,1}},
            {{-1, 1, 1},{0,0,1},{1.00f,0.25f,0.20f,1}},
            {{ 1, 1, 1},{0,0,1},{1.00f,0.25f,0.20f,1}},
            {{ 1,-1, 1},{0,0,1},{1.00f,0.25f,0.20f,1}},
            // Back (-Z)
            {{ 1,-1,-1},{0,0,-1},{0.20f,0.65f,1.00f,1}},
            {{ 1, 1,-1},{0,0,-1},{0.20f,0.65f,1.00f,1}},
            {{-1, 1,-1},{0,0,-1},{0.20f,0.65f,1.00f,1}},
            {{-1,-1,-1},{0,0,-1},{0.20f,0.65f,1.00f,1}},
            // Top (+Y)
            {{-1,1,-1},{0,1,0},{0.35f,0.90f,0.50f,1}},
            {{-1,1, 1},{0,1,0},{0.35f,0.90f,0.50f,1}},
            {{ 1,1, 1},{0,1,0},{0.35f,0.90f,0.50f,1}},
            {{ 1,1,-1},{0,1,0},{0.35f,0.90f,0.50f,1}},
            // Bottom (-Y)
            {{-1,-1, 1},{0,-1,0},{0.95f,0.75f,0.20f,1}},
            {{-1,-1,-1},{0,-1,0},{0.95f,0.75f,0.20f,1}},
            {{ 1,-1,-1},{0,-1,0},{0.95f,0.75f,0.20f,1}},
            {{ 1,-1, 1},{0,-1,0},{0.95f,0.75f,0.20f,1}},
            // Right (+X)
            {{1,-1, 1},{1,0,0},{0.75f,0.35f,0.95f,1}},
            {{1,-1,-1},{1,0,0},{0.75f,0.35f,0.95f,1}},
            {{1, 1,-1},{1,0,0},{0.75f,0.35f,0.95f,1}},
            {{1, 1, 1},{1,0,0},{0.75f,0.35f,0.95f,1}},
            // Left (-X)
            {{-1,-1,-1},{-1,0,0},{0.20f,0.85f,0.85f,1}},
            {{-1,-1, 1},{-1,0,0},{0.20f,0.85f,0.85f,1}},
            {{-1, 1, 1},{-1,0,0},{0.20f,0.85f,0.85f,1}},
            {{-1, 1,-1},{-1,0,0},{0.20f,0.85f,0.85f,1}}
        }};
        constexpr std::array<std::uint16_t, 36> indices{{
             0, 1, 2, 0, 2, 3,  4, 5, 6, 4, 6, 7,
             8, 9,10, 8,10,11, 12,13,14,12,14,15,
            16,17,18,16,18,19, 20,21,22,20,22,23
        }};
        index_count_ = static_cast<UINT>(indices.size());

        const UINT vertex_bytes = static_cast<UINT>(sizeof(vertices));
        const UINT index_bytes = static_cast<UINT>(sizeof(indices));
        CreateUploadBuffer(vertex_bytes, &vertex_buffer_);
        CreateUploadBuffer(index_bytes, &index_buffer_);

        void* mapped = nullptr;
        D3D12_RANGE read_range{0, 0};
        ThrowIfFailed(vertex_buffer_->Map(0, &read_range, &mapped), "Map vertex buffer");
        std::memcpy(mapped, vertices.data(), vertex_bytes);
        vertex_buffer_->Unmap(0, nullptr);
        ThrowIfFailed(index_buffer_->Map(0, &read_range, &mapped), "Map index buffer");
        std::memcpy(mapped, indices.data(), index_bytes);
        index_buffer_->Unmap(0, nullptr);

        vertex_buffer_view_.BufferLocation = vertex_buffer_->GetGPUVirtualAddress();
        vertex_buffer_view_.SizeInBytes = vertex_bytes;
        vertex_buffer_view_.StrideInBytes = sizeof(Vertex);
        index_buffer_view_.BufferLocation = index_buffer_->GetGPUVirtualAddress();
        index_buffer_view_.SizeInBytes = index_bytes;
        index_buffer_view_.Format = DXGI_FORMAT_R16_UINT;
    }

    void CreateUploadBuffer(UINT64 size, ComPtr<ID3D12Resource>* resource) {
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = size;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ThrowIfFailed(device_->CreateCommittedResource(
                          &heap, D3D12_HEAP_FLAG_NONE, &desc,
                          D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                          IID_PPV_ARGS(resource->ReleaseAndGetAddressOf())),
                      "Create upload buffer");
    }

    void CreateConstantBuffers() {
        constexpr UINT buffer_size = AlignConstantBufferSize(sizeof(FrameConstants));
        for (UINT index = 0; index < kFrameCount; ++index) {
            CreateUploadBuffer(buffer_size, &constant_buffers_[index]);
            D3D12_RANGE read_range{0, 0};
            ThrowIfFailed(constant_buffers_[index]->Map(
                              0, &read_range, reinterpret_cast<void**>(&mapped_constants_[index])),
                          "Map frame constants");
        }
    }

    void CreatePipeline() {
        const std::wstring shader_directory = XWE_SHADER_DIR;
        const auto vertex_shader = ReadBinaryFile(shader_directory + L"\\mesh_vs.hlsl.cso");
        const auto pixel_shader = ReadBinaryFile(shader_directory + L"\\mesh_ps.hlsl.cso");

        D3D12_ROOT_PARAMETER root_parameter{};
        root_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        root_parameter.Descriptor.ShaderRegister = 0;
        root_parameter.Descriptor.RegisterSpace = 0;
        root_parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_SIGNATURE_DESC root_desc{};
        root_desc.NumParameters = 1;
        root_desc.pParameters = &root_parameter;
        root_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> signature_blob;
        ComPtr<ID3DBlob> error_blob;
        ThrowIfFailed(D3D12SerializeRootSignature(
                          &root_desc, D3D_ROOT_SIGNATURE_VERSION_1,
                          &signature_blob, &error_blob),
                      "Serialize root signature");
        ThrowIfFailed(device_->CreateRootSignature(
                          0, signature_blob->GetBufferPointer(),
                          signature_blob->GetBufferSize(),
                          IID_PPV_ARGS(&root_signature_)),
                      "Create root signature");

        constexpr D3D12_INPUT_ELEMENT_DESC input_layout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
             offsetof(Vertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
             offsetof(Vertex, normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
             offsetof(Vertex, color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
        pso.pRootSignature = root_signature_.Get();
        pso.VS = {vertex_shader.data(), vertex_shader.size()};
        pso.PS = {pixel_shader.data(), pixel_shader.size()};
        pso.BlendState.AlphaToCoverageEnable = FALSE;
        pso.BlendState.IndependentBlendEnable = FALSE;
        const D3D12_RENDER_TARGET_BLEND_DESC blend = {
            FALSE, FALSE,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_LOGIC_OP_NOOP,
            D3D12_COLOR_WRITE_ENABLE_ALL
        };
        for (auto& target : pso.BlendState.RenderTarget) {
            target = blend;
        }
        pso.SampleMask = UINT_MAX;
        pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        pso.RasterizerState.FrontCounterClockwise = FALSE;
        pso.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        pso.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        pso.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        pso.RasterizerState.DepthClipEnable = TRUE;
        pso.RasterizerState.MultisampleEnable = FALSE;
        pso.RasterizerState.AntialiasedLineEnable = FALSE;
        pso.RasterizerState.ForcedSampleCount = 0;
        pso.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        pso.DepthStencilState.DepthEnable = TRUE;
        pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        pso.DepthStencilState.StencilEnable = FALSE;
        pso.InputLayout = {input_layout, static_cast<UINT>(std::size(input_layout))};
        pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso.NumRenderTargets = 1;
        pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        pso.SampleDesc.Count = 1;
        pipeline_ = pso_cache_.GetOrCreateGraphics(device_.Get(), "mesh.raster.default.v1", pso);
        if (pipeline_ == nullptr) {
            throw std::runtime_error("Graphics PSO cache returned a null pipeline");
        }
    }

    void UpdateFrameConstants() {
        const float time = static_cast<float>(GetTickCount64()) * 0.00035f;
        const XMMATRIX world = XMMatrixRotationY(time) *
                               XMMatrixRotationX(std::sin(time * 0.63f) * 0.22f);
        const XMVECTOR eye = XMVectorSet(0.0f, 1.25f, -5.5f, 1.0f);
        const XMVECTOR target = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        const XMMATRIX view = XMMatrixLookAtLH(eye, target, up);
        const float aspect = static_cast<float>(render_width_) /
                             static_cast<float>(render_height_);
        const XMMATRIX projection = XMMatrixPerspectiveFovLH(
            XMConvertToRadians(60.0f), aspect, 0.1f, 100.0f);

        FrameConstants constants{};
        XMStoreFloat4x4(&constants.world, world);
        XMStoreFloat4x4(&constants.view_projection, view * projection);
        std::memcpy(mapped_constants_[frame_index_], &constants, sizeof(constants));
    }

    void WaitForFrame(UINT frame) {
        const UINT64 value = frame_fence_values_[frame];
        if (value != 0 && fence_->GetCompletedValue() < value) {
            ThrowIfFailed(fence_->SetEventOnCompletion(value, fence_event_),
                          "SetEventOnCompletion");
            WaitForSingleObject(fence_event_, INFINITE);
        }
    }

    void WaitForGpu() noexcept {
        if (!queue_ || !fence_ || fence_event_ == nullptr) {
            return;
        }
        const UINT64 value = ++next_fence_value_;
        if (SUCCEEDED(queue_->Signal(fence_.Get(), value)) &&
            fence_->GetCompletedValue() < value) {
            if (SUCCEEDED(fence_->SetEventOnCompletion(value, fence_event_))) {
                WaitForSingleObject(fence_event_, INFINITE);
            }
        }
    }

    ComPtr<IDXGIFactory4> factory_;
    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12CommandQueue> queue_;
    ComPtr<IDXGISwapChain3> swap_chain_;
    ComPtr<ID3D12DescriptorHeap> rtv_heap_;
    ComPtr<ID3D12DescriptorHeap> dsv_heap_;
    ComPtr<ID3D12DescriptorHeap> upscale_rtv_heap_;
    ComPtr<ID3D12DescriptorHeap> upscale_srv_heap_;
    std::array<ComPtr<ID3D12Resource>, kFrameCount> render_targets_{};
    ComPtr<ID3D12Resource> scene_color_;
    ComPtr<ID3D12RootSignature> upscale_root_signature_;
    ComPtr<ID3D12PipelineState> upscale_pipeline_;
    ComPtr<ID3D12Resource> depth_buffer_;
    std::array<ComPtr<ID3D12CommandAllocator>, kFrameCount> command_allocators_{};
    ComPtr<ID3D12GraphicsCommandList> command_list_;
    ComPtr<ID3D12Fence> fence_;
    ComPtr<ID3D12RootSignature> root_signature_;
    ComPtr<ID3D12PipelineState> pipeline_;
    XWe::DX12::PipelineStateCache pso_cache_;
    DxrPipeline dxr_pipeline_;
    XWe::Rendering::Upscaling::FSR3Loader fsr3_loader_;
    XWe::Rendering::Upscaling::UpscalingConfig upscaling_config_{};
    ComPtr<ID3D12Resource> vertex_buffer_;
    ComPtr<ID3D12Resource> index_buffer_;
    std::array<ComPtr<ID3D12Resource>, kFrameCount> constant_buffers_{};
    std::array<std::byte*, kFrameCount> mapped_constants_{};
    D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view_{};
    D3D12_INDEX_BUFFER_VIEW index_buffer_view_{};
    UINT rtv_increment_ = 0;
    UINT width_ = 1;
    UINT height_ = 1;
    UINT render_width_ = 1;
    UINT render_height_ = 1;
    UINT frame_index_ = 0;
    UINT index_count_ = 0;
    UINT64 next_fence_value_ = 0;
    std::array<UINT64, kFrameCount> frame_fence_values_{};
    HANDLE fence_event_ = nullptr;
};

} // namespace

class Engine::Impl final {
public:
    explicit Impl(EngineConfig config) : config_(std::move(config)) {}

    int Run(HINSTANCE instance, int show_command) {
        try {
            RegisterWindowClass(instance);
            RECT bounds{0, 0, static_cast<LONG>(config_.window_width),
                        static_cast<LONG>(config_.window_height)};
            AdjustWindowRect(&bounds, WS_OVERLAPPEDWINDOW, FALSE);

            window_ = CreateWindowExW(
                0, WindowClassName(), config_.window_title, WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT, CW_USEDEFAULT, bounds.right - bounds.left,
                bounds.bottom - bounds.top, nullptr, nullptr, instance, this);
            if (!window_) {
                throw std::runtime_error("CreateWindowExW failed");
            }

            renderer_.Initialize(window_, config_.window_width,
                                 config_.window_height, config_.enable_debug_layer);
            ShowWindow(window_, show_command);
            UpdateWindow(window_);

            MSG message{};
            while (message.message != WM_QUIT) {
                if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                } else {
                    renderer_.Render();
                }
            }
            return static_cast<int>(message.wParam);
        } catch (const std::exception& error) {
            const std::string message = error.what();
            MessageBoxA(window_, message.c_str(), "FX3.DX12 - fatal error",
                        MB_OK | MB_ICONERROR);
            return 1;
        }
    }

private:
    static constexpr const wchar_t* WindowClassName() {
        return L"FX3DX12WindowClass";
    }

    void RegisterWindowClass(HINSTANCE instance) {
        WNDCLASSEXW window_class{};
        window_class.cbSize = sizeof(window_class);
        window_class.style = CS_HREDRAW | CS_VREDRAW;
        window_class.lpfnWndProc = WindowProcedure;
        window_class.hInstance = instance;
        window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        window_class.lpszClassName = WindowClassName();
        if (!RegisterClassExW(&window_class) &&
            GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            throw std::runtime_error("RegisterClassExW failed");
        }
    }

    static LRESULT CALLBACK WindowProcedure(HWND window, UINT message,
                                            WPARAM wparam, LPARAM lparam) {
        Impl* self = reinterpret_cast<Impl*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
            self = static_cast<Impl*>(create->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA,
                              reinterpret_cast<LONG_PTR>(self));
        }

        if (self) {
            switch (message) {
            case WM_SIZE:
                if (wparam != SIZE_MINIMIZED) {
                    self->renderer_.Resize(LOWORD(lparam), HIWORD(lparam));
                }
                return 0;
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
            default:
                break;
            }
        }
        return DefWindowProcW(window, message, wparam, lparam);
    }

    EngineConfig config_;
    HWND window_ = nullptr;
    D3D12Renderer renderer_;
};

Engine::Engine(const EngineConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

Engine::~Engine() = default;

int Engine::Run(HINSTANCE instance, int show_command) {
    return impl_->Run(instance, show_command);
}

} // namespace XWe
