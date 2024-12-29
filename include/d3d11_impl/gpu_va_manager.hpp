#pragma once

#include <d3d11.h>
#include <d3d12.h>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <wrl/client.h>

#include "common/debug.hpp"

namespace dxiided {

class GPUVAManager {
public:
    static GPUVAManager& Get();

    // Generate a new virtual address for a resource
    D3D12_GPU_VIRTUAL_ADDRESS AllocateVirtualAddress(
        ID3D11Resource* resource,
        D3D12_RESOURCE_DIMENSION dimension,
        uint64_t size);

    // Free a virtual address when resource is destroyed
    void FreeVirtualAddress(ID3D11Resource* resource);

    // Get the virtual address for an existing resource
    D3D12_GPU_VIRTUAL_ADDRESS GetVirtualAddress(ID3D11Resource* resource) const;

    // Check if a virtual address is valid and belongs to a resource
    bool IsValidAddress(D3D12_GPU_VIRTUAL_ADDRESS address) const;

private:
    GPUVAManager() = default;
    ~GPUVAManager() = default;
    GPUVAManager(const GPUVAManager&) = delete;
    GPUVAManager& operator=(const GPUVAManager&) = delete;

    // Constants for address space management
    static constexpr D3D12_GPU_VIRTUAL_ADDRESS BASE_ADDRESS = 0x10000000000;  // 1TB base
    static constexpr uint32_t MAX_RESOURCE_TYPES = 6;  // Number of resource types
    static constexpr uint64_t TYPE_BLOCK_SIZE = 0x10000000;  // 256MB per type
    static constexpr uint64_t MINIMUM_ALIGNMENT = 256;
    static constexpr uint64_t SIMD_ALIGNMENT = 32;

    // Resource type offsets from base address
    static constexpr uint64_t TYPE_OFFSETS[MAX_RESOURCE_TYPES] = {
        0x00000000,  // Default/Unknown
        0x10000000,  // Buffer
        0x20000000,  // Texture1D
        0x30000000,  // Texture2D
        0x40000000,  // Texture3D
        0x50000000   // Reserved
    };

    // Resource tracking
    struct ResourceInfo {
        D3D12_GPU_VIRTUAL_ADDRESS address;
        uint64_t size;
        D3D12_RESOURCE_DIMENSION dimension;
    };

    // Counter for each resource type to ensure unique addresses
    std::atomic<uint64_t> m_typeCounters[MAX_RESOURCE_TYPES];
    
    // Map of resources to their virtual addresses
    std::unordered_map<ID3D11Resource*, ResourceInfo> m_resourceMap;
    mutable std::mutex m_mutex;

    // Helper functions
    uint32_t GetTypeIndex(D3D12_RESOURCE_DIMENSION dimension) const;
    uint64_t AlignSize(uint64_t size) const;
    D3D12_GPU_VIRTUAL_ADDRESS GenerateAddress(uint32_t typeIndex, uint64_t size);
};

}  // namespace dxiided
