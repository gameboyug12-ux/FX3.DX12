#include "fx3/pso_cache.hpp"
#include <stdexcept>\n#include <utility>
namespace fx3 {
ID3D12PipelineState* GraphicsPsoCache::GetOrCreate(ID3D12Device* device,const std::string& key,const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc) {
 if (!device || key.empty()) throw std::invalid_argument("PSO cache requires a device and stable key");
 auto found=entries_.find(key);
 if(found!=entries_.end()) return found->second.Get();
 Microsoft::WRL::ComPtr<ID3D12PipelineState> state;
 HRESULT hr=device->CreateGraphicsPipelineState(&desc,IID_PPV_ARGS(&state));
 if(FAILED(hr)) throw std::runtime_error("CreateGraphicsPipelineState failed for PSO key: "+key);
 auto* result=state.Get(); entries_.emplace(key,std::move(state)); return result;
}
} // namespace fx3
