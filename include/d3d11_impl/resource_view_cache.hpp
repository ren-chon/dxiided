#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "common/debug.hpp"

namespace dxiided {

class D3D11Resource;

class D3D11ResourceViewCache {
   public:
    struct ViewKey {
        D3D11Resource* resource;
        union {
            D3D11_SHADER_RESOURCE_VIEW_DESC srv;
            D3D11_RENDER_TARGET_VIEW_DESC rtv;
            D3D11_DEPTH_STENCIL_VIEW_DESC dsv;
            D3D11_UNORDERED_ACCESS_VIEW_DESC uav;
        } desc;
        uint32_t type;  // 0=SRV, 1=RTV, 2=DSV, 3=UAV

        bool operator==(const ViewKey& other) const;
    };

    struct ViewKeyHasher {
        size_t operator()(const ViewKey& key) const;
    };

    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetOrCreateSRV(
        ID3D11Device* device, D3D11Resource* resource,
        const D3D11_SHADER_RESOURCE_VIEW_DESC* desc);

    static Microsoft::WRL::ComPtr<ID3D11RenderTargetView> GetOrCreateRTV(
        ID3D11Device* device, D3D11Resource* resource,
        const D3D11_RENDER_TARGET_VIEW_DESC* desc);

    static Microsoft::WRL::ComPtr<ID3D11DepthStencilView> GetOrCreateDSV(
        ID3D11Device* device, D3D11Resource* resource,
        const D3D11_DEPTH_STENCIL_VIEW_DESC* desc);

    static Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> GetOrCreateUAV(
        ID3D11Device* device, D3D11Resource* resource,
        const D3D11_UNORDERED_ACCESS_VIEW_DESC* desc);

   private:
    static std::unordered_map<ViewKey, Microsoft::WRL::ComPtr<IUnknown>,
                             ViewKeyHasher>
        s_viewCache;
    static std::mutex s_cacheMutex;
};

}  // namespace dxiided
