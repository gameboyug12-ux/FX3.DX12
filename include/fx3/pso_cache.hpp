#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <cstddef>
#include <string>
#include <unordered_map>
namespace fx3 {
class GraphicsPsoCache final {
public:
 ID3D12PipelineState* GetOrCreate(ID3D12Device* device,const std::string& stable_key,const D3D12_GRAPHICS_PIPELINE_STATE_DESC& description);
 void Clear() noexcept { entries_.clear(); }
 std::size_t Size() const noexcept { return entries_.size(); }
private:
 std::unordered_map<std::string,Microsoft::WRL::ComPtr<ID3D12PipelineState>> entries_;
};
} // namespace fx3
