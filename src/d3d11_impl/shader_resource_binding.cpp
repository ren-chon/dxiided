#include "d3d11_impl/shader_resource_binding.hpp"

namespace dxiided {

// Static member initialization
std::unordered_map<D3D11ShaderResourceBinding::SamplerKey,
                   Microsoft::WRL::ComPtr<ID3D11SamplerState>,
                   D3D11ShaderResourceBinding::SamplerKeyHasher>
    D3D11ShaderResourceBinding::s_samplerCache;

std::unordered_map<D3D11ShaderResourceBinding::ConstantBufferKey,
                   Microsoft::WRL::ComPtr<ID3D11Buffer>,
                   D3D11ShaderResourceBinding::ConstantBufferKeyHasher>
    D3D11ShaderResourceBinding::s_constantBufferCache;

std::mutex D3D11ShaderResourceBinding::s_cacheMutex;

Microsoft::WRL::ComPtr<ID3D11SamplerState>
D3D11ShaderResourceBinding::GetOrCreateSampler(ID3D11Device* device,
                                               const D3D11_SAMPLER_DESC* desc) {
    SamplerKey key;
    key.desc = *desc;

    static std::mutex mutex;
    static std::unordered_map<SamplerKey,
                              Microsoft::WRL::ComPtr<ID3D11SamplerState>,
                              SamplerKeyHasher>
        cache;

    std::lock_guard<std::mutex> lock(mutex);

    auto it = cache.find(key);
    if (it != cache.end()) {
        return it->second;
    }

    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
    HRESULT hr = device->CreateSamplerState(desc, &sampler);
    if (FAILED(hr)) {
        TRACE("Failed to create sampler state: %08x\n", hr);
        return nullptr;
    }

    cache[key] = sampler;
    return sampler;
}

Microsoft::WRL::ComPtr<ID3D11Buffer>
D3D11ShaderResourceBinding::GetOrCreateConstantBuffer(ID3D11Device* device,
                                                      const void* data,
                                                      size_t size) {
    ConstantBufferKey key;
    key.size = size;
    key.data.assign(static_cast<const uint8_t*>(data),
                    static_cast<const uint8_t*>(data) + size);

    std::lock_guard<std::mutex> lock(s_cacheMutex);

    auto it = s_constantBufferCache.find(key);
    if (it != s_constantBufferCache.end()) {
        return it->second;
    }

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = static_cast<UINT>(size);
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = data;

    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
    HRESULT hr = device->CreateBuffer(&desc, &initData, &buffer);
    if (FAILED(hr)) {
        ERR("Failed to create constant buffer, hr %#x.\n", hr);
        return nullptr;
    }

    s_constantBufferCache[key] = buffer;
    return buffer;
}

void D3D11ShaderResourceBinding::ApplyBindings(ID3D11DeviceContext* context,
                                               const BindingState& state) {
    // Set shader resources
    if (!state.srvs.empty()) {
        std::vector<ID3D11ShaderResourceView*> srvPtrs(state.srvs.size());
        for (size_t i = 0; i < state.srvs.size(); i++) {
            srvPtrs[i] = state.srvs[i].Get();
        }
        context->VSSetShaderResources(0, static_cast<UINT>(srvPtrs.size()),
                                      srvPtrs.data());
        context->PSSetShaderResources(0, static_cast<UINT>(srvPtrs.size()),
                                      srvPtrs.data());
        context->GSSetShaderResources(0, static_cast<UINT>(srvPtrs.size()),
                                      srvPtrs.data());
        context->HSSetShaderResources(0, static_cast<UINT>(srvPtrs.size()),
                                      srvPtrs.data());
        context->DSSetShaderResources(0, static_cast<UINT>(srvPtrs.size()),
                                      srvPtrs.data());
        context->CSSetShaderResources(0, static_cast<UINT>(srvPtrs.size()),
                                      srvPtrs.data());
    }

    // Set UAVs
    if (!state.uavs.empty()) {
        std::vector<ID3D11UnorderedAccessView*> uavPtrs(state.uavs.size());
        for (size_t i = 0; i < state.uavs.size(); i++) {
            uavPtrs[i] = state.uavs[i].Get();
        }
        context->CSSetUnorderedAccessViews(0, static_cast<UINT>(uavPtrs.size()),
                                           uavPtrs.data(), nullptr);
    }

    // Set samplers
    if (!state.samplers.empty()) {
        std::vector<ID3D11SamplerState*> samplerPtrs(state.samplers.size());
        for (size_t i = 0; i < state.samplers.size(); i++) {
            samplerPtrs[i] = state.samplers[i].Get();
        }
        context->VSSetSamplers(0, static_cast<UINT>(samplerPtrs.size()),
                               samplerPtrs.data());
        context->PSSetSamplers(0, static_cast<UINT>(samplerPtrs.size()),
                               samplerPtrs.data());
        context->GSSetSamplers(0, static_cast<UINT>(samplerPtrs.size()),
                               samplerPtrs.data());
        context->HSSetSamplers(0, static_cast<UINT>(samplerPtrs.size()),
                               samplerPtrs.data());
        context->DSSetSamplers(0, static_cast<UINT>(samplerPtrs.size()),
                               samplerPtrs.data());
        context->CSSetSamplers(0, static_cast<UINT>(samplerPtrs.size()),
                               samplerPtrs.data());
    }

    // Set constant buffers
    if (!state.constantBuffers.empty()) {
        std::vector<ID3D11Buffer*> bufferPtrs(state.constantBuffers.size());
        for (size_t i = 0; i < state.constantBuffers.size(); i++) {
            bufferPtrs[i] = state.constantBuffers[i].Get();
        }
        context->VSSetConstantBuffers(0, static_cast<UINT>(bufferPtrs.size()),
                                      bufferPtrs.data());
        context->PSSetConstantBuffers(0, static_cast<UINT>(bufferPtrs.size()),
                                      bufferPtrs.data());
        context->GSSetConstantBuffers(0, static_cast<UINT>(bufferPtrs.size()),
                                      bufferPtrs.data());
        context->HSSetConstantBuffers(0, static_cast<UINT>(bufferPtrs.size()),
                                      bufferPtrs.data());
        context->DSSetConstantBuffers(0, static_cast<UINT>(bufferPtrs.size()),
                                      bufferPtrs.data());
        context->CSSetConstantBuffers(0, static_cast<UINT>(bufferPtrs.size()),
                                      bufferPtrs.data());
    }
}

}  // namespace dxiided
