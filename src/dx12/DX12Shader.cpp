#include "XWe/DX/DX12/DX12Shader.hpp"
#include <fstream>
#include <iterator>
#include <system_error>
namespace XWe::DX::DX12 {
HRESULT DX12Shader::Load(const std::filesystem::path& path) noexcept {
    try {
        std::ifstream file(path,std::ios::binary|std::ios::ate);
        if(!file) return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        const auto length=static_cast<std::streamsize>(file.tellg()); if(length<=0) return E_FAIL;
        std::vector<std::byte> data(static_cast<std::size_t>(length));
        file.seekg(0); file.read(reinterpret_cast<char*>(data.data()),length);
        if(!file) return E_FAIL;
        bytecode_=std::move(data); return S_OK;
    } catch (...) { return E_OUTOFMEMORY; }
}
HRESULT DX12Shader::SetBytecode(const void* data,std::size_t size) noexcept {
    if(!data || !size) return E_INVALIDARG;
    try { const auto* first=static_cast<const std::byte*>(data); bytecode_.assign(first,first+size); return S_OK; }
    catch (...) { return E_OUTOFMEMORY; }
}
D3D12_SHADER_BYTECODE DX12Shader::Bytecode() const noexcept {
    return {bytecode_.empty()?nullptr:bytecode_.data(),bytecode_.size()};
}
}
