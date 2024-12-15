#include "d3d11_impl/resource_view_cache.hpp"

#include "d3d11_impl/resource.hpp"

namespace dxiided {

// Static member initialization
std::unordered_map<D3D11ResourceViewCache::ViewKey,
                  Microsoft::WRL::ComPtr<IUnknown>,
                  D3D11ResourceViewCache::ViewKeyHasher>
    D3D11ResourceViewCache::s_viewCache;
std::mutex D3D11ResourceViewCache::s_cacheMutex;

bool D3D11ResourceViewCache::ViewKey::operator==(const ViewKey& other) const {
    if (resource != other.resource || type != other.type) {
        return false;
    }

    switch (type) {
        case 0:  // SRV
            return memcmp(&desc.srv, &other.desc.srv, sizeof(desc.srv)) == 0;
        case 1:  // RTV
            return memcmp(&desc.rtv, &other.desc.rtv, sizeof(desc.rtv)) == 0;
        case 2:  // DSV
            return memcmp(&desc.dsv, &other.desc.dsv, sizeof(desc.dsv)) == 0;
        case 3:  // UAV
            return memcmp(&desc.uav, &other.desc.uav, sizeof(desc.uav)) == 0;
        default:
            return false;
    }
}

size_t D3D11ResourceViewCache::ViewKeyHasher::operator()(
    const ViewKey& key) const {
    size_t hash = std::hash<void*>()(key.resource);
    hash = hash * 31 + key.type;

    const uint8_t* bytes = nullptr;
    size_t size = 0;

    switch (key.type) {
        case 0:  // SRV
            bytes = reinterpret_cast<const uint8_t*>(&key.desc.srv);
            size = sizeof(key.desc.srv);
            break;
        case 1:  // RTV
            bytes = reinterpret_cast<const uint8_t*>(&key.desc.rtv);
            size = sizeof(key.desc.rtv);
            break;
        case 2:  // DSV
            bytes = reinterpret_cast<const uint8_t*>(&key.desc.dsv);
            size = sizeof(key.desc.dsv);
            break;
        case 3:  // UAV
            bytes = reinterpret_cast<const uint8_t*>(&key.desc.uav);
            size = sizeof(key.desc.uav);
            break;
    }

    for (size_t i = 0; i < size; i++) {
        hash = hash * 31 + bytes[i];
    }

    return hash;
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>
D3D11ResourceViewCache::GetOrCreateSRV(
    ID3D11Device* device, D3D11Resource* resource,
    const D3D11_SHADER_RESOURCE_VIEW_DESC* desc) {
    ViewKey key;
    key.resource = resource;
    key.type = 0;
    if (desc) {
        key.desc.srv = *desc;
    } else {
        memset(&key.desc.srv, 0, sizeof(key.desc.srv));
    }

    std::lock_guard<std::mutex> lock(s_cacheMutex);

    auto it = s_viewCache.find(key);
    if (it != s_viewCache.end()) {
        return Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>(
            static_cast<ID3D11ShaderResourceView*>(it->second.Get()));
    }

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    HRESULT hr = device->CreateShaderResourceView(resource->GetD3D11Resource(),
                                                desc, &srv);
    if (FAILED(hr)) {
        ERR("Failed to create SRV, hr %#x.\n", hr);
        return nullptr;
    }

    s_viewCache[key] = srv;
    return srv;
}

Microsoft::WRL::ComPtr<ID3D11RenderTargetView>
D3D11ResourceViewCache::GetOrCreateRTV(
    ID3D11Device* device, D3D11Resource* resource,
    const D3D11_RENDER_TARGET_VIEW_DESC* desc) {
    ViewKey key;
    key.resource = resource;
    key.type = 1;
    if (desc) {
        key.desc.rtv = *desc;
    } else {
        memset(&key.desc.rtv, 0, sizeof(key.desc.rtv));
    }

    std::lock_guard<std::mutex> lock(s_cacheMutex);

    auto it = s_viewCache.find(key);
    if (it != s_viewCache.end()) {
        return Microsoft::WRL::ComPtr<ID3D11RenderTargetView>(
            static_cast<ID3D11RenderTargetView*>(it->second.Get()));
    }

    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
    HRESULT hr = device->CreateRenderTargetView(resource->GetD3D11Resource(),
                                              desc, &rtv);
    if (FAILED(hr)) {
        ERR("Failed to create RTV, hr %#x.\n", hr);
        return nullptr;
    }

    s_viewCache[key] = rtv;
    return rtv;
}

Microsoft::WRL::ComPtr<ID3D11DepthStencilView>
D3D11ResourceViewCache::GetOrCreateDSV(
    ID3D11Device* device, D3D11Resource* resource,
    const D3D11_DEPTH_STENCIL_VIEW_DESC* desc) {
    ViewKey key;
    key.resource = resource;
    key.type = 2;
    if (desc) {
        key.desc.dsv = *desc;
    } else {
        memset(&key.desc.dsv, 0, sizeof(key.desc.dsv));
    }

    std::lock_guard<std::mutex> lock(s_cacheMutex);

    auto it = s_viewCache.find(key);
    if (it != s_viewCache.end()) {
        return Microsoft::WRL::ComPtr<ID3D11DepthStencilView>(
            static_cast<ID3D11DepthStencilView*>(it->second.Get()));
    }

    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv;
    HRESULT hr = device->CreateDepthStencilView(resource->GetD3D11Resource(),
                                              desc, &dsv);
    if (FAILED(hr)) {
        ERR("Failed to create DSV, hr %#x.\n", hr);
        return nullptr;
    }

    s_viewCache[key] = dsv;
    return dsv;
}

Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>
D3D11ResourceViewCache::GetOrCreateUAV(
    ID3D11Device* device, D3D11Resource* resource,
    const D3D11_UNORDERED_ACCESS_VIEW_DESC* desc) {
    ViewKey key;
    key.resource = resource;
    key.type = 3;
    if (desc) {
        key.desc.uav = *desc;
    } else {
        memset(&key.desc.uav, 0, sizeof(key.desc.uav));
    }

    std::lock_guard<std::mutex> lock(s_cacheMutex);

    auto it = s_viewCache.find(key);
    if (it != s_viewCache.end()) {
        return Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>(
            static_cast<ID3D11UnorderedAccessView*>(it->second.Get()));
    }

    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
    HRESULT hr = device->CreateUnorderedAccessView(resource->GetD3D11Resource(),
                                                 desc, &uav);
    if (FAILED(hr)) {
        ERR("Failed to create UAV, hr %#x.\n", hr);
        return nullptr;
    }

    s_viewCache[key] = uav;
    return uav;
}

}  // namespace dxiided
