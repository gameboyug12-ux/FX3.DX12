#include "XWe/DX/DX12/DX12RendererStateObject.hpp"
namespace XWe::DX::DX12 {
HRESULT DX12RendererStateObject::Configure(const DX12RendererStateDesc& desc) noexcept {
    if (desc.render_target_count > desc.render_target_formats.size() ||
        desc.sample_desc.Count == 0 || desc.viewport.Width < 0 || desc.viewport.Height < 0)
        return E_INVALIDARG;
    for (UINT i=0;i<desc.render_target_count;++i)
        if (desc.render_target_formats[i] == DXGI_FORMAT_UNKNOWN) return E_INVALIDARG;
    desc_ = desc; configured_ = true; return S_OK;
}
}
