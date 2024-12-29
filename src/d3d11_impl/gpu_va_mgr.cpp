#include "d3d11_impl/gpu_va_mgr.hpp"
#include "common/debug.hpp"

namespace dxiided {

namespace {
    // Increased number of fallback addresses with better spacing and more options
    constexpr D3D12_GPU_VIRTUAL_ADDRESS BASE_ADDRESSES[] = {
        0x20000000,   // 512MB offset (primary)
        0x40000000,   // 1GB offset
        0x80000000,   // 2GB offset
        0x100000000,  // 4GB offset
        0x200000000,  // 8GB offset
        0x400000000,  // 16GB offset
        0x5c0010000,  // Death Stranding specific fallback
        0x600000000,  // 24GB offset
        0x800000000,  // 32GB offset
        0xa00000000,  // 40GB offset
        0xc00000000,  // 48GB offset
        0x1000000000  // 64GB offset (last resort)
    };
    constexpr size_t NUM_BASE_ADDRESSES = sizeof(BASE_ADDRESSES) / sizeof(BASE_ADDRESSES[0]);
    
    // More granular fallback sizes
    constexpr size_t FALLBACK_SIZES[] = {
        8ULL * 1024 * 1024 * 1024,  // 8GB
        4ULL * 1024 * 1024 * 1024,  // 4GB
        2ULL * 1024 * 1024 * 1024,  // 2GB
        1ULL * 1024 * 1024 * 1024,  // 1GB
        512ULL * 1024 * 1024,       // 512MB
        256ULL * 1024 * 1024,       // 256MB
        128ULL * 1024 * 1024,       // 128MB
        64ULL * 1024 * 1024,        // 64MB
        32ULL * 1024 * 1024,        // 32MB
        16ULL * 1024 * 1024         // 16MB minimum
    };
    constexpr size_t NUM_FALLBACK_SIZES = sizeof(FALLBACK_SIZES) / sizeof(FALLBACK_SIZES[0]);
    
    // Alignment options from most to least restrictive
    constexpr size_t ALIGNMENTS[] = {
        256ULL * 1024 * 1024,  // 256MB
        64ULL * 1024 * 1024,   // 64MB
        16ULL * 1024 * 1024,   // 16MB
        4ULL * 1024 * 1024,    // 4MB
        1ULL * 1024 * 1024,    // 1MB
        256ULL * 1024,         // 256KB
        64ULL * 1024,          // 64KB (minimum)
    };
    constexpr size_t NUM_ALIGNMENTS = sizeof(ALIGNMENTS) / sizeof(ALIGNMENTS[0]);
}

GPUVirtualAddressManager& GPUVirtualAddressManager::Get() {
    static GPUVirtualAddressManager instance;
    return instance;
}

D3D12_GPU_VIRTUAL_ADDRESS GPUVirtualAddressManager::Allocate(
    const D3D12_RESOURCE_DESC* pDesc) {
    size_t size = CalculateSize(pDesc);
    size_t alignment = GetAlignment(pDesc);
    return AllocateInternal(size, alignment);
}

void GPUVirtualAddressManager::Free(D3D12_GPU_VIRTUAL_ADDRESS address) {
    auto it = m_allocations.find(address);
    if (it != m_allocations.end()) {
        VirtualFree((LPVOID)address, 0, MEM_RELEASE);
        m_allocations.erase(it);
    }
}

size_t GPUVirtualAddressManager::CalculateSize(const D3D12_RESOURCE_DESC* pDesc) {
    size_t size = 0;
    
    switch (pDesc->Dimension) {
        case D3D12_RESOURCE_DIMENSION_BUFFER:
            size = pDesc->Width;
            break;
            
        case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
            size = pDesc->Width * pDesc->DepthOrArraySize * pDesc->MipLevels;
            break;
            
        case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
            size = pDesc->Width * pDesc->Height * pDesc->DepthOrArraySize * 
                   pDesc->MipLevels;
            break;
            
        case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
            size = pDesc->Width * pDesc->Height * pDesc->DepthOrArraySize * 
                   pDesc->MipLevels;
            break;
            
        default:
            WARN("Unknown resource dimension: %d", pDesc->Dimension);
            size = pDesc->Width;  // fallback to minimum
    }
    
    return size;
}

size_t GPUVirtualAddressManager::GetAlignment(const D3D12_RESOURCE_DESC* pDesc) {
    size_t alignment = ALIGNMENTS[NUM_ALIGNMENTS - 1];  // Start with minimum 64KB alignment
    
    switch (pDesc->Dimension) {
        case D3D12_RESOURCE_DIMENSION_BUFFER:
            alignment = std::max(alignment, static_cast<size_t>(256));  // D3D12 buffer alignment
            break;
            
        case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
        case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
        case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
            alignment = std::max(alignment, static_cast<size_t>(512));  // D3D12 texture alignment
            break;
            
        default:
            WARN("Unknown resource dimension: %d", pDesc->Dimension);
    }
    
    return alignment;
}

D3D12_GPU_VIRTUAL_ADDRESS GPUVirtualAddressManager::AllocateInternal(
    size_t size, size_t alignment) {
    static bool initialized = false;
    static D3D12_GPU_VIRTUAL_ADDRESS base_address = 0;
    static D3D12_GPU_VIRTUAL_ADDRESS next_address = 0;
    
    // Validate inputs
    if (size == 0) {
        WARN("Attempted to allocate zero bytes");
        return 0;
    }
    
    // Ensure minimum alignment
    alignment = std::max(alignment, ALIGNMENTS[NUM_ALIGNMENTS - 1]);
    
    // Round up size to alignment
    size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);
    
    if (!initialized) {
        // Try each base address with different block sizes
        for (size_t i = 0; i < NUM_BASE_ADDRESSES; i++) {
            for (size_t j = 0; j < NUM_FALLBACK_SIZES; j++) {
                size_t block_size = FALLBACK_SIZES[j];
                if (block_size < aligned_size) {
                    continue;  // Skip if block size is too small
                }
                
                // Try different alignments for each base address
                for (size_t k = 0; k < NUM_ALIGNMENTS; k++) {
                    size_t curr_alignment = std::max(ALIGNMENTS[k], alignment);
                    
                    // Ensure base address alignment
                    D3D12_GPU_VIRTUAL_ADDRESS aligned_base = 
                        (BASE_ADDRESSES[i] + curr_alignment - 1) & ~(curr_alignment - 1);
                    
                    // Validate aligned base is within reasonable range
                    if (aligned_base < BASE_ADDRESSES[i] || 
                        aligned_base >= BASE_ADDRESSES[i] + curr_alignment) {
                        WARN("Invalid aligned base address %p from base %p with alignment %zu",
                             (void*)aligned_base, (void*)BASE_ADDRESSES[i], curr_alignment);
                        continue;
                    }
                    
                    void* addr = VirtualAlloc((LPVOID)aligned_base, block_size,
                                            MEM_RESERVE, PAGE_NOACCESS);
                    if (addr) {
                        base_address = (D3D12_GPU_VIRTUAL_ADDRESS)addr;
                        next_address = base_address;
                        initialized = true;
                        TRACE("Reserved %zuGB GPU VA block at %p with alignment %zu",
                              block_size >> 30, addr, curr_alignment);
                        goto allocation;
                    }
                    
                    DWORD error = GetLastError();
                    if (error != ERROR_INVALID_ADDRESS && error != ERROR_COMMITMENT_LIMIT) {
                        WARN("Failed to reserve %zuGB GPU VA block at %p, unexpected error: %lu",
                             block_size >> 30, (void*)aligned_base, error);
                    }
                }
            }
        }
        
        // If all base addresses failed, try system-chosen address
        for (size_t j = 0; j < NUM_FALLBACK_SIZES; j++) {
            size_t block_size = FALLBACK_SIZES[j];
            if (block_size < aligned_size) {
                continue;
            }
            
            // Try different alignments
            for (size_t k = 0; k < NUM_ALIGNMENTS; k++) {
                size_t curr_alignment = std::max(ALIGNMENTS[k], alignment);
                
                // First try to allocate with extra space for alignment
                void* addr = VirtualAlloc(NULL, block_size + curr_alignment,
                                        MEM_RESERVE, PAGE_NOACCESS);
                if (addr) {
                    // Calculate aligned address
                    void* aligned_addr = (void*)(((uintptr_t)addr + curr_alignment - 1) 
                                               & ~(curr_alignment - 1));
                    
                    // Validate aligned address
                    if ((uintptr_t)aligned_addr < (uintptr_t)addr || 
                        (uintptr_t)aligned_addr >= (uintptr_t)addr + curr_alignment) {
                        WARN("Invalid aligned address %p from base %p with alignment %zu",
                             aligned_addr, addr, curr_alignment);
                        VirtualFree(addr, 0, MEM_RELEASE);
                        continue;
                    }
                    
                    // Release the original allocation
                    VirtualFree(addr, 0, MEM_RELEASE);
                    
                    // Try to allocate at the aligned address
                    addr = VirtualAlloc(aligned_addr, block_size,
                                      MEM_RESERVE, PAGE_NOACCESS);
                    if (addr) {
                        base_address = (D3D12_GPU_VIRTUAL_ADDRESS)addr;
                        next_address = base_address;
                        initialized = true;
                        TRACE("Reserved %zuGB GPU VA block at system-chosen address %p with alignment %zu",
                              block_size >> 30, addr, curr_alignment);
                        goto allocation;
                    }
                }
            }
        }
        
        WARN("Failed to reserve any GPU VA block");
        return 0;
    }
    
allocation:
    // Align the next address
    D3D12_GPU_VIRTUAL_ADDRESS aligned_addr = (next_address + alignment - 1) & ~(alignment - 1);
    
    // Validate aligned address
    if (aligned_addr < base_address || aligned_addr > base_address + FALLBACK_SIZES[0] - aligned_size) {
        WARN("Invalid aligned address %p outside block range [%p, %p]",
             (void*)aligned_addr, (void*)base_address, 
             (void*)(base_address + FALLBACK_SIZES[0] - aligned_size));
             
        // Try to allocate a new block
        for (size_t j = 0; j < NUM_FALLBACK_SIZES; j++) {
            size_t block_size = FALLBACK_SIZES[j];
            if (block_size < aligned_size) {
                continue;
            }
            
            // Try different alignments
            for (size_t k = 0; k < NUM_ALIGNMENTS; k++) {
                size_t curr_alignment = std::max(ALIGNMENTS[k], alignment);
                
                void* addr = VirtualAlloc(NULL, block_size + curr_alignment,
                                        MEM_RESERVE, PAGE_NOACCESS);
                if (addr) {
                    // Calculate aligned address
                    void* aligned_addr = (void*)(((uintptr_t)addr + curr_alignment - 1)
                                               & ~(curr_alignment - 1));
                    
                    // Validate aligned address
                    if ((uintptr_t)aligned_addr < (uintptr_t)addr || 
                        (uintptr_t)aligned_addr >= (uintptr_t)addr + curr_alignment) {
                        WARN("Invalid aligned address %p from base %p with alignment %zu",
                             aligned_addr, addr, curr_alignment);
                        VirtualFree(addr, 0, MEM_RELEASE);
                        continue;
                    }
                    
                    // Release the original allocation
                    VirtualFree(addr, 0, MEM_RELEASE);
                    
                    // Try to allocate at the aligned address
                    addr = VirtualAlloc(aligned_addr, block_size,
                                      MEM_RESERVE, PAGE_NOACCESS);
                    if (addr) {
                        D3D12_GPU_VIRTUAL_ADDRESS new_addr = (D3D12_GPU_VIRTUAL_ADDRESS)addr;
                        next_address = new_addr + aligned_size;
                        m_allocations[new_addr] = aligned_size;
                        TRACE("Allocated new %zuGB block at %p for resource size %zu",
                              block_size >> 30, addr, size);
                        return new_addr;
                    }
                }
            }
        }
        
        WARN("Out of GPU virtual address space");
        return 0;
    }
    
    // Validate the allocation doesn't overlap with existing ones
    for (const auto& alloc : m_allocations) {
        if ((aligned_addr >= alloc.first && aligned_addr < alloc.first + alloc.second) ||
            (aligned_addr + aligned_size > alloc.first && 
             aligned_addr + aligned_size <= alloc.first + alloc.second)) {
            WARN("Allocation [%p, %p] would overlap with existing allocation [%p, %p]",
                 (void*)aligned_addr, (void*)(aligned_addr + aligned_size),
                 (void*)alloc.first, (void*)(alloc.first + alloc.second));
            return 0;
        }
    }
    
    // Use space from current block
    next_address = aligned_addr + aligned_size;
    m_allocations[aligned_addr] = aligned_size;
    TRACE("Allocated %zu bytes at %p from existing block", size, (void*)aligned_addr);
    return aligned_addr;
}

}  // namespace dxiided
