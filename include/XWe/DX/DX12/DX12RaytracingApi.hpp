#pragma once
#include "DX12AccelerationStructure.hpp"
#include "DX12ShaderBindingTable.hpp"
#include "DX12RaytracingSceneData.hpp"
#include <wrl/client.h>
#include <string>

namespace XWe::DX::DX12 {
class DX12RaytracingApi final {
public:
    static constexpr UINT SceneSrvDescriptorCount = kRTSceneSrvDescriptorCount;
    static constexpr UINT OutputUavDescriptorCount = kRTOutputUavDescriptorCount;
    HRESULT Initialize(ID3D12Device* device) noexcept;
    HRESULT CreatePipeline(const std::wstring& dxil_library_path) noexcept;
    bool IsSupported() const noexcept { return device5_ != nullptr; }
    bool IsPipelineReady() const noexcept { return state_object_ != nullptr; }
    ID3D12StateObject* Pipeline() const noexcept { return state_object_.Get(); }
    ID3D12StateObjectProperties* PipelineProperties() const noexcept { return properties_.Get(); }
    ID3D12RootSignature* GlobalRootSignature() const noexcept { return global_root_signature_.Get(); }
    ID3D12Device5* Device() const noexcept { return device5_.Get(); }
private:
    Microsoft::WRL::ComPtr<ID3D12Device5> device5_;
    Microsoft::WRL::ComPtr<ID3D12StateObject> state_object_;
    Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> properties_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> global_root_signature_;
};
}
