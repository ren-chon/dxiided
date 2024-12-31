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

    // Remove lower 32-bit tracking
    uint32_t truncStart = static_cast<uint32_t>(address & 0xFFFFFFFFull);
    uint32_t truncEnd = static_cast<uint32_t>((address + it->second.Size - 1) & 0xFFFFFFFFull);
    
    for (uint32_t curr = truncStart; curr <= truncEnd; curr += 0x1000) {
        m_usedLower32Bits.erase(curr & ~0xFFFu);
    }
    
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
    // Check if any page in the range conflicts with existing allocations
    uint32_t startPage = static_cast<uint32_t>(addr & 0xFFFFFFFFllu) & ~0xFFFu;
    uint32_t endPage = static_cast<uint32_t>((addr + size - 1) & 0xFFFFFFFFllu) & ~0xFFFu;
    
    for (uint32_t page = startPage; page <= endPage; page += 0x1000) {
        if (m_usedLower32Bits.find(page) != m_usedLower32Bits.end()) {
            return false;
        }
    }
    return true;
}

D3D12_GPU_VIRTUAL_ADDRESS GPUVirtualAddressManager::AllocateAlignedAddress(
    SIZE_T size, UINT64 alignment) {
    
    TRACE("GVA: Attempting to allocate size %llu with alignment %llu", size, alignment);
    
    // Only allocate in the lower 32-bit address space
    for (auto it = m_addressRanges.begin(); it != m_addressRanges.end(); ++it) {
        if (!it->IsFree || it->Start >= (1ull << 32)) {
            continue;
        }

        D3D12_GPU_VIRTUAL_ADDRESS rangeStart = it->Start;
        D3D12_GPU_VIRTUAL_ADDRESS rangeEnd = std::min(it->End, (1ull << 32) - 1);
        
        // Align the start address
        D3D12_GPU_VIRTUAL_ADDRESS alignedStart = (rangeStart + alignment - 1) & ~(alignment - 1);
        
        if (alignedStart < rangeEnd && (rangeEnd - alignedStart) >= size) {
            // Split and allocate
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

            TRACE("GVA: Successfully allocated address %llx of size %llu", alignedStart, size);
            return alignedStart;
        }
    }
    
    ERR("GVA: Failed to allocate address in 32-bit range, size %llu", size);
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
    
    TRACE("=== GPUVA: GPU Virtual Address Map ===");
    for (const auto& range : m_addressRanges) {
        TRACE("Range [%llx-%llx] Size: %llu %s",
              range.Start, range.End, range.Size,
              range.IsFree ? "FREE" : "USED");
    }
    
    TRACE("=== GPUVA: Used Lower 32-bit Pages ===");
    for (uint32_t page : m_usedLower32Bits) {
        TRACE("Page: %08x", page);
    }
    TRACE("===========================");
}

bool GPUVirtualAddressManager::FindAddressByLowerBits(D3D12_GPU_VIRTUAL_ADDRESS truncated, 
                                                     D3D12_GPU_VIRTUAL_ADDRESS& outFullAddress) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Mask to get lower 32 bits
    const D3D12_GPU_VIRTUAL_ADDRESS LOWER_MASK = 0xFFFFFFFFull;
    
    // First try to find an exact match in active resources
    for (const auto& entry : m_resourceMap) {
        if ((entry.first & LOWER_MASK) == truncated && entry.second.D3D11Resource) {
            outFullAddress = entry.first;
            TRACE("GVA: Found exact match for truncated address %llx -> %llx", truncated, outFullAddress);
            return true;
        }
    }
    
    // If no active resource found, try any allocated address
    for (const auto& entry : m_resourceMap) {
        if ((entry.first & LOWER_MASK) == truncated) {
            outFullAddress = entry.first;
            TRACE("GVA: Found potential match for truncated address %llx -> %llx", truncated, outFullAddress);
            return true;
        }
    }
    
    ERR("GVA: Failed to recover truncated address %llx", truncated);
    return false;
}

} // namespace dxiided