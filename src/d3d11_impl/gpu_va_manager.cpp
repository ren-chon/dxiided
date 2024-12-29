#include "d3d11_impl/gpu_va_manager.hpp"
#include "common/debug.hpp"

namespace dxiided {

GPUVAManager& GPUVAManager::Get() {
    static GPUVAManager instance;
    return instance;
}

GPUVAManager::GPUVAManager() {
    // Initialize with empty used regions
    m_usedRegions.clear();
}

uint64_t GPUVAManager::AlignSize(uint64_t size, D3D12_RESOURCE_DIMENSION dimension) {
    uint64_t alignment;
    
    switch (dimension) {
        case D3D12_RESOURCE_DIMENSION_BUFFER:
            // D3D12 buffer alignment rules
            if (size <= 4096) {
                alignment = 256;  // Small buffers
            } else if (size <= 64 * 1024) {
                alignment = 4096;  // Medium buffers
            } else {
                alignment = 64 * 1024;  // Large buffers
            }
            break;
            
        case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
            alignment = 64 * 1024;  // Textures always 64KB aligned
            break;
            
        default:
            alignment = 4096;  // Default page size alignment
            break;
    }
    
    return (size + alignment - 1) & ~(alignment - 1);
}

D3D12_GPU_VIRTUAL_ADDRESS GPUVAManager::FindFreeRegion(uint64_t size, D3D12_RESOURCE_DIMENSION dimension) {
    D3D12_GPU_VIRTUAL_ADDRESS start = (dimension == D3D12_RESOURCE_DIMENSION_BUFFER) ? BUFFER_START : TEXTURE_START;
    D3D12_GPU_VIRTUAL_ADDRESS current = start;
    
    for (const auto& region : m_usedRegions) {
        if (current + size <= region.start) {
            // Found a gap big enough
            return current;
        }
        current = region.end;
    }
    
    // No gaps found, append to end
    if (current + size <= MAX_ADDRESS) {
        return current;
    }
    
    ERR("Failed to find free GPU VA region of size %llu", size);
    return 0;
}

void GPUVAManager::AddUsedRegion(D3D12_GPU_VIRTUAL_ADDRESS start, uint64_t size) {
    GPUVARegion region{start, start + size};
    m_usedRegions.insert(region);
}

void GPUVAManager::RemoveUsedRegion(D3D12_GPU_VIRTUAL_ADDRESS address) {
    for (auto it = m_usedRegions.begin(); it != m_usedRegions.end(); ++it) {
        if (it->start == address) {
            m_usedRegions.erase(it);
            return;
        }
    }
}

D3D12_GPU_VIRTUAL_ADDRESS GPUVAManager::AllocateVirtualAddress(
    ID3D11Resource* resource,
    D3D12_RESOURCE_DIMENSION dimension,
    uint64_t size) {
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Validate input parameters
    if (size == 0) {
        ERR("Cannot allocate VA of size 0");
        return 0;
    }

    // Check if resource already has VA allocated
    if (resource && m_resourceRegions.find(resource) != m_resourceRegions.end()) {
        ERR("Resource %p already has VA allocated", resource);
        return 0;
    }

    // Align the size according to D3D12 rules
    uint64_t alignedSize = AlignSize(size, dimension);
    
    // Find a free region for this allocation
    D3D12_GPU_VIRTUAL_ADDRESS address = FindFreeRegion(alignedSize, dimension);
    if (address == 0) {
        return 0;
    }
    
    // Mark region as used
    AddUsedRegion(address, alignedSize);
    
    // Store mapping if resource provided
    if (resource) {
        m_resourceRegions[resource] = GPUVARegion{address, address + alignedSize};
        TRACE("Allocated virtual address %p (size %llu) for resource %p", 
              (void*)address, alignedSize, resource);
    } else {
        TRACE("Pre-allocated virtual address %p (size %llu) for future resource", 
              (void*)address, alignedSize);
    }
    
    return address;
}

void GPUVAManager::FreeVirtualAddress(ID3D11Resource* resource) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!resource) {
        ERR("Cannot free VA for null resource");
        return;
    }
    
    auto it = m_resourceRegions.find(resource);
    if (it != m_resourceRegions.end()) {
        RemoveUsedRegion(it->second.start);
        TRACE("Freed virtual address %p for resource %p", 
              (void*)it->second.start, resource);
        m_resourceRegions.erase(it);
    } else {
        WARN("No VA found for resource %p during free", resource);
    }
}

}  // namespace dxiided
