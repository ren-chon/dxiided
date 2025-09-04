// gpu_va_mgr.cpp
#include "d3d11_impl/gpu_va_mgr.hpp"
#include "d3d11_impl/resource.hpp"
#include "common/debug.hpp"

namespace dxiided {

GPUVirtualAddressManager::GPUVirtualAddressManager() : m_nextAddress(BASE_ADDRESS) {
    TRACE("GPUVirtualAddressManager created");
}

GPUVirtualAddressManager::~GPUVirtualAddressManager() {
    TRACE("GPUVirtualAddressManager destroyed, %zu addresses still allocated", m_addressToResource.size());
    if (!m_addressToResource.empty()) {
        WARN("GPU virtual addresses still allocated during manager destruction");
    }
}

D3D12_GPU_VIRTUAL_ADDRESS GPUVirtualAddressManager::AllocateGPUVirtualAddress(WrappedD3D12ToD3D11Resource* resource) {
    if (!resource) {
        ERR("Attempted to allocate GPU virtual address for null resource");
        return 0;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if this resource already has an address
    auto it = m_resourceToAddress.find(resource);
    if (it != m_resourceToAddress.end()) {
        TRACE("Resource %p already has GPU virtual address %llu", resource, it->second);
        return it->second;
    }
    
    // Allocate a new address
    D3D12_GPU_VIRTUAL_ADDRESS address = m_nextAddress;
    m_nextAddress += 0x1000; // Increment by 4KB for the next allocation
    
    // Store mappings
    m_addressToResource[address] = resource;
    m_resourceToAddress[resource] = address;
    
    TRACE("Allocated GPU virtual address %llu for resource %p", address, resource);
    return address;
}

void GPUVirtualAddressManager::FreeGPUVirtualAddress(D3D12_GPU_VIRTUAL_ADDRESS address) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_addressToResource.find(address);
    if (it == m_addressToResource.end()) {
        WARN("Attempted to free non-existent GPU virtual address %llu", address);
        return;
    }
    
    WrappedD3D12ToD3D11Resource* resource = it->second;
    m_resourceToAddress.erase(resource);
    m_addressToResource.erase(address);
    
    TRACE("Freed GPU virtual address %llu for resource %p", address, resource);
}

WrappedD3D12ToD3D11Resource* GPUVirtualAddressManager::GetResourceFromGPUVirtualAddress(D3D12_GPU_VIRTUAL_ADDRESS address) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_addressToResource.find(address);
    if (it == m_addressToResource.end()) {
        WARN("GPU virtual address %llu not found", address);
        return nullptr;
    }
    
    return it->second;
}

D3D12_GPU_VIRTUAL_ADDRESS GPUVirtualAddressManager::GetGPUVirtualAddressFromResource(WrappedD3D12ToD3D11Resource* resource) {
    if (!resource) {
        ERR("Attempted to get GPU virtual address for null resource");
        return 0;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_resourceToAddress.find(resource);
    if (it == m_resourceToAddress.end()) {
        WARN("Resource %p does not have a GPU virtual address", resource);
        return 0;
    }
    
    return it->second;
}

} // namespace dxiided