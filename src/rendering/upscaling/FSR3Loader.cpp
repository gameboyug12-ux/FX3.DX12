#include "XWe/Rendering/Upscaling/FSR3Loader.hpp"

#include <array>
#include <string>

namespace XWe::Rendering::Upscaling {
namespace {

struct ExportRequirement final {
    FSR3Loader::Module module;
    const char* name;
};

constexpr ExportRequirement kRequiredExports[] = {
    {FSR3Loader::Module::Fsr3, "ffxFsr3ConfigureFrameGeneration"},
    {FSR3Loader::Module::Fsr3, "ffxFsr3ContextCreate"},
    {FSR3Loader::Module::Fsr3, "ffxFsr3ContextDestroy"},
    {FSR3Loader::Module::Fsr3, "ffxFsr3ContextDispatchFrameGenerationPrepare"},
    {FSR3Loader::Module::Fsr3, "ffxFsr3ContextDispatchUpscale"},
    {FSR3Loader::Module::Fsr3, "ffxFsr3DispatchFrameGeneration"},
    {FSR3Loader::Module::Fsr3, "ffxFsr3GetEffectVersion"},
    {FSR3Loader::Module::Fsr3, "ffxFsr3GetJitterOffset"},
    {FSR3Loader::Module::Fsr3, "ffxFsr3GetJitterPhaseCount"},

    {FSR3Loader::Module::Upscaler, "ffxFsr3UpscalerContextCreate"},
    {FSR3Loader::Module::Upscaler, "ffxFsr3UpscalerContextDestroy"},
    {FSR3Loader::Module::Upscaler, "ffxFsr3UpscalerContextDispatch"},
    {FSR3Loader::Module::Upscaler, "ffxFsr3UpscalerGetJitterOffset"},
    {FSR3Loader::Module::Upscaler, "ffxFsr3UpscalerGetJitterPhaseCount"},

    {FSR3Loader::Module::OpticalFlow, "ffxOpticalflowContextCreate"},
    {FSR3Loader::Module::OpticalFlow, "ffxOpticalflowContextDestroy"},
    {FSR3Loader::Module::OpticalFlow, "ffxOpticalflowContextDispatch"},

    {FSR3Loader::Module::FrameInterpolation, "ffxFrameInterpolationContextCreate"},
    {FSR3Loader::Module::FrameInterpolation, "ffxFrameInterpolationContextDestroy"},
    {FSR3Loader::Module::FrameInterpolation, "ffxFrameInterpolationDispatch"},
    {FSR3Loader::Module::FrameInterpolation, "ffxFrameInterpolationPrepare"},

    {FSR3Loader::Module::Dx12Backend, "ffxGetInterfaceDX12"},
    {FSR3Loader::Module::Dx12Backend, "ffxGetScratchMemorySizeDX12"},
    {FSR3Loader::Module::Dx12Backend, "ffxGetResourceDX12"}
};

constexpr unsigned int Index(FSR3Loader::Module module) noexcept {
    return static_cast<unsigned int>(module);
}

} // namespace

FSR3Loader::~FSR3Loader() {
    Shutdown();
}

HMODULE FSR3Loader::GetModule(Module module) const noexcept {
    return modules_[Index(module)];
}

void FSR3Loader::SetWin32Error(
    const wchar_t* operation,
    const std::filesystem::path& path) {
    const DWORD error = GetLastError();
    last_error_ = operation;
    last_error_ += L" failed for ";
    last_error_ += path.wstring();
    last_error_ += L" (Win32 error ";
    last_error_ += std::to_wstring(error);
    last_error_ += L")";
}

bool FSR3Loader::LoadModule(Module module, const wchar_t* file_name) {
    const auto path = dll_directory_ / file_name;
    HMODULE handle = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (handle == nullptr) {
        SetWin32Error(L"LoadLibraryExW", path);
        return false;
    }
    modules_[Index(module)] = handle;
    return true;
}

bool FSR3Loader::ValidateExports() {
    for (const auto& requirement : kRequiredExports) {
        const auto module = GetModule(requirement.module);
        if (module == nullptr || GetProcAddress(module, requirement.name) == nullptr) {
            last_error_ = L"Required FSR3 export is missing: ";
            for (const char* p = requirement.name; *p != '\0'; ++p) {
                last_error_.push_back(static_cast<wchar_t>(*p));
            }
            return false;
        }
    }

    capabilities_.upscaler = true;
    capabilities_.frame_generation = true;
    capabilities_.dx12_backend = true;
    return true;
}

bool FSR3Loader::Initialize(const std::filesystem::path& dll_directory) {
    Shutdown();
    last_error_.clear();

    std::error_code ec;
    dll_directory_ = std::filesystem::absolute(dll_directory, ec);
    if (ec || !std::filesystem::is_directory(dll_directory_, ec) || ec) {
        last_error_ = L"FSR3 DLL directory does not exist: ";
        last_error_ += dll_directory.wstring();
        return false;
    }

    // Load dependencies before the top-level FSR3 module.
    if (!LoadModule(Module::Upscaler, L"ffx_fsr3upscaler_x64.dll") ||
        !LoadModule(Module::OpticalFlow, L"ffx_opticalflow_x64.dll") ||
        !LoadModule(Module::FrameInterpolation, L"ffx_frameinterpolation_x64.dll") ||
        !LoadModule(Module::Dx12Backend, L"ffx_backend_dx12_x64.dll") ||
        !LoadModule(Module::Fsr3, L"ffx_fsr3_x64.dll") ||
        !ValidateExports()) {
        Shutdown();
        return false;
    }

    loaded_ = true;
    return true;
}

void FSR3Loader::Shutdown() noexcept {
    loaded_ = false;
    capabilities_ = {};

    constexpr std::array<Module, 5> release_order{
        Module::Fsr3,
        Module::FrameInterpolation,
        Module::OpticalFlow,
        Module::Upscaler,
        Module::Dx12Backend
    };

    for (const auto module : release_order) {
        auto& handle = modules_[Index(module)];
        if (handle != nullptr) {
            FreeLibrary(handle);
            handle = nullptr;
        }
    }

}

} // namespace XWe::Rendering::Upscaling
