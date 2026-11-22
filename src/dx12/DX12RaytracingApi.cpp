#include "XWe/DX/DX12/DX12RaytracingApi.hpp"
#include <fstream>
#include <filesystem>
#include <vector>
#include <iterator>
#include <cstddef>

namespace XWe::DX::DX12 {
HRESULT DX12RaytracingApi::Initialize(ID3D12Device* device) noexcept {
    if (!device) return E_INVALIDARG;
    state_object_.Reset();
    properties_.Reset();
    global_root_signature_.Reset();
    device5_.Reset();

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options{};
    HRESULT hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options, sizeof(options));
    if (FAILED(hr)) return hr;
    if (options.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED) return DXGI_ERROR_UNSUPPORTED;
    return device->QueryInterface(IID_PPV_ARGS(&device5_));
}

HRESULT DX12RaytracingApi::CreatePipeline(const std::wstring& path) noexcept {
    if (!device5_) return E_NOINTERFACE;
    state_object_.Reset();
    properties_.Reset();
    global_root_signature_.Reset();

    try {
        std::ifstream file(std::filesystem::path(path), std::ios::binary | std::ios::ate);
        if (!file) return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        const auto length = static_cast<std::streamsize>(file.tellg());
        if (length <= 0) return E_FAIL;
        std::vector<std::byte> code(static_cast<std::size_t>(length));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(code.data()), length);
        if (!file) return E_FAIL;

        // Contiguous SRVs: t0=TLAS, t1=RTVertex[], t2=uint32 indices[],
        // t3=RTGeometryInfo[], t4=RTMaterial[]. UAV u0 is the output texture.
        D3D12_DESCRIPTOR_RANGE ranges[2]{};
        ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        ranges[0].NumDescriptors = kRTSceneSrvDescriptorCount;
        ranges[0].BaseShaderRegister = 0;
        ranges[0].RegisterSpace = 0;
        ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        ranges[1].NumDescriptors = kRTOutputUavDescriptorCount;
        ranges[1].BaseShaderRegister = 0;
        ranges[1].RegisterSpace = 0;
        ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER params[2]{};
        for (UINT i = 0; i < 2; ++i) {
            params[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            params[i].DescriptorTable.NumDescriptorRanges = 1;
            params[i].DescriptorTable.pDescriptorRanges = &ranges[i];
            params[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        }
        D3D12_ROOT_SIGNATURE_DESC root_desc{};
        root_desc.NumParameters = 2;
        root_desc.pParameters = params;
        Microsoft::WRL::ComPtr<ID3DBlob> root_blob, errors;
        HRESULT hr = D3D12SerializeRootSignature(&root_desc, D3D_ROOT_SIGNATURE_VERSION_1, &root_blob, &errors);
        if (FAILED(hr)) return hr;

        Microsoft::WRL::ComPtr<ID3D12RootSignature> root;
        hr = device5_->CreateRootSignature(0, root_blob->GetBufferPointer(), root_blob->GetBufferSize(), IID_PPV_ARGS(&root));
        if (FAILED(hr)) return hr;
        global_root_signature_ = root;

        D3D12_EXPORT_DESC exports[] = {
            {L"RayGen", nullptr, D3D12_EXPORT_FLAG_NONE},
            {L"Miss", nullptr, D3D12_EXPORT_FLAG_NONE},
            {L"ClosestHit", nullptr, D3D12_EXPORT_FLAG_NONE}
        };
        D3D12_DXIL_LIBRARY_DESC lib{};
        lib.DXILLibrary = {code.data(), code.size()};
        lib.NumExports = 3;
        lib.pExports = exports;

        D3D12_HIT_GROUP_DESC hit{};
        hit.HitGroupExport = L"TriangleHitGroup";
        hit.ClosestHitShaderImport = L"ClosestHit";
        hit.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;

        D3D12_RAYTRACING_SHADER_CONFIG shader_config{80, 8};
        // RayGen issues sequential TraceRay calls; no nested hit-shader recursion is used.
        D3D12_RAYTRACING_PIPELINE_CONFIG pipeline_config{1};
        ID3D12RootSignature* root_ptr = root.Get();
        const wchar_t* names[] = {L"RayGen", L"Miss", L"ClosestHit", L"TriangleHitGroup"};

        D3D12_STATE_SUBOBJECT subobjects[6]{};
        subobjects[0] = {D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &lib};
        subobjects[1] = {D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &hit};
        subobjects[2] = {D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &shader_config};
        subobjects[3] = {D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &root_ptr};
        subobjects[4] = {D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &pipeline_config};
        D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION association{};
        association.pSubobjectToAssociate = &subobjects[2];
        association.NumExports = 4;
        association.pExports = names;
        subobjects[5] = {D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION, &association};

        D3D12_STATE_OBJECT_DESC state{};
        state.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
        state.NumSubobjects = 6;
        state.pSubobjects = subobjects;

        Microsoft::WRL::ComPtr<ID3D12StateObject> candidate;
        hr = device5_->CreateStateObject(&state, IID_PPV_ARGS(&candidate));
        if (FAILED(hr)) return hr;
        Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> properties;
        hr = candidate.As(&properties);
        if (FAILED(hr)) return hr;
        state_object_ = std::move(candidate);
        properties_ = std::move(properties);
        return S_OK;
    } catch (...) {
        return E_FAIL;
    }
}
}
