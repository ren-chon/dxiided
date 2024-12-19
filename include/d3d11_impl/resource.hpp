#pragma once

#include <d3d11.h>
#include <d3d12.h>
#include <wrl/client.h>

#include <memory>
#include <vector>
#include <string>

#include "common/debug.hpp"

namespace dxiided {

// Large buffer handling strategies
enum class LargeBufferStrategy {
    DEFAULT,           // Try to create as-is
    FALLBACK,         // Try different flags and smaller sizes
    SPLIT_CHUNKS      // Split into multiple smaller buffers
};

class D3D11Device;

class D3D11Resource final : public ID3D12Resource {
   public:
    static HRESULT Create(D3D11Device* device,
                          const D3D12_HEAP_PROPERTIES* pHeapProperties,
                          D3D12_HEAP_FLAGS HeapFlags,
                          const D3D12_RESOURCE_DESC* pDesc,
                          D3D12_RESOURCE_STATES InitialState,
                          const D3D12_CLEAR_VALUE* pOptimizedClearValue,
                          REFIID riid, void** ppvResource);

    // Create a D3D11Resource wrapper around an existing D3D11 resource
    static HRESULT Create(D3D11Device* device,
                         ID3D11Resource* resource,
                         const D3D12_RESOURCE_DESC* pDesc,
                         D3D12_RESOURCE_STATES InitialState,
                         REFIID riid, void** ppvResource);

    // IUnknown methods
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                             void** ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    // ID3D12Object methods
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT* pDataSize,
                                             void* pData) override;
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize,
                                             const void* pData) override;
    HRESULT STDMETHODCALLTYPE
    SetPrivateDataInterface(REFGUID guid, const IUnknown* pData) override;
    HRESULT STDMETHODCALLTYPE SetName(LPCWSTR Name) override;

    // ID3D12DeviceChild methods
    HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void** ppvDevice) override;

    // ID3D12Resource methods
    HRESULT STDMETHODCALLTYPE Map(UINT Subresource,
                                  const D3D12_RANGE* pReadRange,
                                  void** ppData) override;
    void STDMETHODCALLTYPE Unmap(UINT Subresource,
                                 const D3D12_RANGE* pWrittenRange) override;
    D3D12_RESOURCE_DESC* STDMETHODCALLTYPE GetDesc(D3D12_RESOURCE_DESC* pDesc) override;
    D3D12_GPU_VIRTUAL_ADDRESS STDMETHODCALLTYPE GetGPUVirtualAddress() override;
    HRESULT STDMETHODCALLTYPE WriteToSubresource(UINT DstSubresource,
                                                 const D3D12_BOX* pDstBox,
                                                 const void* pSrcData,
                                                 UINT SrcRowPitch,
                                                 UINT SrcDepthPitch) override;
    HRESULT STDMETHODCALLTYPE
    ReadFromSubresource(void* pDstData, UINT DstRowPitch, UINT DstDepthPitch,
                        UINT SrcSubresource, const D3D12_BOX* pSrcBox) override;
    HRESULT STDMETHODCALLTYPE
    GetHeapProperties(D3D12_HEAP_PROPERTIES* pHeapProperties,
                      D3D12_HEAP_FLAGS* pHeapFlags) override;

    // Resource state tracking
    D3D12_RESOURCE_STATES GetCurrentState() const { return m_currentState; }
    void TransitionTo(ID3D11DeviceContext* context,
                      D3D12_RESOURCE_STATES newState);
    void UAVBarrier(ID3D11DeviceContext* context);
    void AliasingBarrier(ID3D11DeviceContext* context,
                         D3D11Resource* pResourceAfter);

    // Helper methods
    ID3D11Resource* GetD3D11Resource() const { return m_resource.Get(); }
    static UINT GetMiscFlags(const D3D12_RESOURCE_DESC* pDesc);
    void StoreInDeviceMap();
    static D3D11_BIND_FLAG GetD3D11BindFlags(
        const D3D12_RESOURCE_DESC* pDesc,
        const D3D12_HEAP_PROPERTIES* pHeapProperties);

   private:
    struct ChunkInfo {
        UINT64 offset;
        UINT64 size;
        Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
    };

    D3D11Resource(D3D11Device* device,
                  const D3D12_HEAP_PROPERTIES* pHeapProperties,
                  D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC* pDesc,
                  D3D12_RESOURCE_STATES InitialState);

    // Constructor for wrapping existing D3D11 resource
    D3D11Resource(D3D11Device* device, ID3D11Resource* resource,
                  const D3D12_RESOURCE_DESC* pDesc,
                  D3D12_RESOURCE_STATES InitialState);

    static DXGI_FORMAT GetViewFormat(DXGI_FORMAT format);
    static D3D11_USAGE GetD3D11Usage(
        const D3D12_HEAP_PROPERTIES* pHeapProperties);

    D3D11Device* m_device;
    Microsoft::WRL::ComPtr<ID3D11Resource> m_resource;
    D3D12_RESOURCE_DESC m_desc;
    D3D12_HEAP_PROPERTIES m_heapProperties;
    D3D12_HEAP_FLAGS m_heapFlags;
    D3D12_GPU_VIRTUAL_ADDRESS m_gpuAddress{0};
    LONG m_refCount{1};
    D3D12_RESOURCE_STATES m_currentState{D3D12_RESOURCE_STATE_COMMON};
    D3D12_RESOURCE_STATES m_state;  // Add state member
    bool m_isUAV{false};
    std::vector<ChunkInfo> m_chunks;  // For split buffers

    // Helper functions for large buffer creation
    HRESULT CreateBufferDefault(const D3D11_BUFFER_DESC& desc);
    HRESULT CreateBufferWithFallback(const D3D11_BUFFER_DESC& desc);
    HRESULT CreateBufferInChunks(const D3D11_BUFFER_DESC& desc);

    static const char* GetStrategyName(LargeBufferStrategy strategy) {
        switch (strategy) {
            case LargeBufferStrategy::DEFAULT: return "DEFAULT";
            case LargeBufferStrategy::FALLBACK: return "FALLBACK";
            case LargeBufferStrategy::SPLIT_CHUNKS: return "SPLIT_CHUNKS";
            default: return "UNKNOWN";
        }
    }
};

// Convert D3D11 usage to human readable string
inline const char* GetD3D11UsageName(D3D11_USAGE usage) {
    switch (usage) {
        case D3D11_USAGE_DEFAULT: return "DEFAULT";
        case D3D11_USAGE_IMMUTABLE: return "IMMUTABLE";
        case D3D11_USAGE_DYNAMIC: return "DYNAMIC";
        case D3D11_USAGE_STAGING: return "STAGING";
        default: return "UNKNOWN";
    }
}

// Convert D3D11 map type to human readable string 
inline const char* GetD3D11MapTypeName(D3D11_MAP type) {
    switch (type) {
        case D3D11_MAP_READ: return "READ";
        case D3D11_MAP_WRITE: return "WRITE";
        case D3D11_MAP_READ_WRITE: return "READ_WRITE";
        case D3D11_MAP_WRITE_DISCARD: return "WRITE_DISCARD";
        case D3D11_MAP_WRITE_NO_OVERWRITE: return "WRITE_NO_OVERWRITE";
        default: return "UNKNOWN";
    }
}

// Get string representation of combined resource states
inline std::string GetD3D12ResourceStateString(D3D12_RESOURCE_STATES state) {
    std::string result;
    
    // Check individual bits and combine names
    if (state & D3D12_RESOURCE_STATE_COMMON) {
        result += "COMMON";
    }
    if (state & D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER) {
        if (!result.empty()) result += " | ";
        result += "VERTEX_AND_CONSTANT_BUFFER";
    }
    if (state & D3D12_RESOURCE_STATE_INDEX_BUFFER) {
        if (!result.empty()) result += " | ";
        result += "INDEX_BUFFER";
    }
    if (state & D3D12_RESOURCE_STATE_RENDER_TARGET) {
        if (!result.empty()) result += " | ";
        result += "RENDER_TARGET";
    }
    if (state & D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        if (!result.empty()) result += " | ";
        result += "UNORDERED_ACCESS";
    }
    if (state & D3D12_RESOURCE_STATE_DEPTH_WRITE) {
        if (!result.empty()) result += " | ";
        result += "DEPTH_WRITE";
    }
    if (state & D3D12_RESOURCE_STATE_DEPTH_READ) {
        if (!result.empty()) result += " | ";
        result += "DEPTH_READ";
    }
    if (state & D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) {
        if (!result.empty()) result += " | ";
        result += "NON_PIXEL_SHADER_RESOURCE";
    }
    if (state & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
        if (!result.empty()) result += " | ";
        result += "PIXEL_SHADER_RESOURCE";
    }
    if (state & D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT) {
        if (!result.empty()) result += " | ";
        result += "INDIRECT_ARGUMENT";
    }
    if (state & D3D12_RESOURCE_STATE_COPY_DEST) {
        if (!result.empty()) result += " | ";
        result += "COPY_DEST";
    }
    if (state & D3D12_RESOURCE_STATE_COPY_SOURCE) {
        if (!result.empty()) result += " | ";
        result += "COPY_SOURCE";
    }
    if (state & D3D12_RESOURCE_STATE_RESOLVE_DEST) {
        if (!result.empty()) result += " | ";
        result += "RESOLVE_DEST";
    }
    if (state & D3D12_RESOURCE_STATE_RESOLVE_SOURCE) {
        if (!result.empty()) result += " | ";
        result += "RESOLVE_SOURCE";
    }
    if (state & D3D12_RESOURCE_STATE_GENERIC_READ) {
        if (!result.empty()) result += " | ";
        result += "GENERIC_READ";
    }
    if (state & D3D12_RESOURCE_STATE_PRESENT) {
        if (!result.empty()) result += " | ";
        result += "PRESENT";
    }
    if (state & D3D12_RESOURCE_STATE_PREDICATION) {
        if (!result.empty()) result += " | ";
        result += "PREDICATION";
    }
    if (state & D3D12_RESOURCE_STATE_STREAM_OUT) {
        if (!result.empty()) result += " | ";
        result += "STREAM_OUT";
    }
    
    return result.empty() ? "UNKNOWN" : result;
}

// Convert D3D12 heap type to human readable string
inline const char* GetD3D12HeapTypeName(D3D12_HEAP_TYPE type) {
    switch (type) {
        case D3D12_HEAP_TYPE_DEFAULT:
            return "DEFAULT";
        case D3D12_HEAP_TYPE_UPLOAD:
            return "UPLOAD";
        case D3D12_HEAP_TYPE_READBACK:
            return "READBACK";
        case D3D12_HEAP_TYPE_CUSTOM:
            return "CUSTOM";
        default:
            return "UNKNOWN";
    }
}

// Convert D3D12 CPU page property to human readable string
inline const char* GetD3D12CPUPagePropertyName(D3D12_CPU_PAGE_PROPERTY prop) {
    switch (prop) {
        case D3D12_CPU_PAGE_PROPERTY_UNKNOWN:
            return "UNKNOWN";
        case D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE:
            return "NOT_AVAILABLE";
        case D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE:
            return "WRITE_COMBINE";
        case D3D12_CPU_PAGE_PROPERTY_WRITE_BACK:
            return "WRITE_BACK";
        default:
            return "INVALID";
    }
}

// Convert D3D12 memory pool to human readable string
inline const char* GetD3D12MemoryPoolName(D3D12_MEMORY_POOL pool) {
    switch (pool) {
        case D3D12_MEMORY_POOL_UNKNOWN:
            return "UNKNOWN";
        case D3D12_MEMORY_POOL_L0:
            return "L0";
        case D3D12_MEMORY_POOL_L1:
            return "L1";
        default:
            return "INVALID";
    }
}

// Convert D3D12 resource dimension to human readable string
inline const char* GetD3D12ResourceDimensionName(D3D12_RESOURCE_DIMENSION dim) {
    switch (dim) {
        case D3D12_RESOURCE_DIMENSION_UNKNOWN:
            return "UNKNOWN";
        case D3D12_RESOURCE_DIMENSION_BUFFER:
            return "BUFFER";
        case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
            return "TEXTURE1D";
        case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
            return "TEXTURE2D";
        case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
            return "TEXTURE3D";
        default:
            return "INVALID";
    }
}

// Convert DXGI format to human readable string
inline const char* GetDXGIFormatName(DXGI_FORMAT format) {
    switch (format) {
        case DXGI_FORMAT_UNKNOWN:
            return "UNKNOWN";
        case DXGI_FORMAT_R32G32B32A32_TYPELESS:
            return "R32G32B32A32_TYPELESS";
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
            return "R32G32B32A32_FLOAT";
        case DXGI_FORMAT_R32G32B32A32_UINT:
            return "R32G32B32A32_UINT";
        case DXGI_FORMAT_R32G32B32A32_SINT:
            return "R32G32B32A32_SINT";
        case DXGI_FORMAT_R32G32B32_TYPELESS:
            return "R32G32B32_TYPELESS";
        case DXGI_FORMAT_R32G32B32_FLOAT:
            return "R32G32B32_FLOAT";
        case DXGI_FORMAT_R32G32B32_UINT:
            return "R32G32B32_UINT";
        case DXGI_FORMAT_R32G32B32_SINT:
            return "R32G32B32_SINT";
        case DXGI_FORMAT_R16G16B16A16_TYPELESS:
            return "R16G16B16A16_TYPELESS";
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
            return "R16G16B16A16_FLOAT";
        case DXGI_FORMAT_R16G16B16A16_UNORM:
            return "R16G16B16A16_UNORM";
        case DXGI_FORMAT_R16G16B16A16_UINT:
            return "R16G16B16A16_UINT";
        case DXGI_FORMAT_R16G16B16A16_SNORM:
            return "R16G16B16A16_SNORM";
        case DXGI_FORMAT_R16G16B16A16_SINT:
            return "R16G16B16A16_SINT";
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
            return "R8G8B8A8_TYPELESS";
        case DXGI_FORMAT_R8G8B8A8_UNORM:
            return "R8G8B8A8_UNORM";
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            return "R8G8B8A8_UNORM_SRGB";
        case DXGI_FORMAT_R8G8B8A8_UINT:
            return "R8G8B8A8_UINT";
        case DXGI_FORMAT_R8G8B8A8_SNORM:
            return "R8G8B8A8_SNORM";
        case DXGI_FORMAT_R8G8B8A8_SINT:
            return "R8G8B8A8_SINT";
        default:
            return "OTHER";
    }
}

// Convert D3D12 texture layout to human readable string
inline const char* GetD3D12TextureLayoutName(D3D12_TEXTURE_LAYOUT layout) {
    switch (layout) {
        case D3D12_TEXTURE_LAYOUT_UNKNOWN:
            return "UNKNOWN";
        case D3D12_TEXTURE_LAYOUT_ROW_MAJOR:
            return "ROW_MAJOR";
        case D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE:
            return "64KB_UNDEFINED_SWIZZLE";
        case D3D12_TEXTURE_LAYOUT_64KB_STANDARD_SWIZZLE:
            return "64KB_STANDARD_SWIZZLE";
        default:
            return "INVALID";
    }
}
// Convert D3D12 resource flags to human readable string
inline std::string GetD3D12ResourceFlagsString(D3D12_RESOURCE_FLAGS flags) {
    std::string result;
    
    if (flags == D3D12_RESOURCE_FLAG_NONE) {
        return "NONE";
    }

    if (flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) {
        if (!result.empty()) result += " | ";
        result += "ALLOW_RENDER_TARGET";
    }
    if (flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) {
        if (!result.empty()) result += " | ";
        result += "ALLOW_DEPTH_STENCIL";
    }
    if (flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) {
        if (!result.empty()) result += " | ";
        result += "ALLOW_UNORDERED_ACCESS";
    }
    if (flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) {
        if (!result.empty()) result += " | ";
        result += "DENY_SHADER_RESOURCE";
    }
    if (flags & D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER) {
        if (!result.empty()) result += " | ";
        result += "ALLOW_CROSS_ADAPTER";
    }
    if (flags & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS) {
        if (!result.empty()) result += " | ";
        result += "ALLOW_SIMULTANEOUS_ACCESS";
    }
    if (flags & D3D12_RESOURCE_FLAG_VIDEO_DECODE_REFERENCE_ONLY) {
        if (!result.empty()) result += " | ";
        result += "VIDEO_DECODE_REFERENCE_ONLY";
    }

    return result.empty() ? "UNKNOWN" : result;
}

// Convert D3D11 CPU access flags to human readable string
inline std::string GetD3D11CPUAccessFlagsString(UINT flags) {
    std::string result;
    
    if (flags & D3D11_CPU_ACCESS_WRITE)
        result += "WRITE";
    
    if (flags & D3D11_CPU_ACCESS_READ) {
        if (!result.empty()) result += " | ";
        result += "READ";
    }
    
    if (result.empty())
        return "NONE";
        
    return result;
}

// Convert D3D11 bind flags to human readable string
inline std::string GetD3D11BindFlagsString(UINT flags) {
    std::string result;
    
    if (flags & D3D11_BIND_VERTEX_BUFFER) {
        if (!result.empty()) result += " | ";
        result += "VERTEX_BUFFER";
    }
    
    if (flags & D3D11_BIND_INDEX_BUFFER) {
        if (!result.empty()) result += " | ";
        result += "INDEX_BUFFER";
    }
    
    if (flags & D3D11_BIND_CONSTANT_BUFFER) {
        if (!result.empty()) result += " | ";
        result += "CONSTANT_BUFFER";
    }
    
    if (flags & D3D11_BIND_SHADER_RESOURCE) {
        if (!result.empty()) result += " | ";
        result += "SHADER_RESOURCE";
    }
    
    if (flags & D3D11_BIND_RENDER_TARGET) {
        if (!result.empty()) result += " | ";
        result += "RENDER_TARGET";
    }
    
    if (flags & D3D11_BIND_DEPTH_STENCIL) {
        if (!result.empty()) result += " | ";
        result += "DEPTH_STENCIL";
    }
    
    if (flags & D3D11_BIND_UNORDERED_ACCESS) {
        if (!result.empty()) result += " | ";
        result += "UNORDERED_ACCESS";
    }
    
    if (result.empty())
        return "NONE";
        
    return result;
}
}  // namespace dxiided
