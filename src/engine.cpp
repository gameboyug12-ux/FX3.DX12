#include "fx3/engine.hpp"

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

using Microsoft::WRL::ComPtr;

namespace fx3 {
namespace {

constexpr UINT kFrameCount = 2;

[[noreturn]] void ThrowIfFailed(HRESULT result, const char* operation) {
    if (FAILED(result)) {
        throw std::runtime_error(std::string(operation) + " failed (HRESULT=" +
                                 std::to_string(static_cast<unsigned long>(result)) + ")");
    }
}

class D3D12Renderer final {
public:
    void Initialize(HWND window, UINT width, UINT height, bool enable_debug) {
        UINT factory_flags = 0;
#ifdef FX3_DEBUG
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
        factory_->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);

        D3D12_DESCRIPTOR_HEAP_DESC rtv_desc{};
        rtv_desc.NumDescriptors = kFrameCount;
        rtv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        ThrowIfFailed(device_->CreateDescriptorHeap(&rtv_desc, IID_PPV_ARGS(&rtv_heap_)),
                      "CreateDescriptorHeap");
        rtv_increment_ = device_->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

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

        CreateRenderTargets();
        frame_index_ = swap_chain_->GetCurrentBackBufferIndex();
    }

    ~D3D12Renderer() {
        WaitForGpu();
        if (fence_event_ != nullptr) {
            CloseHandle(fence_event_);
        }
    }

    void Render() {
        WaitForFrame(frame_index_);

        auto& allocator = command_allocators_[frame_index_];
        ThrowIfFailed(allocator->Reset(), "Reset command allocator");
        ThrowIfFailed(command_list_->Reset(allocator.Get(), nullptr),
                      "Reset command list");

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = render_targets_[frame_index_].Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        command_list_->ResourceBarrier(1, &barrier);

        D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
        rtv.ptr += static_cast<SIZE_T>(frame_index_) * rtv_increment_;
        command_list_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

        constexpr float clear_color[] = {0.025f, 0.035f, 0.055f, 1.0f};
        command_list_->ClearRenderTargetView(rtv, clear_color, 0, nullptr);

        std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
        command_list_->ResourceBarrier(1, &barrier);

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
        if (!swap_chain_ || width == 0 || height == 0) {
            return;
        }
        WaitForGpu();
        for (auto& target : render_targets_) {
            target.Reset();
        }
        ThrowIfFailed(swap_chain_->ResizeBuffers(
                          kFrameCount, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0),
                      "ResizeBuffers");
        frame_index_ = swap_chain_->GetCurrentBackBufferIndex();
        frame_fence_values_.fill(0);
        CreateRenderTargets();
    }

private:
    void CreateRenderTargets() {
        auto handle = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
        for (UINT index = 0; index < kFrameCount; ++index) {
            ThrowIfFailed(swap_chain_->GetBuffer(index, IID_PPV_ARGS(&render_targets_[index])),
                          "Get swap-chain buffer");
            device_->CreateRenderTargetView(render_targets_[index].Get(), nullptr, handle);
            handle.ptr += rtv_increment_;
        }
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
    std::array<ComPtr<ID3D12Resource>, kFrameCount> render_targets_{};
    std::array<ComPtr<ID3D12CommandAllocator>, kFrameCount> command_allocators_{};
    ComPtr<ID3D12GraphicsCommandList> command_list_;
    ComPtr<ID3D12Fence> fence_;
    UINT rtv_increment_ = 0;
    UINT frame_index_ = 0;
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

} // namespace fx3
