// gpu_va_manager.hpp
#pragma once

#include <d3d11.h>
#include <d3d12.h>
#include <atomic>
#include <mutex>
#include <unordered_map>

#include "common/debug.hpp"

namespace dxiided {

class GPUVAManager {
public:
    static GPUVAManager& Get();

    D3D12_GPU_VIRTUAL_ADDRESS AllocateVirtualAddress(
        ID3D11Resource* resource,
        D3D12_RESOURCE_DIMENSION dimension,
        uint64_t size);

    void FreeVirtualAddress(ID3D11Resource* resource);
    D3D12_GPU_VIRTUAL_ADDRESS GetVirtualAddress(ID3D11Resource* resource) const;
    bool IsValidAddress(D3D12_GPU_VIRTUAL_ADDRESS address) const;

private:
    GPUVAManager() = default;
    ~GPUVAManager() = default;
    GPUVAManager(const GPUVAManager&) = delete;
    GPUVAManager& operator=(const GPUVAManager&) = delete;

    // Base address and alignment constants
    static constexpr D3D12_GPU_VIRTUAL_ADDRESS BASE_ADDRESS = 0x100000000;  // 4GB
    static constexpr uint32_t MAX_RESOURCE_TYPES = 6;
    
    // Keep resources of same type together, use ascending addresses
    static constexpr uint64_t TYPE_BLOCK_SIZE = 0x40000000;  // 1GB per type
    static constexpr uint64_t TYPE_OFFSETS[MAX_RESOURCE_TYPES] = {
        BASE_ADDRESS + (0 * TYPE_BLOCK_SIZE),  // Default/Unknown
        BASE_ADDRESS + (1 * TYPE_BLOCK_SIZE),  // Buffer
        BASE_ADDRESS + (2 * TYPE_BLOCK_SIZE),  // Texture1D
        BASE_ADDRESS + (3 * TYPE_BLOCK_SIZE),  // Texture2D
        BASE_ADDRESS + (4 * TYPE_BLOCK_SIZE),  // Texture3D
        BASE_ADDRESS + (5 * TYPE_BLOCK_SIZE)   // Reserved
    };

    // Alignment requirements - keep consistent
    static constexpr uint64_t MINIMUM_ALIGNMENT = 65536;      // 64KB minimum
    static constexpr uint64_t BUFFER_ALIGNMENT = 65536;       // 64KB for buffers
    static constexpr uint64_t TEXTURE_ALIGNMENT = 65536;      // 64KB for textures
    static constexpr uint64_t LARGE_BUFFER_THRESHOLD = 65536; // 64KB threshold

    struct ResourceInfo {
        D3D12_GPU_VIRTUAL_ADDRESS address;
        uint64_t size;
        D3D12_RESOURCE_DIMENSION dimension;
    };

    std::atomic<uint64_t> m_typeCounters[MAX_RESOURCE_TYPES];
    std::unordered_map<ID3D11Resource*, ResourceInfo> m_resourceMap;
    mutable std::mutex m_mutex;

    uint32_t GetTypeIndex(D3D12_RESOURCE_DIMENSION dimension) const;
    uint64_t AlignSize(uint64_t size, D3D12_RESOURCE_DIMENSION dimension) const;
    D3D12_GPU_VIRTUAL_ADDRESS GenerateAddress(uint32_t typeIndex, uint64_t size);
};

}  // namespace dxiided