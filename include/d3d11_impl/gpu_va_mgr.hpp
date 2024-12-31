#pragma once
#include <d3d11.h>
#include <d3d12.h>
#include <mutex>
#include <vector>

namespace dxiided {

#define D3D11_FAKE_VA_ALIGNMENT (65536)

struct GPUVARange {
    D3D12_GPU_VIRTUAL_ADDRESS base;
    SIZE_T size;
};

class GPUVirtualAddressManager {
public:
    GPUVirtualAddressManager();
    ~GPUVirtualAddressManager();

    D3D12_GPU_VIRTUAL_ADDRESS AllocateGPUVA(SIZE_T size);
    void FreeGPUVA(D3D12_GPU_VIRTUAL_ADDRESS va, SIZE_T size);
    
private:
    std::mutex mutex_;
    std::vector<GPUVARange> free_ranges_;
    D3D12_GPU_VIRTUAL_ADDRESS next_va_;
};

} // namespace dxiided