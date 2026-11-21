#pragma once
#include <d3d12.h>
#include <cstdint>
namespace fx3 {
struct Dx12FeatureReport {
 D3D12_RAYTRACING_TIER raytracing_tier = D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
 D3D12_MESH_SHADER_TIER mesh_shader_tier = D3D12_MESH_SHADER_TIER_NOT_SUPPORTED;
 D3D12_SAMPLER_FEEDBACK_TIER sampler_feedback_tier = D3D12_SAMPLER_FEEDBACK_TIER_NOT_SUPPORTED;
 bool enhanced_barriers = false;
 bool variable_rate_shading = false;
 bool SupportsDXR() const noexcept { return raytracing_tier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED; }
};
Dx12FeatureReport QueryDx12Features(ID3D12Device* device);
} // namespace fx3
