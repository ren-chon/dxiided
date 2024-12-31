#pragma once

#include <d3d11.h>
#include <d3d12.h>
#include <wrl/client.h>

#include <list>
#include <map>
#include <mutex>
#include <vector>

#include "common/debug.hpp"

namespace dxiided {

typedef UINT64 D3D12_GPU_VIRTUAL_ADDRESS;

class GPUVirtualAddressManager {
   public:
    static constexpr D3D12_GPU_VIRTUAL_ADDRESS GPU_VA_NULL = 0;
    static constexpr D3D12_GPU_VIRTUAL_ADDRESS GPU_VA_INVALID = ~0ull;
    static constexpr UINT64 DEFAULT_RESOURCE_ALIGNMENT = 64 * 1024;  // 64KB
    static constexpr UINT64 SMALL_RESOURCE_ALIGNMENT = 4 * 1024;     // 4KB
    static constexpr UINT64 CONSTANT_BUFFER_ALIGNMENT = 256;         // 256B
    static constexpr UINT64 TEXTURE_DATA_ALIGNMENT = 512;            // 512B
    static constexpr UINT64 UAV_COUNTER_ALIGNMENT = 4096;            // 4KB

    // Singleton access
    static GPUVirtualAddressManager& Get() {
        static GPUVirtualAddressManager instance;
        return instance;
    }

    // Delete copy constructor and assignment operator
    GPUVirtualAddressManager(const GPUVirtualAddressManager&) = delete;
    GPUVirtualAddressManager& operator=(const GPUVirtualAddressManager&) = delete;

    D3D12_GPU_VIRTUAL_ADDRESS AllocateGPUVA(
        const D3D12_RESOURCE_DESC* pDesc,
        const D3D12_HEAP_PROPERTIES* pHeapProperties);

    void FreeGPUVA(D3D12_GPU_VIRTUAL_ADDRESS address);

    bool RegisterResource(D3D12_GPU_VIRTUAL_ADDRESS address,
                          ID3D11Resource* resource,
                          const D3D12_RESOURCE_DESC* pDesc,
                          const D3D12_HEAP_PROPERTIES* pHeapProperties);

    ID3D11Resource* GetD3D11Resource(D3D12_GPU_VIRTUAL_ADDRESS address);

    bool ValidateAddress(D3D12_GPU_VIRTUAL_ADDRESS address);

    // Find full GPU virtual address given truncated 32-bit version
    bool FindAddressByLowerBits(D3D12_GPU_VIRTUAL_ADDRESS truncated,
                               D3D12_GPU_VIRTUAL_ADDRESS& outFullAddress);
    
    bool IsSafeTruncatedAddress(D3D12_GPU_VIRTUAL_ADDRESS addr, size_t size);
    // Debug helpers
    void DumpAddressMap();

   private:
    // Private constructor for singleton
    GPUVirtualAddressManager();
    ~GPUVirtualAddressManager();

    struct ResourceInfo {
        D3D12_RESOURCE_DIMENSION Dimension;
        D3D12_RESOURCE_FLAGS Flags;
        D3D12_HEAP_TYPE HeapType;
        SIZE_T Size;
        UINT64 Alignment;
        bool IsConstantBuffer;
        bool IsUAV;
        Microsoft::WRL::ComPtr<ID3D11Resource> D3D11Resource;
    };

    struct AddressRange {
        D3D12_GPU_VIRTUAL_ADDRESS Start;
        D3D12_GPU_VIRTUAL_ADDRESS End;
        SIZE_T Size;
        bool IsFree;
    };

    UINT64 GetRequiredAlignment(const D3D12_RESOURCE_DESC* pDesc,
                                const D3D12_HEAP_PROPERTIES* pHeapProperties);

    D3D12_GPU_VIRTUAL_ADDRESS AllocateAlignedAddress(SIZE_T size,
                                                     UINT64 alignment);
    void CoalesceRanges();
    bool IsConstantBuffer(const D3D12_RESOURCE_DESC* pDesc);
    SIZE_T GetResourceSize(const D3D12_RESOURCE_DESC* pDesc);

    std::map<D3D12_GPU_VIRTUAL_ADDRESS, ResourceInfo> m_resourceMap;
    std::list<AddressRange> m_addressRanges;
    std::mutex m_mutex;

    const std::vector<
        std::pair<D3D12_GPU_VIRTUAL_ADDRESS, D3D12_GPU_VIRTUAL_ADDRESS>>
        m_reservedRanges = {{0x0000000000000000ull, 0x0000000000000FFFull},
                            {0xFFFFFFFFFFFF0000ull, 0xFFFFFFFFFFFFFFFFull}};

    // Base address for allocations
    static constexpr D3D12_GPU_VIRTUAL_ADDRESS BASE_ADDRESS = 0x100000000ull;
};
}  // namespace dxiided
