#include "d3d11_impl/gpu_va_mgr.hpp"
#include "common/debug.hpp"

namespace dxiided {
#define D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN ((D3D12_GPU_VIRTUAL_ADDRESS)-1)
#define D3D12_GPU_VIRTUAL_ADDRESS_RESERVE_START ((D3D12_GPU_VIRTUAL_ADDRESS)0x0000100000000000ULL)
GPUVirtualAddressManager::GPUVirtualAddressManager() 
    : next_va_(D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN) {
}

GPUVirtualAddressManager::~GPUVirtualAddressManager() {
}

D3D12_GPU_VIRTUAL_ADDRESS GPUVirtualAddressManager::AllocateGPUVA(SIZE_T size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Align size to required boundary
    size = (size + D3D11_FAKE_VA_ALIGNMENT - 1) & ~(D3D11_FAKE_VA_ALIGNMENT - 1);
    
    // Try to find a suitable free range
    for (size_t i = 0; i < free_ranges_.size(); i++) {
        if (free_ranges_[i].size >= size) {
            D3D12_GPU_VIRTUAL_ADDRESS va = free_ranges_[i].base;
            
            // Update or remove the free range
            if (free_ranges_[i].size > size) {
                free_ranges_[i].base += size;
                free_ranges_[i].size -= size;
            } else {
                free_ranges_.erase(free_ranges_.begin() + i);
            }
            
            return va;
        }
    }
    
    // No suitable free range found, allocate new address
    if (next_va_ == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN) {
        next_va_ = D3D12_GPU_VIRTUAL_ADDRESS_RESERVE_START;
    }
    
    D3D12_GPU_VIRTUAL_ADDRESS va = next_va_;
    next_va_ += size;
    
    return va;
}

void GPUVirtualAddressManager::FreeGPUVA(D3D12_GPU_VIRTUAL_ADDRESS va, SIZE_T size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size = (size + D3D11_FAKE_VA_ALIGNMENT - 1) & ~(D3D11_FAKE_VA_ALIGNMENT - 1);
    
    GPUVARange new_range = {va, size};
    
    // Find position to insert the new range, maintaining order by address
    auto it = free_ranges_.begin();
    while (it != free_ranges_.end() && it->base < va) {
        ++it;
    }
    
    // Try to merge with adjacent ranges
    if (it != free_ranges_.begin()) {
        auto prev = it - 1;
        if (prev->base + prev->size == va) {
            prev->size += size;
            return;
        }
    }
    
    if (it != free_ranges_.end() && va + size == it->base) {
        it->base = va;
        it->size += size;
        return;
    }
    
    // Insert as new range
    free_ranges_.insert(it, new_range);
}

} // namespace dxiided