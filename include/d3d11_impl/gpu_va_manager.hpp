#pragma once

#include <atomic>
#include <d3d11.h>
#include <d3d12.h>
#include <map>
#include <set>
#include <mutex>

namespace dxiided {

struct GPUVARegion {
    D3D12_GPU_VIRTUAL_ADDRESS start;
    D3D12_GPU_VIRTUAL_ADDRESS end;
    bool operator<(const GPUVARegion& other) const { return start < other.start; }
};

class GPUVAManager {
public:
    static GPUVAManager& Get();

    /**
     * Allocates a GPU virtual address for a D3D11 resource
     * @param resource The D3D11 resource to allocate VA for, or nullptr for pre-allocation
     * @param dimension Resource dimension (buffer or texture)
     * @param size Size of the resource in bytes
     * @return Allocated GPU virtual address, or 0 on failure
     * @note If resource is provided, it must not already have a VA allocated
     * @note Size must be non-zero
     */
    D3D12_GPU_VIRTUAL_ADDRESS AllocateVirtualAddress(
        ID3D11Resource* resource,
        D3D12_RESOURCE_DIMENSION dimension,
        uint64_t size);

    /**
     * Frees a previously allocated GPU virtual address
     * @param resource The D3D11 resource whose VA should be freed
     * @note Resource must not be null
     * @note It is safe to call this multiple times on the same resource
     */
    void FreeVirtualAddress(ID3D11Resource* resource);

private:
    GPUVAManager();
    ~GPUVAManager() = default;

    GPUVAManager(const GPUVAManager&) = delete;
    GPUVAManager& operator=(const GPUVAManager&) = delete;

    uint64_t AlignSize(uint64_t size, D3D12_RESOURCE_DIMENSION dimension);
    D3D12_GPU_VIRTUAL_ADDRESS FindFreeRegion(uint64_t size, D3D12_RESOURCE_DIMENSION dimension);
    void AddUsedRegion(D3D12_GPU_VIRTUAL_ADDRESS start, uint64_t size);
    void RemoveUsedRegion(D3D12_GPU_VIRTUAL_ADDRESS address);

    // Memory regions
    static constexpr D3D12_GPU_VIRTUAL_ADDRESS BUFFER_START = 0x0000000100000000ULL;  // 4GB
    static constexpr D3D12_GPU_VIRTUAL_ADDRESS TEXTURE_START = 0x0000001000000000ULL; // 64GB
    static constexpr D3D12_GPU_VIRTUAL_ADDRESS MAX_ADDRESS = 0x0000004000000000ULL;   // 256GB

    mutable std::mutex m_mutex;
    std::map<ID3D11Resource*, GPUVARegion> m_resourceRegions;
    std::set<GPUVARegion> m_usedRegions;
};

}  // namespace dxiided