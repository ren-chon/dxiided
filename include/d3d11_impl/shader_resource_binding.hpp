#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "common/debug.hpp"

namespace dxiided {

class D3D11Device;

class D3D11ShaderResourceBinding {
   public:
    struct SamplerKey {
        D3D11_SAMPLER_DESC desc;
        bool operator==(const SamplerKey& other) const {
            return memcmp(&desc, &other.desc, sizeof(desc)) == 0;
        }
    };

    struct SamplerKeyHasher {
        size_t operator()(const SamplerKey& key) const {
            const uint8_t* data =
                reinterpret_cast<const uint8_t*>(&key.desc);
            size_t hash = 0;
            for (size_t i = 0; i < sizeof(key.desc); i++) {
                hash = hash * 31 + data[i];
            }
            return hash;
        }
    };

    static Microsoft::WRL::ComPtr<ID3D11SamplerState> GetOrCreateSampler(
        ID3D11Device* device, const D3D11_SAMPLER_DESC* desc);

    // Constant buffer optimization
    struct ConstantBufferKey {
        size_t size;
        std::vector<uint8_t> data;
        bool operator==(const ConstantBufferKey& other) const {
            return size == other.size && data == other.data;
        }
    };

    struct ConstantBufferKeyHasher {
        size_t operator()(const ConstantBufferKey& key) const {
            size_t hash = key.size;
            for (uint8_t byte : key.data) {
                hash = hash * 31 + byte;
            }
            return hash;
        }
    };

    static Microsoft::WRL::ComPtr<ID3D11Buffer> GetOrCreateConstantBuffer(
        ID3D11Device* device, const void* data, size_t size);

    // Resource binding state
    struct BindingState {
        std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> srvs;
        std::vector<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>> uavs;
        std::vector<Microsoft::WRL::ComPtr<ID3D11SamplerState>> samplers;
        std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> constantBuffers;
    };

    static void ApplyBindings(ID3D11DeviceContext* context,
                            const BindingState& state);

   private:
    static std::unordered_map<SamplerKey,
                             Microsoft::WRL::ComPtr<ID3D11SamplerState>,
                             SamplerKeyHasher>
        s_samplerCache;
    static std::unordered_map<ConstantBufferKey,
                             Microsoft::WRL::ComPtr<ID3D11Buffer>,
                             ConstantBufferKeyHasher>
        s_constantBufferCache;
    static std::mutex s_cacheMutex;
};

}  // namespace dxiided
