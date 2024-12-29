#pragma once

#include <d3d12.h>
#include <unordered_map>
#include <windows.h>

namespace dxiided {

class GPUVirtualAddressManager {
public:
    static GPUVirtualAddressManager& Get();

    D3D12_GPU_VIRTUAL_ADDRESS Allocate(const D3D12_RESOURCE_DESC* pDesc);
    void Free(D3D12_GPU_VIRTUAL_ADDRESS address);

private:
    GPUVirtualAddressManager() = default;
    ~GPUVirtualAddressManager() = default;
    
    GPUVirtualAddressManager(const GPUVirtualAddressManager&) = delete;
    GPUVirtualAddressManager& operator=(const GPUVirtualAddressManager&) = delete;

    size_t CalculateSize(const D3D12_RESOURCE_DESC* pDesc);
    size_t GetAlignment(const D3D12_RESOURCE_DESC* pDesc);
    D3D12_GPU_VIRTUAL_ADDRESS AllocateInternal(size_t size, size_t alignment);

    std::unordered_map<D3D12_GPU_VIRTUAL_ADDRESS, size_t> m_allocations;
};

}  // namespace dxiided
