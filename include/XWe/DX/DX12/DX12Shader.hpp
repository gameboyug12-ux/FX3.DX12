#pragma once
#include <d3d12.h>
#include <cstddef>
#include <cstddef>
#include <filesystem>
#include <vector>

namespace XWe::DX::DX12 {
class DX12Shader final {
public:
    HRESULT Load(const std::filesystem::path& path) noexcept;
    HRESULT SetBytecode(const void* data, std::size_t size) noexcept;
    D3D12_SHADER_BYTECODE Bytecode() const noexcept;
    bool IsValid() const noexcept { return !bytecode_.empty(); }
private:
    std::vector<std::byte> bytecode_;
};
}
