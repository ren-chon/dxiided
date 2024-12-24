#include "d3d11_impl/resource.hpp"

#include "d3d11_impl/device.hpp"

namespace dxiided {

HRESULT WrappedD3D12ToD3D11Resource::Create(WrappedD3D12ToD3D11Device* device,
                              const D3D12_HEAP_PROPERTIES* pHeapProperties,
                              D3D12_HEAP_FLAGS HeapFlags,
                              const D3D12_RESOURCE_DESC* pDesc,
                              D3D12_RESOURCE_STATES InitialState,
                              const D3D12_CLEAR_VALUE* pOptimizedClearValue,
                              REFIID riid, void** ppvResource) {
    TRACE(
        "WrappedD3D12ToD3D11Resource::Create called with: %p, %p, %#x, %p, %#x, %p, %s, %p",
        device, pHeapProperties, HeapFlags, pDesc, InitialState,
        pOptimizedClearValue, debugstr_guid(&riid).c_str(), ppvResource);

    if (!device || !pHeapProperties || !pDesc || !ppvResource) {
        WARN("Invalid parameters: device: %d, pHeapProperties: %p, pDesc: %d, ppvResource: %r", device , pHeapProperties , pDesc , ppvResource);
        return E_INVALIDARG;
    }

    Microsoft::WRL::ComPtr<WrappedD3D12ToD3D11Resource> resource = new WrappedD3D12ToD3D11Resource(
        device, pHeapProperties, HeapFlags, pDesc, InitialState);

    if (!resource->GetD3D11Resource()) {
        ERR("Failed to create D3D11 resource.");
        return E_FAIL;
    }

    return resource.CopyTo(reinterpret_cast<ID3D12Resource**>(ppvResource));
}

HRESULT WrappedD3D12ToD3D11Resource::Create(WrappedD3D12ToD3D11Device* device,
                             ID3D11Resource* resource,
                             const D3D12_RESOURCE_DESC* pDesc,
                             D3D12_RESOURCE_STATES InitialState,
                             REFIID riid, void** ppvResource) {
    if (!device || !resource || !pDesc || !ppvResource) {
        WARN("Invalid parameters: device=%p, resource=%p, pDesc=%p, ppvResource=%p",
              device, resource, pDesc, ppvResource);
        return E_INVALIDARG;
    }

    Microsoft::WRL::ComPtr<WrappedD3D12ToD3D11Resource> wrapper = new WrappedD3D12ToD3D11Resource(
        device, resource, pDesc, InitialState);

    if (!wrapper->GetD3D11Resource()) {
        ERR("Failed to wrap D3D11 resource.");
        return E_FAIL;
    }

    return wrapper.CopyTo(reinterpret_cast<ID3D12Resource**>(ppvResource));
}

WrappedD3D12ToD3D11Resource::WrappedD3D12ToD3D11Resource(WrappedD3D12ToD3D11Device* device,
                             const D3D12_HEAP_PROPERTIES* pHeapProperties,
                             D3D12_HEAP_FLAGS HeapFlags,
                             const D3D12_RESOURCE_DESC* pDesc,
                             D3D12_RESOURCE_STATES InitialState)
    : m_device(device),
      m_resource(nullptr),
      m_desc(*pDesc),
      m_heapProperties(*pHeapProperties),
      m_heapFlags(HeapFlags),
      m_gpuAddress(0),
      m_refCount(1),
      m_currentState(InitialState),
      m_state(InitialState),
      m_isUAV(false),
      m_format(pDesc->Format) {
    
    // If it looks like a buffer (1D with height=1), treat it as one
    if (pDesc->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER ||
        (pDesc->Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D &&
         pDesc->Height == 1)) {
        
        D3D11_BUFFER_DESC bufferDesc = {};
        bufferDesc.ByteWidth = static_cast<UINT>(pDesc->Width);
        bufferDesc.Usage = GetD3D11Usage(pHeapProperties);
        bufferDesc.BindFlags = GetD3D11BindFlags(pDesc);
        bufferDesc.CPUAccessFlags = GetD3D11CPUAccessFlags(pHeapProperties);
        bufferDesc.MiscFlags = GetMiscFlags(pDesc);
        bufferDesc.StructureByteStride = 0;

        // If this buffer will be used as SRV/UAV, set format
        if (bufferDesc.BindFlags & (D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS)) {
            // If we know the format from resource desc, use that
            if (m_format == DXGI_FORMAT_UNKNOWN) {
                // Default to R32 format for shader visibility
                m_format = DXGI_FORMAT_R32_FLOAT;
            }
            TRACE("Creating buffer with format %d for SRV/UAV usage", m_format);
        }

        TRACE("Creating buffer with Usage=%d, CPUAccessFlags=%d, BindFlags=%d",
              bufferDesc.Usage, bufferDesc.CPUAccessFlags, bufferDesc.BindFlags);

        Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
        HRESULT hr = m_device->GetD3D11Device()->CreateBuffer(
            &bufferDesc, nullptr, &buffer);
        
        if (FAILED(hr)) {
            ERR("Failed to create D3D11 buffer, hr %#x", hr);
            return;
        }
        
        m_resource = buffer;
        
        // Store GPU virtual address for buffer
        if (bufferDesc.BindFlags & (D3D11_BIND_VERTEX_BUFFER |
                                  D3D11_BIND_INDEX_BUFFER |
                                  D3D11_BIND_CONSTANT_BUFFER |
                                  D3D11_BIND_SHADER_RESOURCE |
                                  D3D11_BIND_UNORDERED_ACCESS)) {
            m_gpuAddress = m_device->AllocateGPUVirtualAddress(this, bufferDesc.ByteWidth);
        }
    }
    else {
        DXGI_FORMAT format = GetViewFormat(pDesc->Format);
        switch (pDesc->Dimension) {
            case D3D12_RESOURCE_DIMENSION_TEXTURE1D: {
                TRACE("D3D12_RESOURCE_DIMENSION_TEXTURE1D match");
                D3D11_TEXTURE1D_DESC texDesc = {};
                texDesc.Width = static_cast<UINT>(pDesc->Width);
                texDesc.MipLevels = pDesc->MipLevels;
                texDesc.ArraySize = pDesc->DepthOrArraySize;
                texDesc.Format = format;
                texDesc.Usage = GetD3D11Usage(pHeapProperties);
                texDesc.BindFlags = GetD3D11BindFlags(pDesc);
                texDesc.CPUAccessFlags =
                    (GetD3D11Usage(pHeapProperties) == D3D11_USAGE_DYNAMIC) ? D3D11_CPU_ACCESS_WRITE
                    : (GetD3D11Usage(pHeapProperties) == D3D11_USAGE_STAGING)
                        ? (D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE)
                        : 0;
                texDesc.MiscFlags = 0;

                Microsoft::WRL::ComPtr<ID3D11Texture1D> texture;
                HRESULT hr = m_device->GetD3D11Device()->CreateTexture1D(
                    &texDesc, nullptr, &texture);
                if (FAILED(hr)) {
                    ERR("Failed to create texture 1D, hr %#x.", hr);
                    return;
                }
                m_resource = texture;
                StoreInDeviceMap();
                break;
            }
            case D3D12_RESOURCE_DIMENSION_TEXTURE2D: {
                TRACE("D3D12_RESOURCE_DIMENSION_TEXTURE2D match");
                D3D11_TEXTURE2D_DESC texDesc = {};
                texDesc.Width = static_cast<UINT>(pDesc->Width);
                texDesc.Height = pDesc->Height;
                texDesc.MipLevels = pDesc->MipLevels;
                texDesc.ArraySize = pDesc->DepthOrArraySize;
                texDesc.Format = format;
                texDesc.SampleDesc = pDesc->SampleDesc;
                texDesc.Usage = GetD3D11Usage(pHeapProperties);
                texDesc.BindFlags = GetD3D11BindFlags(pDesc);
                texDesc.CPUAccessFlags =
                    (GetD3D11Usage(pHeapProperties) == D3D11_USAGE_DYNAMIC) ? D3D11_CPU_ACCESS_WRITE
                    : (GetD3D11Usage(pHeapProperties) == D3D11_USAGE_STAGING)
                        ? (D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE)
                        : 0;
                texDesc.MiscFlags = GetMiscFlags(pDesc);

                Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
                HRESULT hr = m_device->GetD3D11Device()->CreateTexture2D(
                    &texDesc, nullptr, &texture);
                if (FAILED(hr)) {
                    ERR("Failed to create texture 2D, hr %#x.", hr);
                    return;
                }
                m_resource = texture;
                StoreInDeviceMap();
                break;
            }
            case D3D12_RESOURCE_DIMENSION_TEXTURE3D: {
                TRACE("D3D12_RESOURCE_DIMENSION_TEXTURE3D match");
                D3D11_TEXTURE3D_DESC texDesc = {};
                texDesc.Width = static_cast<UINT>(pDesc->Width);
                texDesc.Height = pDesc->Height;
                texDesc.Depth = pDesc->DepthOrArraySize;
                texDesc.MipLevels = pDesc->MipLevels;
                texDesc.Format = format;
                texDesc.Usage = GetD3D11Usage(pHeapProperties);
                texDesc.BindFlags = GetD3D11BindFlags(pDesc);
                texDesc.CPUAccessFlags =
                    (GetD3D11Usage(pHeapProperties) == D3D11_USAGE_DYNAMIC) ? D3D11_CPU_ACCESS_WRITE
                    : (GetD3D11Usage(pHeapProperties) == D3D11_USAGE_STAGING)
                        ? (D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE)
                        : 0;
                texDesc.MiscFlags = GetMiscFlags(pDesc);

                Microsoft::WRL::ComPtr<ID3D11Texture3D> texture;
                HRESULT hr = m_device->GetD3D11Device()->CreateTexture3D(
                    &texDesc, nullptr, &texture);
                if (FAILED(hr)) {
                    ERR("Failed to create texture 3D, hr %#x.", hr);
                    return;
                }
                m_resource = texture;
                StoreInDeviceMap();
                break;
            }
            default:
                ERR("Unsupported resource dimension %d.", pDesc->Dimension);
                break;
        }
    }

    TRACE("Creating resource type=%d, format=%d, width=%llu, height=%u, this=%p",
          m_desc.Dimension, m_desc.Format, m_desc.Width, m_desc.Height, this);
}

WrappedD3D12ToD3D11Resource::WrappedD3D12ToD3D11Resource(WrappedD3D12ToD3D11Device* device,
                            ID3D11Resource* resource,
                            const D3D12_RESOURCE_DESC* pDesc,
                            D3D12_RESOURCE_STATES InitialState)
    : m_device(device)
    , m_resource(resource)
    , m_desc(*pDesc)
    , m_heapProperties{}
    , m_heapFlags(D3D12_HEAP_FLAG_NONE)
    , m_gpuAddress(0)
    , m_refCount(1)
    , m_currentState(InitialState)
    , m_state(InitialState)
    , m_isUAV(false)
    , m_format(pDesc->Format) {
    
    if (resource) {
        // Check if this is a buffer that needs GPU virtual address
        if (pDesc->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
            D3D11_BUFFER_DESC bufferDesc = {};
            Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
            if (SUCCEEDED(resource->QueryInterface(IID_PPV_ARGS(&buffer)))) {
                buffer->GetDesc(&bufferDesc);
                
                // Allocate GPU virtual address if buffer can be accessed by shaders
                if (bufferDesc.BindFlags & (D3D11_BIND_CONSTANT_BUFFER | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS)) {
                    m_gpuAddress = m_device->AllocateGPUVirtualAddress(this, static_cast<UINT64>(pDesc->Width));
                    TRACE("Allocated GPU virtual address %llx for existing buffer", m_gpuAddress);
                }
            }
        }
        
        StoreInDeviceMap();
    }
}

WrappedD3D12ToD3D11Resource::~WrappedD3D12ToD3D11Resource() {
    TRACE("Destroying resource this=%p, gpuAddress=%llu", this, m_gpuAddress);
    if (m_gpuAddress) {
        m_device->FreeGPUVirtualAddress(m_gpuAddress);
        m_gpuAddress = 0;  // Clear it to help catch use-after-free
    }
}

void WrappedD3D12ToD3D11Resource::StoreInDeviceMap() {
    TRACE("StoreInDeviceMap for resource %p, type: %d", this, m_desc.Dimension);
    
    // Store D3D11<->D3D12 resource mapping
    Microsoft::WRL::ComPtr<ID3D11Resource> d3d11Resource;
    if (SUCCEEDED(m_resource.As(&d3d11Resource))) {
        m_device->StoreD3D11ResourceMapping(this, d3d11Resource.Get());
        
        // Extra debug info for buffers
        if (m_desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
            Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
            if (SUCCEEDED(m_resource.As(&buffer))) {
                D3D11_BUFFER_DESC desc = {};
                buffer->GetDesc(&desc);
                TRACE("  Buffer info - BindFlags: %u, ByteWidth: %u, Usage: %d", 
                      desc.BindFlags, desc.ByteWidth, desc.Usage);
            }
        }
    }
}

UINT WrappedD3D12ToD3D11Resource::GetMiscFlags(const D3D12_RESOURCE_DESC* pDesc) {
    TRACE("GetMiscFlags called");
    TRACE("  Alignment: %llu", pDesc->Alignment);
    TRACE("  Dimension: %x", pDesc->Dimension);
    TRACE("  DepthOrArraySize: %x", pDesc->DepthOrArraySize);
    TRACE("  MipLevels: %x", pDesc->MipLevels);
    TRACE("  Format: %x", pDesc->Format);
    TRACE("  Flags: %x", pDesc->Flags);
    TRACE("  Height: %x", pDesc->Height);
    TRACE("  Width: %x", pDesc->Width);
    TRACE("  SampleDesc Count: %x", pDesc->SampleDesc.Count);
    TRACE("  SampleDesc Quality: %x", pDesc->SampleDesc.Quality);

    UINT flags = 0;

    if (pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS) {
        TRACE("pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS");
        flags |= D3D11_RESOURCE_MISC_SHARED;
    }

    if (pDesc->Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D &&
        pDesc->DepthOrArraySize == 6) {
        TRACE("pDesc->Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D");
        flags |= D3D11_RESOURCE_MISC_TEXTURECUBE;
    }

    if (pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET &&
        pDesc->MipLevels > 1) {
        TRACE("pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET");
        flags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
    }

    TRACE("GetMiscFlags returns %#x", flags);
    return flags;
}

void WrappedD3D12ToD3D11Resource::TransitionTo(ID3D11DeviceContext* context,
                                 D3D12_RESOURCE_STATES newState) {
    TRACE("WrappedD3D12ToD3D11Resource::TransitionTo %p, %#x -> %#x", context,
          m_currentState, newState);

    // No need to transition if states are the same
    if (m_currentState == newState) {
        return;
    }

    // Handle UAV barriers
    if (newState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS ||
        m_currentState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        TRACE("UAV barrier");
        context->CSSetUnorderedAccessViews(0, 1, nullptr, nullptr);
        m_isUAV = (newState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }

    // Handle transitions requiring synchronization
    if ((m_currentState == D3D12_RESOURCE_STATE_RENDER_TARGET &&
         newState != D3D12_RESOURCE_STATE_RENDER_TARGET) ||
        (m_currentState != D3D12_RESOURCE_STATE_RENDER_TARGET &&
         newState == D3D12_RESOURCE_STATE_RENDER_TARGET)) {
        TRACE("RT barrier");
        context->Flush();
    }

    // Handle depth-stencil transitions
    if ((m_currentState == D3D12_RESOURCE_STATE_DEPTH_WRITE &&
         newState != D3D12_RESOURCE_STATE_DEPTH_WRITE) ||
        (m_currentState != D3D12_RESOURCE_STATE_DEPTH_WRITE &&
         newState == D3D12_RESOURCE_STATE_DEPTH_WRITE)) {
        TRACE("DS barrier");
        context->Flush();
    }

    m_currentState = newState;
}

void WrappedD3D12ToD3D11Resource::UAVBarrier(ID3D11DeviceContext* context) {
    TRACE("WrappedD3D12ToD3D11Resource::UAVBarrier %p", context);

    if (m_isUAV) {
        // Ensure UAV writes are completed
        context->CSSetUnorderedAccessViews(0, 1, nullptr, nullptr);
        context->Flush();
    }
}

void WrappedD3D12ToD3D11Resource::AliasingBarrier(ID3D11DeviceContext* context,
                                    WrappedD3D12ToD3D11Resource* pResourceAfter) {
    TRACE("WrappedD3D12ToD3D11Resource::AliasingBarrier %p, %p", context, pResourceAfter);

    // Ensure all operations on both resources are completed
    context->Flush();
}

D3D11_BIND_FLAG WrappedD3D12ToD3D11Resource::GetD3D11BindFlags(
    const D3D12_RESOURCE_DESC* pDesc) {
    TRACE("WrappedD3D12ToD3D11Resource::GetD3D11BindFlags called");
    D3D11_BIND_FLAG flags = static_cast<D3D11_BIND_FLAG>(0);

    // For buffers, set appropriate bind flags based on size and alignment
    if (pDesc->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER || 
        (pDesc->Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D && pDesc->Height == 1)) {
        TRACE("Resource is a buffer or 1D resource with height=1");
        
        // For small buffers (<4KB), could be vertex or index buffer
        if (pDesc->Width <= 4096) {
            flags = static_cast<D3D11_BIND_FLAG>(flags | D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_INDEX_BUFFER);
        }
        
        // For aligned buffers, could be constant buffer regardless of size
        if (!(pDesc->Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) && 
            (pDesc->Width % 16) == 0) {
            flags = static_cast<D3D11_BIND_FLAG>(flags | D3D11_BIND_CONSTANT_BUFFER);
        }
    }

    // Add UAV flag if requested
    if (pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) {
        flags = static_cast<D3D11_BIND_FLAG>(flags | D3D11_BIND_UNORDERED_ACCESS);
    }

    // Add shader resource view if not denied and we don't already have constant buffer
    if (!(pDesc->Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)) {
        flags = static_cast<D3D11_BIND_FLAG>(flags | D3D11_BIND_SHADER_RESOURCE);
    }

    // For render targets
    if (pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) {
        flags = static_cast<D3D11_BIND_FLAG>(flags | D3D11_BIND_RENDER_TARGET);
    }

    // For depth-stencil
    if (pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) {
        flags = static_cast<D3D11_BIND_FLAG>(flags | D3D11_BIND_DEPTH_STENCIL);
    }

    TRACE("  Resource flags: %#x -> D3D11 bind flags: %#x", pDesc->Flags, flags);
    return flags;
}

DXGI_FORMAT WrappedD3D12ToD3D11Resource::GetViewFormat(DXGI_FORMAT format) {
    switch (format) {
        // Keep original format for common UNORM/SNORM formats
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R8G8B8A8_SNORM:
        case DXGI_FORMAT_R16G16B16A16_SNORM:
            return format;

        // For buffer SRVs, handle typed buffer formats
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
        case DXGI_FORMAT_R32_FLOAT:
        // Add structured buffer formats
        case DXGI_FORMAT_R16G16B16A16_SINT:
        case DXGI_FORMAT_R32G32_FLOAT:
        case DXGI_FORMAT_R32G32_UINT:
        case DXGI_FORMAT_R32G32_SINT:
            return format;
            
        default:
            WARN("Unsupported format %d, using original format", format);
            return format;
    }
}

D3D11_USAGE WrappedD3D12ToD3D11Resource::GetD3D11Usage(
    const D3D12_HEAP_PROPERTIES* pHeapProperties) {
    TRACE("WrappedD3D12ToD3D11Resource::GetD3D11Usage called");
    switch (pHeapProperties->Type) {
        case D3D12_HEAP_TYPE_DEFAULT:
            return D3D11_USAGE_DEFAULT;
        case D3D12_HEAP_TYPE_UPLOAD:
            return D3D11_USAGE_DYNAMIC;
        case D3D12_HEAP_TYPE_READBACK:
            return D3D11_USAGE_STAGING;
        case D3D12_HEAP_TYPE_CUSTOM:
            if (pHeapProperties->CPUPageProperty ==
                D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE) {
                return D3D11_USAGE_DYNAMIC;
            }
            if (pHeapProperties->CPUPageProperty ==
                D3D12_CPU_PAGE_PROPERTY_WRITE_BACK) {
                return D3D11_USAGE_STAGING;
            }
            return D3D11_USAGE_DEFAULT;
        default:
            return D3D11_USAGE_DEFAULT;
    }
}

UINT WrappedD3D12ToD3D11Resource::GetD3D11CPUAccessFlags(const D3D12_HEAP_PROPERTIES* pHeapProperties) {
    TRACE("WrappedD3D12ToD3D11Resource::GetD3D11CPUAccessFlags called");
    switch (pHeapProperties->Type) {
        case D3D12_HEAP_TYPE_DEFAULT:
            return 0;
        case D3D12_HEAP_TYPE_UPLOAD:
            return D3D11_CPU_ACCESS_WRITE;
        case D3D12_HEAP_TYPE_READBACK:
            return D3D11_CPU_ACCESS_READ;
        case D3D12_HEAP_TYPE_CUSTOM:
            if (pHeapProperties->CPUPageProperty ==
                D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE) {
                return D3D11_CPU_ACCESS_WRITE;
            }
            if (pHeapProperties->CPUPageProperty ==
                D3D12_CPU_PAGE_PROPERTY_WRITE_BACK) {
                return D3D11_CPU_ACCESS_READ;
            }
            return 0;
        default:
            return 0;
    }
}

// IUnknown methods
HRESULT WrappedD3D12ToD3D11Resource::QueryInterface(REFIID riid, void** ppvObject) {
    TRACE("WrappedD3D12ToD3D11Resource::QueryInterface called: %s, %p",
          debugstr_guid(&riid).c_str(), ppvObject);

    if (!ppvObject) {
        return E_POINTER;
    }

    *ppvObject = nullptr;

    if (riid == __uuidof(ID3D12Resource)) {
        TRACE("WrappedD3D12ToD3D11Resource::QueryInterface returning ID3D12Resource");
        AddRef();
        *ppvObject = static_cast<ID3D12Resource*>(this);
        return S_OK;
    }

    if (riid == __uuidof(IUnknown)) {
        TRACE("WrappedD3D12ToD3D11Resource::QueryInterface returning IUnknown");
        AddRef();
        *ppvObject = static_cast<IUnknown*>(this);
        return S_OK;
    }

    // Handle D3D11 resource mapping interface
    if (riid == __uuidof(ID3D11Resource)) {
        if (m_resource) {
            m_resource->AddRef();  // AddRef on the underlying resource instead of the wrapper
            *ppvObject = m_resource.Get();
            return S_OK;
        }
    }

    WARN("Unknown interface %s.", debugstr_guid(&riid).c_str());
    return E_NOINTERFACE;
}

ULONG WrappedD3D12ToD3D11Resource::AddRef() {
    ULONG ref = InterlockedIncrement(&m_refCount);
    TRACE("%p increasing refcount to %u.", this, ref);
    return ref;
}

ULONG WrappedD3D12ToD3D11Resource::Release() {
    ULONG ref = InterlockedDecrement(&m_refCount);
    TRACE("%p decreasing refcount to %u.", this, ref);
    if (ref == 0) {
        delete this;
    }
    return ref;
}

// ID3D12Object methods
HRESULT WrappedD3D12ToD3D11Resource::GetPrivateData(REFGUID guid,
                                                        UINT* pDataSize,
                                                        void* pData) {
    TRACE("WrappedD3D12ToD3D11Resource::GetPrivateData called: %s, %p, %p",
          debugstr_guid(&guid).c_str(), pDataSize, pData);
    return m_resource->GetPrivateData(guid, pDataSize, pData);
}

HRESULT WrappedD3D12ToD3D11Resource::SetPrivateData(REFGUID guid,
                                                        UINT DataSize,
                                                        const void* pData) {
    TRACE("WrappedD3D12ToD3D11Resource::SetPrivateData %s, %u, %p",
          debugstr_guid(&guid).c_str(), DataSize, pData);
    return m_resource->SetPrivateData(guid, DataSize, pData);
}

HRESULT WrappedD3D12ToD3D11Resource::SetPrivateDataInterface(REFGUID guid, const IUnknown* pData) {
    TRACE("WrappedD3D12ToD3D11Resource::SetPrivateDataInterface %s, %p",
          debugstr_guid(&guid).c_str(), pData);
    return m_resource->SetPrivateDataInterface(guid, pData);
}

HRESULT WrappedD3D12ToD3D11Resource::SetName(LPCWSTR Name) {
    TRACE("WrappedD3D12ToD3D11Resource::SetName %s", debugstr_w(Name).c_str());
    return m_resource->SetPrivateData(
        WKPDID_D3DDebugObjectName,
        static_cast<UINT>((wcslen(Name) + 1) * sizeof(WCHAR)), Name);
}

// ID3D12DeviceChild methods
HRESULT WrappedD3D12ToD3D11Resource::GetDevice(REFIID riid,
                                                   void** ppvDevice) {
    TRACE("WrappedD3D12ToD3D11Resource::GetDevice %s, %p", debugstr_guid(&riid).c_str(),
          ppvDevice);
    return m_device->QueryInterface(riid, ppvDevice);
}

// ID3D12Resource methods
HRESULT WrappedD3D12ToD3D11Resource::Map(UINT Subresource,
                                             const D3D12_RANGE* pReadRange,
                                             void** ppData) {
    TRACE("WrappedD3D12ToD3D11Resource::Map %u, %p, %p", Subresource, pReadRange, ppData);

    if (!ppData) {
        ERR("Invalid ppData parameter");
        return E_INVALIDARG;
    }

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    D3D11_MAP mapType;

    // Determine map type based on heap properties and resource type
    if (m_desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
        if (m_heapProperties.Type == D3D12_HEAP_TYPE_UPLOAD) {
            mapType = D3D11_MAP_WRITE_NO_OVERWRITE;
        } else if (m_heapProperties.Type == D3D12_HEAP_TYPE_READBACK) {
            mapType = D3D11_MAP_READ;
        } else if (m_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) {
            mapType = D3D11_MAP_WRITE_DISCARD;
        } else {
            // For dynamic buffers, use NO_OVERWRITE for better performance
            mapType = D3D11_MAP_WRITE_NO_OVERWRITE;
        }
    } else {
        // For textures, use the default mapping strategy
        mapType = D3D11_MAP_WRITE_DISCARD;
    }
    
    TRACE("Mapping resource with type %d", mapType);
    HRESULT hr = m_device->GetD3D11Context()->Map(m_resource.Get(), Subresource,
                                                 mapType, 0,
                                                 &mappedResource);
    if (SUCCEEDED(hr)) {
        *ppData = mappedResource.pData;
        TRACE("Successfully mapped resource at %p", mappedResource.pData);
    } else {
        ERR("Failed to map resource with type %d, hr %#x", mapType, hr);
    }

    return hr;
}

void WrappedD3D12ToD3D11Resource::Unmap(UINT Subresource,
                                            const D3D12_RANGE* pWrittenRange) {
    TRACE("WrappedD3D12ToD3D11Resource::Unmap %u, %p", Subresource, pWrittenRange);
    m_device->GetD3D11Context()->Unmap(m_resource.Get(), Subresource);
}

D3D12_RESOURCE_DESC* WrappedD3D12ToD3D11Resource::GetDesc(D3D12_RESOURCE_DESC* pDesc) {
    if (pDesc) {
        TRACE("WrappedD3D12ToD3D11Resource::GetDesc(%p)", pDesc);
        TRACE("  Dimension: %d", m_desc.Dimension);
        TRACE("  Alignment: %llu", m_desc.Alignment);
        TRACE("  Width: %llu", m_desc.Width);
        TRACE("  Height: %u", m_desc.Height);
        TRACE("  DepthOrArraySize: %hu", m_desc.DepthOrArraySize);
        TRACE("  MipLevels: %hu", m_desc.MipLevels);
        TRACE("  Format: %d", m_desc.Format);
        TRACE("  SampleDesc.Count: %u", m_desc.SampleDesc.Count);
        TRACE("  SampleDesc.Quality: %u", m_desc.SampleDesc.Quality);
        TRACE("  Layout: %d", m_desc.Layout);
        *pDesc = m_desc;
    }
    return pDesc;
}

D3D12_GPU_VIRTUAL_ADDRESS WrappedD3D12ToD3D11Resource::GetGPUVirtualAddress() {
    TRACE("GetGPUVirtualAddress called for resource %p", this);
    
    // Only buffers should have GPU addresses
    if (m_desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER) {
        ERR("GetGPUVirtualAddress called on non-buffer resource type %d", m_desc.Dimension);
        return 0;
    }

    // Return existing GPU address if already allocated
    if (m_gpuAddress) {
        TRACE("Returning existing GPU address %llu for resource %p", m_gpuAddress, this);
        return m_gpuAddress;
    }

    // Get buffer description
    D3D11_BUFFER_DESC bufferDesc = {};
    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
    
    if (!m_resource) {
        ERR("No D3D11 resource available for %p", this);
        return 0;
    }
    
    if (FAILED(m_resource.As(&buffer))) {
        ERR("Failed to get ID3D11Buffer from resource %p", this);
        return 0;
    }

    buffer->GetDesc(&bufferDesc);
    TRACE("Getting GPU address for buffer - BindFlags: %#x, ByteWidth: %u, Usage: %d", 
          bufferDesc.BindFlags, bufferDesc.ByteWidth, bufferDesc.Usage);
              
    // Only allocate GPU address for shader-visible resources
    if (!(bufferDesc.BindFlags & (D3D11_BIND_VERTEX_BUFFER |
                                D3D11_BIND_INDEX_BUFFER |
                                D3D11_BIND_CONSTANT_BUFFER |
                                D3D11_BIND_SHADER_RESOURCE |
                                D3D11_BIND_UNORDERED_ACCESS))) {
        TRACE("Buffer %p does not need GPU address - bind flags %#x", this, bufferDesc.BindFlags);
        return 0;
    }
    
    m_gpuAddress = m_device->AllocateGPUVirtualAddress(this, bufferDesc.ByteWidth);
    
    if (!m_gpuAddress) {
        ERR("Failed to allocate GPU address for buffer resource %p", this);
        return 0;
    }
    
    TRACE("Allocated GPU address %llu for buffer resource %p", m_gpuAddress, this);
    return m_gpuAddress;
}

HRESULT WrappedD3D12ToD3D11Resource::WriteToSubresource(
    UINT DstSubresource, const D3D12_BOX* pDstBox, const void* pSrcData,
    UINT SrcRowPitch, UINT SrcDepthPitch) {
    TRACE("WrappedD3D12ToD3D11Resource::WriteToSubresource called %u, %p, %p, %u, %u",
          DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);

    m_device->GetD3D11Context()->UpdateSubresource(
        m_resource.Get(), DstSubresource,
        reinterpret_cast<const D3D11_BOX*>(pDstBox), pSrcData, SrcRowPitch,
        SrcDepthPitch);

    return S_OK;
}

HRESULT WrappedD3D12ToD3D11Resource::ReadFromSubresource(
    void* pDstData, UINT DstRowPitch, UINT DstDepthPitch, UINT SrcSubresource,
    const D3D12_BOX* pSrcBox) {
    TRACE("WrappedD3D12ToD3D11Resource::ReadFromSubresource %p, %u, %u, %u, %p", pDstData,
          DstRowPitch, DstDepthPitch, SrcSubresource, pSrcBox);

    // D3D11 doesn't have a direct equivalent for reading from a subresource
    // We need to create a staging resource and copy the data
    FIXME("ReadFromSubresource not implemented yet.");
    return E_NOTIMPL;
}

HRESULT WrappedD3D12ToD3D11Resource::GetHeapProperties(
    D3D12_HEAP_PROPERTIES* pHeapProperties, D3D12_HEAP_FLAGS* pHeapFlags) {
    TRACE("WrappedD3D12ToD3D11Resource::GetHeapProperties %p, %p", pHeapProperties,
          pHeapFlags);

    if (pHeapProperties) {
        *pHeapProperties = m_heapProperties;
    }
    if (pHeapFlags) {
        *pHeapFlags = m_heapFlags;
    }

    return S_OK;
}

}  // namespace dxiided
