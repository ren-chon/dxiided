// gpu_va_mgr.hpp
#pragma once

#include <unordered_map>
#include <mutex>
#include <wrl/client.h>
#include <d3d12.h>
#include <d3d11.h>

namespace dxiided {

class WrappedD3D12ToD3D11Resource;

class GPUVirtualAddressManager {
public:
    GPUVirtualAddressManager();
    ~GPUVirtualAddressManager();

    // Allocate a new GPU virtual address for a resource
    D3D12_GPU_VIRTUAL_ADDRESS AllocateGPUVirtualAddress(WrappedD3D12ToD3D11Resource* resource);
    
    // Free a previously allocated GPU virtual address
    void FreeGPUVirtualAddress(D3D12_GPU_VIRTUAL_ADDRESS address);
    
    // Get resource from GPU virtual address
    WrappedD3D12ToD3D11Resource* GetResourceFromGPUVirtualAddress(D3D12_GPU_VIRTUAL_ADDRESS address);
    
    // Get GPU virtual address from resource
    D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddressFromResource(WrappedD3D12ToD3D11Resource* resource);

private:
    // Starting base address for our virtual address space
    // We'll use a high value to avoid conflicts with potential real addresses
    static constexpr D3D12_GPU_VIRTUAL_ADDRESS BASE_ADDRESS = 0x100000000000ULL;
    
    // Counter for generating new addresses
    D3D12_GPU_VIRTUAL_ADDRESS m_nextAddress;
    
    // Maps for bidirectional lookups
    std::unordered_map<D3D12_GPU_VIRTUAL_ADDRESS, WrappedD3D12ToD3D11Resource*> m_addressToResource;
    std::unordered_map<WrappedD3D12ToD3D11Resource*, D3D12_GPU_VIRTUAL_ADDRESS> m_resourceToAddress;
    
    // Mutex for thread safety
    std::mutex m_mutex;
};

} // namespace dxiided