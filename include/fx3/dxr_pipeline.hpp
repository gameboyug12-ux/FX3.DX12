#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <string>

namespace fx3 {
class DxrPipeline final {
public:
    bool Initialize(ID3D12Device* device, const std::wstring& dxil_library_path);
    bool IsReady() const noexcept { return state_object_ != nullptr; }
    ID3D12StateObject* Get() const noexcept { return state_object_.Get(); }
private:
    Microsoft::WRL::ComPtr<ID3D12Device5> device5_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> global_root_signature_;
    Microsoft::WRL::ComPtr<ID3D12StateObject> state_object_;
};
} // namespace fx3
