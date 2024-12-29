#include "d3d11_impl/gpu_va_manager.hpp"
#include "common/debug.hpp"

namespace dxiided {

GPUVAManager& GPUVAManager::Get() {
    static GPUVAManager instance;
    return instance;
}

uint32_t GPUVAManager::GetTypeIndex(D3D12_RESOURCE_DIMENSION dimension) const {
    switch (dimension) {
        case D3D12_RESOURCE_DIMENSION_BUFFER:
            return 1;
        case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
            return 2;
        case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
            return 3;
        case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
            return 4;
        default:
            return 0;
    }
}

uint64_t GPUVAManager::AlignSize(uint64_t size, D3D12_RESOURCE_DIMENSION dimension) const {
    // Use consistent 64KB alignment for everything
    uint64_t alignment = MINIMUM_ALIGNMENT;  // 64KB
    
    // Round up to the next multiple of alignment
    return (size + alignment - 1) & ~(alignment - 1);
}

D3D12_GPU_VIRTUAL_ADDRESS GPUVAManager::GenerateAddress(uint32_t typeIndex, uint64_t size) {
    // Get base offset for this type
    D3D12_GPU_VIRTUAL_ADDRESS baseOffset = TYPE_OFFSETS[typeIndex];
    
    // Calculate spacing based on aligned size
    uint64_t alignedSize = AlignSize(size, D3D12_RESOURCE_DIMENSION_BUFFER);
    
    // Get next counter value - ensure ascending order
    uint64_t index = m_typeCounters[typeIndex].fetch_add(1);
    
    // Always generate addresses in ascending order within each type block
    // Use index * alignedSize to ensure proper spacing based on resource size
    return baseOffset + (index * alignedSize);
}

D3D12_GPU_VIRTUAL_ADDRESS GPUVAManager::AllocateVirtualAddress(
    ID3D11Resource* resource,
    D3D12_RESOURCE_DIMENSION dimension,
    uint64_t size) {
    
    std::lock_guard<std::mutex> lock(m_mutex);

    // If resource is provided, check if it already has an address
    if (resource) {
        auto it = m_resourceMap.find(resource);
        if (it != m_resourceMap.end()) {
            WARN("Resource %p already has virtual address %p", 
                 resource, (void*)it->second.address);
            return it->second.address;
        }
    }

    // Generate new address
    uint32_t typeIndex = GetTypeIndex(dimension);
    uint64_t alignedSize = AlignSize(size, dimension);
    D3D12_GPU_VIRTUAL_ADDRESS address = GenerateAddress(typeIndex, alignedSize);

    // Only store in map if resource is provided
    if (resource) {
        ResourceInfo info = {
            .address = address,
            .size = alignedSize,
            .dimension = dimension
        };
        m_resourceMap[resource] = info;
        TRACE("Allocated virtual address %p for resource %p", (void*)address, resource);
    } else {
        TRACE("Pre-allocated virtual address %p for future resource", (void*)address);
    }

    return address;
}

void GPUVAManager::FreeVirtualAddress(ID3D11Resource* resource) {
    if (!resource) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_resourceMap.find(resource);
    if (it != m_resourceMap.end()) {
        TRACE("Freed virtual address %p for resource %p", 
              (void*)it->second.address, resource);
        m_resourceMap.erase(it);
    }
}

D3D12_GPU_VIRTUAL_ADDRESS GPUVAManager::GetVirtualAddress(ID3D11Resource* resource) const {
    if (!resource) return 0;

    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_resourceMap.find(resource);
    if (it != m_resourceMap.end()) {
        return it->second.address;
    }

    WARN("No virtual address found for resource %p", resource);
    return 0;
}

bool GPUVAManager::IsValidAddress(D3D12_GPU_VIRTUAL_ADDRESS address) const {
    if (address < BASE_ADDRESS) return false;

    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (const auto& [resource, info] : m_resourceMap) {
        if (info.address == address) return true;
    }

    return false;
}

}  // namespace dxiided
