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
            return 0;  // Unknown/Default
    }
}

uint64_t GPUVAManager::AlignSize(uint64_t size) const {
    // Align to SIMD boundary for vector operations
    return (size + SIMD_ALIGNMENT - 1) & ~(SIMD_ALIGNMENT - 1);
}

D3D12_GPU_VIRTUAL_ADDRESS GPUVAManager::GenerateAddress(uint32_t typeIndex, uint64_t size) {
    // Get unique offset for this resource type
    uint64_t uniqueOffset = m_typeCounters[typeIndex].fetch_add(1);
    
    // Calculate aligned size for proper resource spacing
    uint64_t alignedSize = AlignSize(size);
    
    // Generate final address: BASE + TYPE_OFFSET + (UNIQUE_INDEX * ALIGNED_SIZE)
    D3D12_GPU_VIRTUAL_ADDRESS address = BASE_ADDRESS + 
                                      TYPE_OFFSETS[typeIndex] + 
                                      (uniqueOffset * alignedSize);
    
    TRACE("Generated virtual address %p for type %u with size %llu", 
          (void*)address, typeIndex, size);
    
    return address;
}

D3D12_GPU_VIRTUAL_ADDRESS GPUVAManager::AllocateVirtualAddress(
    ID3D11Resource* resource,
    D3D12_RESOURCE_DIMENSION dimension,
    uint64_t size) {
    
    if (!resource) {
        ERR("Attempting to allocate virtual address for null resource");
        return 0;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if resource already has an address
    auto it = m_resourceMap.find(resource);
    if (it != m_resourceMap.end()) {
        WARN("Resource %p already has virtual address %p", 
             resource, (void*)it->second.address);
        return it->second.address;
    }

    // Generate new address
    uint32_t typeIndex = GetTypeIndex(dimension);
    D3D12_GPU_VIRTUAL_ADDRESS address = GenerateAddress(typeIndex, size);

    // Store resource information
    ResourceInfo info = {
        .address = address,
        .size = size,
        .dimension = dimension
    };
    m_resourceMap[resource] = info;

    TRACE("Allocated virtual address %p for resource %p", (void*)address, resource);
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
    
    // Check if address belongs to any known resource
    for (const auto& [resource, info] : m_resourceMap) {
        if (info.address == address) return true;
    }

    return false;
}

}  // namespace dxiided
