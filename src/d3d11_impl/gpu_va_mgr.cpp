#include "d3d11_impl/gpu_va_mgr.hpp"
#include "common/debug.hpp"

namespace dxiided {

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
    switch (pDesc->Dimension) {
        case D3D12_RESOURCE_DIMENSION_BUFFER:
            return 256;  // D3D12 buffer alignment
            
        case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
        case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
        case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
            return 512;  // D3D12 texture alignment
            
        default:
            WARN("Unknown resource dimension: %d", pDesc->Dimension);
            return 256;  // minimum alignment
    }
}

D3D12_GPU_VIRTUAL_ADDRESS GPUVirtualAddressManager::AllocateInternal(
    size_t size, size_t alignment) {
    // Round up size to alignment
    size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);
    
    void* addr = VirtualAlloc(NULL, aligned_size, MEM_RESERVE, PAGE_NOACCESS);
    if (!addr) {
        WARN("VirtualAlloc failed with error: %lu", GetLastError());
        return 0;
    }
    
    D3D12_GPU_VIRTUAL_ADDRESS gpu_addr = (D3D12_GPU_VIRTUAL_ADDRESS)addr;
    m_allocations[gpu_addr] = aligned_size;
    
    return gpu_addr;
}

}  // namespace dxiided
