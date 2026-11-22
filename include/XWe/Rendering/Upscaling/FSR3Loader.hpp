#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <filesystem>
#include <string>

namespace XWe::Rendering::Upscaling {

struct FSR3Capabilities final {
    bool upscaler = false;
    bool frame_generation = false;
    bool dx12_backend = false;
};

class FSR3Loader final {
public:
    enum class Module : unsigned int {
        Fsr3 = 0, Upscaler, OpticalFlow, FrameInterpolation, Dx12Backend, Count
    };

    FSR3Loader() = default;
    ~FSR3Loader();

    FSR3Loader(const FSR3Loader&) = delete;
    FSR3Loader& operator=(const FSR3Loader&) = delete;
    FSR3Loader(FSR3Loader&&) = delete;
    FSR3Loader& operator=(FSR3Loader&&) = delete;

    // Loads the AMD FSR 3 DLL set and validates its exported entry points.
    // It deliberately does not invoke the version-specific effect ABI yet.
    bool Initialize(const std::filesystem::path& dll_directory);
    void Shutdown() noexcept;

    bool IsLoaded() const noexcept { return loaded_; }
    const FSR3Capabilities& Capabilities() const noexcept { return capabilities_; }
    const std::wstring& LastError() const noexcept { return last_error_; }
    const std::filesystem::path& DllDirectory() const noexcept { return dll_directory_; }

private:
    bool LoadModule(Module module, const wchar_t* file_name);
    bool ValidateExports();
    HMODULE GetModule(Module module) const noexcept;
    void SetWin32Error(const wchar_t* operation, const std::filesystem::path& path);

    HMODULE modules_[static_cast<unsigned int>(Module::Count)]{};
    std::filesystem::path dll_directory_;
    std::wstring last_error_;
    FSR3Capabilities capabilities_{};
    bool loaded_ = false;
};

} // namespace XWe::Rendering::Upscaling
