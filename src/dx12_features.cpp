#include "fx3/dx12_features.hpp"
namespace XWe {
Dx12FeatureReport QueryDx12Features(ID3D12Device* device) {
 Dx12FeatureReport report{};
 if (!device) return report;
 D3D12_FEATURE_DATA_D3D12_OPTIONS5 o5{};
 if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,&o5,sizeof(o5)))) report.raytracing_tier=o5.RaytracingTier;
 D3D12_FEATURE_DATA_D3D12_OPTIONS7 o7{};
 if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7,&o7,sizeof(o7)))) {
  report.mesh_shader_tier=o7.MeshShaderTier; report.sampler_feedback_tier=o7.SamplerFeedbackTier;
 }
 D3D12_FEATURE_DATA_D3D12_OPTIONS12 o12{};
 if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12,&o12,sizeof(o12)))) report.enhanced_barriers=o12.EnhancedBarriersSupported!=FALSE;
 D3D12_FEATURE_DATA_D3D12_OPTIONS6 o6{};
 if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6,&o6,sizeof(o6)))) report.variable_rate_shading=o6.VariableShadingRateTier!=D3D12_VARIABLE_SHADING_RATE_TIER_NOT_SUPPORTED;
 return report;
}
} // namespace XWe
