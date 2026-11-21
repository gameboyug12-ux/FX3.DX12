#include "fx3/dxr_pipeline.hpp"
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <vector>
#include <cstddef>
#include <iterator>

namespace fx3 {
namespace {
std::vector<std::byte> ReadLibrary(const std::wstring& path) {
    std::ifstream file(std::filesystem::path(path), std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("Cannot open DXR shader library");
    const auto length = static_cast<std::streamsize>(file.tellg());
    if (length <= 0) throw std::runtime_error("DXR shader library is empty");
    std::vector<std::byte> bytes(static_cast<std::size_t>(length));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), length);
    if (!file) throw std::runtime_error("Cannot read DXR shader library");
    return bytes;
}
}

bool DxrPipeline::Initialize(ID3D12Device* device, const std::wstring& path) {
    if (device == nullptr) return false;
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&device5_)))) return false;

    const auto library = ReadLibrary(path);

    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 1;
    range.BaseShaderRegister = 0;
    range.RegisterSpace = 0;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER root_parameter{};
    root_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    root_parameter.DescriptorTable.NumDescriptorRanges = 1;
    root_parameter.DescriptorTable.pDescriptorRanges = &range;
    root_parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC root_desc{};
    root_desc.NumParameters = 1;
    root_desc.pParameters = &root_parameter;
    Microsoft::WRL::ComPtr<ID3DBlob> root_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> error_blob;
    HRESULT hr = D3D12SerializeRootSignature(&root_desc, D3D_ROOT_SIGNATURE_VERSION_1,
                                              &root_blob, &error_blob);
    if (FAILED(hr)) throw std::runtime_error("DXR global root signature serialization failed");
    hr = device5_->CreateRootSignature(0, root_blob->GetBufferPointer(),
                                       root_blob->GetBufferSize(),
                                       IID_PPV_ARGS(&global_root_signature_));
    if (FAILED(hr)) throw std::runtime_error("DXR global root signature creation failed");

    D3D12_EXPORT_DESC exports[] = {
        {L"RayGen", nullptr, D3D12_EXPORT_FLAG_NONE},
        {L"Miss", nullptr, D3D12_EXPORT_FLAG_NONE},
        {L"ClosestHit", nullptr, D3D12_EXPORT_FLAG_NONE}
    };
    D3D12_DXIL_LIBRARY_DESC library_desc{};
    library_desc.DXILLibrary.pShaderBytecode = library.data();
    library_desc.DXILLibrary.BytecodeLength = library.size();
    library_desc.NumExports = static_cast<UINT>(std::size(exports));
    library_desc.pExports = exports;

    D3D12_HIT_GROUP_DESC hit_group{};
    hit_group.HitGroupExport = L"TriangleHitGroup";
    hit_group.ClosestHitShaderImport = L"ClosestHit";
    hit_group.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;

    D3D12_RAYTRACING_SHADER_CONFIG shader_config{};
    shader_config.MaxPayloadSizeInBytes = 16;
    shader_config.MaxAttributeSizeInBytes = 8;

    D3D12_RAYTRACING_PIPELINE_CONFIG pipeline_config{};
    pipeline_config.MaxTraceRecursionDepth = 1;

    const wchar_t* shader_exports[] = {L"RayGen", L"Miss", L"ClosestHit"};
    D3D12_STATE_SUBOBJECT subobjects[6]{};
    subobjects[0] = {D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &library_desc};
    subobjects[1] = {D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &hit_group};
    subobjects[2] = {D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &shader_config};
    ID3D12RootSignature* root_signature = global_root_signature_.Get();
    subobjects[3] = {D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &root_signature};
    subobjects[4] = {D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &pipeline_config};
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION shader_config_association{};
    shader_config_association.pSubobjectToAssociate = &subobjects[2];
    shader_config_association.NumExports = static_cast<UINT>(std::size(shader_exports));
    shader_config_association.pExports = shader_exports;
    subobjects[5] = {D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION,
                     &shader_config_association};

    D3D12_STATE_OBJECT_DESC state_desc{};
    state_desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
    state_desc.NumSubobjects = static_cast<UINT>(std::size(subobjects));
    state_desc.pSubobjects = subobjects;

    hr = device5_->CreateStateObject(&state_desc, IID_PPV_ARGS(&state_object_));
    if (FAILED(hr)) throw std::runtime_error("DXR raytracing pipeline state object creation failed");
    return true;
}
} // namespace fx3
