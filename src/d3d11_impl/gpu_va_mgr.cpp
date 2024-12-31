// src/d3d11_impl/gpu_va_mgr.cpp
#include "d3d11_impl/gpu_va_mgr.hpp"
#include <algorithm>

namespace dxiided {

GPUVirtualAddressManager::GPUVirtualAddressManager() {
    // Initialize with one large free range
    AddressRange initial = {
        BASE_ADDRESS,
        0xFFFFFFFFFFFF0000ull,
        0xFFFFFFFFFFFF0000ull - BASE_ADDRESS,
        true
    };
    m_addressRanges.push_back(initial);
}

GPUVirtualAddressManager::~GPUVirtualAddressManager() {
    // Release all D3D11 resources
    m_resourceMap.clear();
}

D3D12_GPU_VIRTUAL_ADDRESS GPUVirtualAddressManager::AllocateGPUVA(
    const D3D12_RESOURCE_DESC* pDesc,
    const D3D12_HEAP_PROPERTIES* pHeapProperties) {
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    UINT64 alignment = GetRequiredAlignment(pDesc, pHeapProperties);
    SIZE_T size = GetResourceSize(pDesc);
    
    // Ensure proper alignment
    size = (size + alignment - 1) & ~(alignment - 1);
    
    D3D12_GPU_VIRTUAL_ADDRESS address = AllocateAlignedAddress(size, alignment);
    if (address == GPU_VA_NULL) {
        ERR("GVA: Failed to allocate aligned address of size %llu with alignment %llu", size, alignment);
        return GPU_VA_INVALID;
    }

    // Validate the address is not in reserved ranges
    for (const auto& range : m_reservedRanges) {
        if (address >= range.first && address <= range.second) {
            ERR("GVA: Allocated address %llx falls within reserved range [%llx-%llx]", 
                address, range.first, range.second);
            return GPU_VA_INVALID;
        }
    }

    // Create temporary resource info without D3D11 resource
    ResourceInfo info = {
        pDesc->Dimension,
        pDesc->Flags,
        pHeapProperties->Type,
        size,
        alignment,
        IsConstantBuffer(pDesc),
        (pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) != 0,
        nullptr
    };
    
    m_resourceMap[address] = info;
    TRACE("GVA: Successfully allocated address %llx of size %llu", address, size);
    return address;
}

void GPUVirtualAddressManager::FreeGPUVA(D3D12_GPU_VIRTUAL_ADDRESS address) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_resourceMap.find(address);
    if (it == m_resourceMap.end()) {
        return;
    }

    const auto& info = it->second;
    
    // Find and free the address range
    for (auto& range : m_addressRanges) {
        if (range.Start == address) {
            range.IsFree = true;
            break;
        }
    }

    m_resourceMap.erase(it);
    CoalesceRanges();
}

bool GPUVirtualAddressManager::RegisterResource(
    D3D12_GPU_VIRTUAL_ADDRESS address,
    ID3D11Resource* resource,
    const D3D12_RESOURCE_DESC* pDesc,
    const D3D12_HEAP_PROPERTIES* pHeapProperties) {
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_resourceMap.find(address);
    if (it == m_resourceMap.end()) {
        ERR("GVA: Failed to register resource - address %llx not found in resource map", address);
        return false;
    }

    if (!resource) {
        ERR("GVA: Failed to register resource - null D3D11 resource provided");
        return false;
    }

    it->second.D3D11Resource = resource;
    TRACE("GVA: Successfully registered D3D11 resource at address %llx", address);
    return true;
}

ID3D11Resource* GPUVirtualAddressManager::GetD3D11Resource(
    D3D12_GPU_VIRTUAL_ADDRESS address) {
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_resourceMap.find(address);
    if (it == m_resourceMap.end()) {
        return nullptr;
    }

    return it->second.D3D11Resource.Get();
}

bool GPUVirtualAddressManager::ValidateAddress(D3D12_GPU_VIRTUAL_ADDRESS address) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if address is null or invalid
    if (address == GPU_VA_NULL || address == GPU_VA_INVALID) {
        return false;
    }
    
    // Check if address is in reserved ranges
    for (const auto& range : m_reservedRanges) {
        if (address >= range.first && address <= range.second) {
            ERR("GVA: Address %llx falls within reserved range [%llx-%llx]", 
                address, range.first, range.second);
            return false;
        }
    }
    
    // Check if address exists in resource map
    auto it = m_resourceMap.find(address);
    if (it == m_resourceMap.end()) {
        ERR("GVA: Address %llx not found in resource map", address);
        return false;
    }
    
    return true;
}

UINT64 GPUVirtualAddressManager::GetRequiredAlignment(
    const D3D12_RESOURCE_DESC* pDesc,
    const D3D12_HEAP_PROPERTIES* pHeapProperties) {
    
    if (pDesc->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
        if (pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) {
            return UAV_COUNTER_ALIGNMENT;
        }
        
        if (IsConstantBuffer(pDesc)) {
            return CONSTANT_BUFFER_ALIGNMENT;
        }
    }
    
    if (pDesc->Dimension != D3D12_RESOURCE_DIMENSION_BUFFER) {
        return TEXTURE_DATA_ALIGNMENT;
    }
    
    if (pHeapProperties->Type == D3D12_HEAP_TYPE_UPLOAD ||
        pHeapProperties->Type == D3D12_HEAP_TYPE_READBACK) {
        return SMALL_RESOURCE_ALIGNMENT;
    }
    
    return DEFAULT_RESOURCE_ALIGNMENT;
}

bool GPUVirtualAddressManager::IsSafeTruncatedAddress(D3D12_GPU_VIRTUAL_ADDRESS addr, size_t size) {
    // Get the lower 32 bits of the address range
    D3D12_GPU_VIRTUAL_ADDRESS truncStart = addr & 0xFFFFFFFFull;
    D3D12_GPU_VIRTUAL_ADDRESS truncEnd = (addr + size - 1) & 0xFFFFFFFFull;
    
    // Check if any existing allocation would overlap with these truncated addresses
    for (const auto& range : m_addressRanges) {
        D3D12_GPU_VIRTUAL_ADDRESS existingTruncStart = range.Start & 0xFFFFFFFFull;
        D3D12_GPU_VIRTUAL_ADDRESS existingTruncEnd = (range.Start + range.Size - 1) & 0xFFFFFFFFull;
        
        // Check for overlap in truncated space
        if (!(truncEnd < existingTruncStart || truncStart > existingTruncEnd)) {
            return false;
        }
    }
    return true;
}

D3D12_GPU_VIRTUAL_ADDRESS GPUVirtualAddressManager::AllocateAlignedAddress(
    SIZE_T size, UINT64 alignment) {
    
    // First try: find an address that's safe when truncated
    for (auto it = m_addressRanges.begin(); it != m_addressRanges.end(); ++it) {
        if (!it->IsFree) {
            continue;
        }

        // Align the start address
        D3D12_GPU_VIRTUAL_ADDRESS alignedStart = 
            (it->Start + alignment - 1) & ~(alignment - 1);
            
        if (alignedStart >= it->End) {
            continue;
        }

        SIZE_T remainingSize = it->End - alignedStart;
        if (remainingSize < size) {
            continue;
        }

        // Check if this address would be safe when truncated
        if (!IsSafeTruncatedAddress(alignedStart, size)) {
            continue;
        }

        // Split the range if necessary
        if (alignedStart > it->Start) {
            AddressRange prefix = {
                it->Start,
                alignedStart,
                alignedStart - it->Start,
                true
            };
            m_addressRanges.insert(it, prefix);
        }

        it->Start = alignedStart;
        it->Size = size;
        it->IsFree = false;

        if (alignedStart + size < it->End) {
            AddressRange suffix = {
                alignedStart + size,
                it->End,
                it->End - (alignedStart + size),
                true
            };
            m_addressRanges.insert(std::next(it), suffix);
            it->End = alignedStart + size;
        }

        return alignedStart;
    }

    // Second try: find any free address if we couldn't find a safe truncated one
    // This is needed because some games may work fine with truncated addresses
    for (auto it = m_addressRanges.begin(); it != m_addressRanges.end(); ++it) {
        if (!it->IsFree) {
            continue;
        }

        D3D12_GPU_VIRTUAL_ADDRESS alignedStart = 
            (it->Start + alignment - 1) & ~(alignment - 1);
            
        if (alignedStart >= it->End) {
            continue;
        }

        SIZE_T remainingSize = it->End - alignedStart;
        if (remainingSize < size) {
            continue;
        }

        // Split and allocate the range
        if (alignedStart > it->Start) {
            AddressRange prefix = {
                it->Start,
                alignedStart,
                alignedStart - it->Start,
                true
            };
            m_addressRanges.insert(it, prefix);
        }

        it->Start = alignedStart;
        it->Size = size;
        it->IsFree = false;

        if (alignedStart + size < it->End) {
            AddressRange suffix = {
                alignedStart + size,
                it->End,
                it->End - (alignedStart + size),
                true
            };
            m_addressRanges.insert(std::next(it), suffix);
            it->End = alignedStart + size;
        }

        WARN("GVA: Allocated address %llx that may be unsafe when truncated", alignedStart);
        return alignedStart;
    }

    return GPU_VA_NULL;
}

void GPUVirtualAddressManager::CoalesceRanges() {
    auto it = m_addressRanges.begin();
    while (it != m_addressRanges.end()) {
        auto next = std::next(it);
        if (next == m_addressRanges.end()) {
            break;
        }

        if (it->IsFree && next->IsFree && it->End == next->Start) {
            it->End = next->End;
            it->Size += next->Size;
            m_addressRanges.erase(next);
        } else {
            ++it;
        }
    }
}

bool GPUVirtualAddressManager::IsConstantBuffer(const D3D12_RESOURCE_DESC* pDesc) {
    return pDesc->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER &&
           (pDesc->Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) == 0 &&
           (pDesc->Width % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) == 0;
}

SIZE_T GPUVirtualAddressManager::GetResourceSize(const D3D12_RESOURCE_DESC* pDesc) {
    if (pDesc->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
        return pDesc->Width;
    }
    
    // For textures, calculate total size including all mips and array slices
    SIZE_T size = 0;
    UINT numSubresources = pDesc->MipLevels * pDesc->DepthOrArraySize;
    // This is a simplified calculation - real implementation would need proper texture size calculation
    size = pDesc->Width * pDesc->Height * pDesc->DepthOrArraySize;
    return size;
}

void GPUVirtualAddressManager::DumpAddressMap() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    TRACE("=== GPU Virtual Address Map ===");
    for (const auto& range : m_addressRanges) {
        TRACE("Range: 0x%llx - 0x%llx, Size: %llu, %s",
              range.Start, range.End, range.Size,
              range.IsFree ? "Free" : "Allocated");
    }
    
    TRACE("=== Resource Map ===");
    for (const auto& [addr, info] : m_resourceMap) {
        TRACE("Address: 0x%llx, Size: %llu, Type: %d, Has D3D11: %d",
              addr, info.Size, info.HeapType,
              info.D3D11Resource != nullptr);
    }
}

bool GPUVirtualAddressManager::FindAddressByLowerBits(D3D12_GPU_VIRTUAL_ADDRESS truncated, 
                                                     D3D12_GPU_VIRTUAL_ADDRESS& outFullAddress) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Mask to get lower 32 bits
    const D3D12_GPU_VIRTUAL_ADDRESS LOWER_MASK = 0xFFFFFFFFull;
    
    // Search through resource map for an address with matching lower bits
    for (const auto& entry : m_resourceMap) {
        if ((entry.first & LOWER_MASK) == truncated) {
            outFullAddress = entry.first;
            return true;
        }
    }
    
    return false;
}

} // namespace dxiided