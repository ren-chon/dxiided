#include "d3d11_impl/resource.hpp"

#include "d3d11_impl/device.hpp"

namespace dxiided {

HRESULT D3D11Resource::Create(D3D11Device* device,
                              const D3D12_HEAP_PROPERTIES* pHeapProperties,
                              D3D12_HEAP_FLAGS HeapFlags,
                              const D3D12_RESOURCE_DESC* pDesc,
                              D3D12_RESOURCE_STATES InitialState,
                              const D3D12_CLEAR_VALUE* pOptimizedClearValue,
                              REFIID riid, void** ppvResource) {
    TRACE("D3D11Resource::Create called with: %p, %p, %#x, %p, %#x, %p, %s, %p",
          device, pHeapProperties, HeapFlags, pDesc, InitialState,
          pOptimizedClearValue, debugstr_guid(&riid).c_str(), ppvResource);

    if (!device || !pHeapProperties || !pDesc || !ppvResource) {
        WARN(
            "Invalid parameters: device: %d, pHeapProperties: %p, pDesc: %d, "
            "ppvResource: %r",
            device, pHeapProperties, pDesc, ppvResource);
        return E_INVALIDARG;
    }

    Microsoft::WRL::ComPtr<D3D11Resource> resource = new D3D11Resource(
        device, pHeapProperties, HeapFlags, pDesc, InitialState);

    if (!resource->GetD3D11Resource()) {
        ERR("Failed to create D3D11 resource.");
        return E_FAIL;
    }

    return resource.CopyTo(reinterpret_cast<ID3D12Resource**>(ppvResource));
}

HRESULT D3D11Resource::Create(D3D11Device* device, ID3D11Resource* resource,
                              const D3D12_RESOURCE_DESC* pDesc,
                              D3D12_RESOURCE_STATES InitialState, REFIID riid,
                              void** ppvResource) {
    if (!device || !resource || !pDesc || !ppvResource) {
        WARN(
            "Invalid parameters: device=%p, resource=%p, pDesc=%p, "
            "ppvResource=%p",
            device, resource, pDesc, ppvResource);
        return E_INVALIDARG;
    }

    Microsoft::WRL::ComPtr<D3D11Resource> wrapper =
        new D3D11Resource(device, resource, pDesc, InitialState);

    if (!wrapper->GetD3D11Resource()) {
        ERR("Failed to wrap D3D11 resource.");
        return E_FAIL;
    }

    return wrapper.CopyTo(reinterpret_cast<ID3D12Resource**>(ppvResource));
}

D3D11Resource::D3D11Resource(D3D11Device* device,
                             const D3D12_HEAP_PROPERTIES* pHeapProperties,
                             D3D12_HEAP_FLAGS HeapFlags,
                             const D3D12_RESOURCE_DESC* pDesc,
                             D3D12_RESOURCE_STATES InitialState)
    : m_device(device),
      m_refCount(1),
      m_desc(*pDesc),
      m_heapProperties(*pHeapProperties),
      m_heapFlags(HeapFlags),
      m_currentState(InitialState),
      m_isUAV(false) {
    TRACE("Creating D3D11Resource with heap type %s",
          GetD3D12ResourceDimensionName(pDesc->Dimension));

    // Get D3D11 usage based on heap properties
    D3D11_USAGE usage = GetD3D11Usage(pHeapProperties, pDesc);
    
    // Get D3D11 bind flags
    UINT bindFlags = GetD3D11BindFlags(pDesc, pHeapProperties, m_isUAV);
    
    // Get CPU access flags based on usage
    UINT cpuAccessFlags = 0;
    if (usage == D3D11_USAGE_DYNAMIC) {
        cpuAccessFlags |= D3D11_CPU_ACCESS_WRITE;
    }
    if (usage == D3D11_USAGE_STAGING) {
        cpuAccessFlags |= D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
    }

    HRESULT createResult = S_OK;

    // Create the appropriate resource type based on dimension
    TRACE("Creating resource with dimension %s", GetD3D12ResourceDimensionName(pDesc->Dimension));
    switch (pDesc->Dimension) {
        case D3D12_RESOURCE_DIMENSION_UNKNOWN: {
            ERR("Unknown resource dimension specified");
            createResult = E_INVALIDARG;
            break;
        }
        case D3D12_RESOURCE_DIMENSION_BUFFER: {
            D3D11_BUFFER_DESC desc = {};
            desc.ByteWidth = static_cast<UINT>(pDesc->Width);
            desc.Usage = usage;
            desc.BindFlags = bindFlags;
            desc.CPUAccessFlags = cpuAccessFlags;
            desc.MiscFlags = GetMiscFlags(pDesc, m_isUAV);
            desc.StructureByteStride = 0;  // Set if structured buffer

            TRACE("Creating buffer: size=%u, usage=%s, bind_flags=%s, cpu_access=%s", 
                  desc.ByteWidth, GetD3D11UsageName(desc.Usage),
                  GetD3D11BindFlagsString(desc.BindFlags).c_str(),
                  GetD3D11CPUAccessFlagsString(desc.CPUAccessFlags).c_str());

            // Try different strategies for large buffers
            if (desc.ByteWidth > (128 * 1024 * 1024)) {  // > 128MB
                TRACE("Large buffer detected (%u MB), trying creation strategies", 
                      desc.ByteWidth / (1024 * 1024));
                createResult = CreateBufferDefault(desc);
                if (FAILED(createResult)) {
                    WARN("Default buffer creation failed, trying fallback");
                    createResult = CreateBufferWithFallback(desc);
                    if (FAILED(createResult)) {
                        WARN("Fallback creation failed, trying chunked creation");
                        createResult = CreateBufferInChunks(desc);
                        if (FAILED(createResult)) {
                            ERR("All buffer creation strategies failed");
                        }
                    }
                }
            } else {
                createResult = CreateBufferDefault(desc);
                if (FAILED(createResult)) {
                    ERR("Failed to create default buffer");
                }
            }
            break;
        }
        case D3D12_RESOURCE_DIMENSION_TEXTURE1D: {
            D3D11_TEXTURE1D_DESC desc = {};
            desc.Width = static_cast<UINT>(pDesc->Width);
            desc.MipLevels = pDesc->MipLevels;
            desc.ArraySize = pDesc->DepthOrArraySize;
            desc.Format = GetViewFormat(pDesc->Format);
            desc.Usage = usage;
            desc.BindFlags = bindFlags;
            desc.CPUAccessFlags = cpuAccessFlags;
            desc.MiscFlags = GetMiscFlags(pDesc, m_isUAV);
            
            HRESULT hr = m_device->GetD3D11Device()->CreateTexture1D(&desc, nullptr, 
                reinterpret_cast<ID3D11Texture1D**>(m_resource.GetAddressOf()));
            if (FAILED(hr)) {
                ERR("Failed to create 1D texture: width=%u, format=%s", 
                    desc.Width, GetDXGIFormatName(desc.Format));
                createResult = hr;
            }
            TRACE("Successfully created 1D texture");
            break;
        }
        case D3D12_RESOURCE_DIMENSION_TEXTURE2D: {
            D3D11_TEXTURE2D_DESC desc = {};
            desc.Width = static_cast<UINT>(pDesc->Width);
            desc.Height = pDesc->Height;
            desc.MipLevels = pDesc->MipLevels;
            desc.ArraySize = pDesc->DepthOrArraySize;
            desc.Format = GetViewFormat(pDesc->Format);
            desc.SampleDesc = pDesc->SampleDesc;
            desc.Usage = usage;
            desc.BindFlags = bindFlags;
            desc.CPUAccessFlags = cpuAccessFlags;
            desc.MiscFlags = GetMiscFlags(pDesc, m_isUAV);
            
            HRESULT hr = m_device->GetD3D11Device()->CreateTexture2D(&desc, nullptr,
                reinterpret_cast<ID3D11Texture2D**>(m_resource.GetAddressOf()));
            if (FAILED(hr)) {
                ERR("Failed to create 2D texture: width=%u, height=%u, format=%s", 
                    desc.Width, desc.Height, GetDXGIFormatName(desc.Format));
                createResult = hr;
            }
            TRACE("Successfully created 2D texture");
            break;
        }
        case D3D12_RESOURCE_DIMENSION_TEXTURE3D: {
            D3D11_TEXTURE3D_DESC desc = {};
            desc.Width = static_cast<UINT>(pDesc->Width);
            desc.Height = pDesc->Height;
            desc.Depth = pDesc->DepthOrArraySize;
            desc.MipLevels = pDesc->MipLevels;
            desc.Format = GetViewFormat(pDesc->Format);
            desc.Usage = usage;
            desc.BindFlags = bindFlags;
            desc.CPUAccessFlags = cpuAccessFlags;
            desc.MiscFlags = GetMiscFlags(pDesc, m_isUAV);
            
            HRESULT hr = m_device->GetD3D11Device()->CreateTexture3D(&desc, nullptr,
                reinterpret_cast<ID3D11Texture3D**>(m_resource.GetAddressOf()));
            if (FAILED(hr)) {
                ERR("Failed to create 3D texture: width=%u, height=%u, depth=%u, format=%s", 
                    desc.Width, desc.Height, desc.Depth, GetDXGIFormatName(desc.Format));
                createResult = hr;
            }
            TRACE("Successfully created 3D texture");
            break;
        }
    }

    if (SUCCEEDED(createResult)) {
        StoreInDeviceMap();
        TRACE("Successfully registered resource in device map");
    }

    if (FAILED(createResult)) {
        ERR("Failed to create D3D11 resource");
        m_resource.Reset();  // Ensure resource is null on failure
    }
}

D3D11Resource::D3D11Resource(D3D11Device* device, ID3D11Resource* resource,
                             const D3D12_RESOURCE_DESC* pDesc,
                             D3D12_RESOURCE_STATES InitialState)
    : m_device(device), m_desc(*pDesc), m_currentState(InitialState) {
           // TODO:

}

UINT D3D11Resource::GetMiscFlags(const D3D12_RESOURCE_DESC* pDesc, bool isUAV) {
    UINT flags = 0;
    
    // Handle structured buffers
    if (pDesc->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
        if (isUAV) {
            flags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        }
    }
    
    // Handle texture flags
    if (pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS) {
        flags |= D3D11_RESOURCE_MISC_SHARED;
    }
    
    if (pDesc->Layout == D3D12_TEXTURE_LAYOUT_ROW_MAJOR) {
        flags |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
    }
    
    return flags;
}

D3D11_USAGE D3D11Resource::GetD3D11Usage(
    const D3D12_HEAP_PROPERTIES* pHeapProperties,
    const D3D12_RESOURCE_DESC* pDesc) {
    TRACE("D3D11Resource::GetD3D11Usage called");
    
    // Default to D3D11_USAGE_DEFAULT
    D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
    
    switch (pHeapProperties->Type) {
        case D3D12_HEAP_TYPE_DEFAULT:
            usage = D3D11_USAGE_DEFAULT;
            break;
            
        case D3D12_HEAP_TYPE_UPLOAD:
            usage = D3D11_USAGE_DYNAMIC;
            break;
            
        case D3D12_HEAP_TYPE_READBACK:
            usage = D3D11_USAGE_STAGING;
            break;
            
        case D3D12_HEAP_TYPE_CUSTOM:
            // For custom heaps, check CPU page properties
            switch (pHeapProperties->CPUPageProperty) {
                case D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE:
                    usage = D3D11_USAGE_DEFAULT;
                    break;
                    
                case D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE:
                case D3D12_CPU_PAGE_PROPERTY_WRITE_BACK:
                    usage = D3D11_USAGE_DYNAMIC;
                    break;
                    
                default:
                    WARN("Unsupported CPU page property %d", 
                         pHeapProperties->CPUPageProperty);
                    usage = D3D11_USAGE_DEFAULT;
                    break;
            }
            break;
    }
    
    return usage;
}

D3D11_BIND_FLAG D3D11Resource::GetD3D11BindFlags(
    const D3D12_RESOURCE_DESC* pDesc,
    const D3D12_HEAP_PROPERTIES* pHeapProperties,
    bool& isUAV) {
    TRACE("D3D11Resource::GetD3D11BindFlags called");
    
    UINT flags = 0;
    
    // Check resource flags
    if (!(pDesc->Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)) {
        flags |= D3D11_BIND_SHADER_RESOURCE;
    }
    
    if (pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) {
        flags |= D3D11_BIND_RENDER_TARGET;
    }
    
    if (pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) {
        flags |= D3D11_BIND_DEPTH_STENCIL;
    }
    
    if (pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) {
        flags |= D3D11_BIND_UNORDERED_ACCESS;
        isUAV = true;
    }
    
    // For buffers, add vertex and index buffer bindings if appropriate
    if (pDesc->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
        if (pHeapProperties->Type != D3D12_HEAP_TYPE_READBACK) {
            flags |= D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_INDEX_BUFFER;
        }
    }
    
    return static_cast<D3D11_BIND_FLAG>(flags);
}

HRESULT D3D11Resource::CreateBufferDefault(const D3D11_BUFFER_DESC& desc) {
    TRACE("Creating buffer with DEFAULT strategy (size=%u bytes, usage=%s)",
          desc.ByteWidth, GetD3D11UsageName(desc.Usage));
    
    HRESULT hr = m_device->GetD3D11Device()->CreateBuffer(
        &desc, nullptr, reinterpret_cast<ID3D11Buffer**>(m_resource.GetAddressOf()));
        
    if (FAILED(hr)) {
        ERR("Failed to create default buffer with usage %s", GetD3D11UsageName(desc.Usage));
    }
    return hr;
}

HRESULT D3D11Resource::CreateBufferWithFallback(const D3D11_BUFFER_DESC& desc) {
    TRACE("Creating buffer with FALLBACK strategy (size=%u bytes, original_usage=%s)",
          desc.ByteWidth, GetD3D11UsageName(desc.Usage));
          
    // Try with reduced bind flags
    D3D11_BUFFER_DESC modifiedDesc = desc;
    modifiedDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;  // Minimum required
    
    TRACE("Attempting creation with reduced bind flags: %s", 
          GetD3D11BindFlagsString(modifiedDesc.BindFlags).c_str());
    
    HRESULT hr = m_device->GetD3D11Device()->CreateBuffer(
        &modifiedDesc, nullptr, 
        reinterpret_cast<ID3D11Buffer**>(m_resource.GetAddressOf()));
        
    if (SUCCEEDED(hr)) {
        WARN("Buffer created with reduced bind flags - some functionality may be limited");
        return S_OK;
    }
    
    // Try with dynamic usage
    modifiedDesc.Usage = D3D11_USAGE_DYNAMIC;
    modifiedDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    
    TRACE("Attempting creation with dynamic usage");
    hr = m_device->GetD3D11Device()->CreateBuffer(
        &modifiedDesc, nullptr,
        reinterpret_cast<ID3D11Buffer**>(m_resource.GetAddressOf()));
        
    if (FAILED(hr)) {
        ERR("All fallback attempts failed");
    }
    return hr;
}

HRESULT D3D11Resource::CreateBufferInChunks(const D3D11_BUFFER_DESC& desc) {
    TRACE("Creating buffer with SPLIT_CHUNKS strategy (size=%u bytes)",
          desc.ByteWidth);

    const UINT64 chunkSize = 128 * 1024 * 1024;  // 128MB chunks
    UINT64 remainingSize = desc.ByteWidth;
    UINT64 offset = 0;
    
    while (remainingSize > 0) {
        D3D11_BUFFER_DESC chunkDesc = desc;
        chunkDesc.ByteWidth = static_cast<UINT>(
            std::min(remainingSize, chunkSize));
            
        Microsoft::WRL::ComPtr<ID3D11Buffer> chunk;
        HRESULT hr = m_device->GetD3D11Device()->CreateBuffer(
            &chunkDesc, nullptr, &chunk);
            
        if (FAILED(hr)) {
            ERR("Failed to create buffer chunk at offset %llu", offset);
            return hr;
        }
        
        ChunkInfo info;
        info.offset = offset;
        info.size = chunkDesc.ByteWidth;
        info.buffer = chunk;
        m_chunks.push_back(info);
        
        offset += chunkDesc.ByteWidth;
        remainingSize -= chunkDesc.ByteWidth;
    }
    
    return S_OK;
}

HRESULT D3D11Resource::GetBufferDesc(D3D11_BUFFER_DESC* desc) const {
    if (!desc) return E_INVALIDARG;

    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
    HRESULT hr = m_resource.As(&buffer);
    if (FAILED(hr)) return hr;

    buffer->GetDesc(desc);
    return S_OK;
}

HRESULT D3D11Resource::Map(UINT Subresource,
                        const D3D12_RANGE* pReadRange,
                        void** ppData) {
    TRACE("Mapping resource: subresource=%u, read_range=%p", 
          Subresource, pReadRange);
    
    if (!ppData) {
        ERR("Invalid ppData parameter");
        return E_INVALIDARG;
    }
    
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    m_device->GetD3D11Device()->GetImmediateContext(&context);
    if (!context) {
        ERR("Failed to get immediate context");
        return E_FAIL;
    }
    
    D3D11_MAPPED_SUBRESOURCE mappedResource = {};
    
    // For chunked buffers, map the appropriate chunk
    if (!m_chunks.empty()) {
        TRACE("Mapping chunked buffer with %zu chunks", m_chunks.size());
        
        // Get the first chunk's properties
        D3D11_BUFFER_DESC chunkDesc;
        m_chunks[0].buffer->GetDesc(&chunkDesc);
        
        // Determine appropriate mapping type
        D3D11_MAP mapType = D3D11_MAP_WRITE;
        if (chunkDesc.Usage == D3D11_USAGE_DYNAMIC) {
            mapType = D3D11_MAP_WRITE_DISCARD;
        }
        
        // For upload buffers, only map the first chunk
        HRESULT hr = context->Map(m_chunks[0].buffer.Get(), 0, 
                                mapType, 0, &mappedResource);
        if (SUCCEEDED(hr)) {
            *ppData = mappedResource.pData;
            TRACE("Successfully mapped chunk with type %d", mapType);
            return S_OK;
        }
        
        ERR("Failed to map chunk with type %d (hr=%08X)", mapType, hr);
        return hr;
    }
    
    // For regular resources
    D3D11_MAP mapType = D3D11_MAP_WRITE;
    UINT mapFlags = 0;
    
    // Determine map type based on usage
    D3D11_BUFFER_DESC desc;
    if (SUCCEEDED(GetBufferDesc(&desc))) {
        switch (desc.Usage) {
            case D3D11_USAGE_DYNAMIC:
                mapType = D3D11_MAP_WRITE_DISCARD;
                break;
            case D3D11_USAGE_STAGING:
                mapType = D3D11_MAP_READ_WRITE;
                break;
            default:
                mapType = D3D11_MAP_WRITE;
                break;
        }
        TRACE("Using map type %s for usage %s", 
              GetD3D11MapTypeName(mapType), GetD3D11UsageName(desc.Usage));
    } else {
        WARN("Failed to get buffer description, defaulting to MAP_WRITE");
    }
    
    HRESULT hr = context->Map(m_resource.Get(), Subresource, mapType,
                             mapFlags, &mappedResource);
    if (FAILED(hr)) {
        ERR("Failed to map resource with type %s", GetD3D11MapTypeName(mapType));
        return hr;
    }
    
    TRACE("Successfully mapped resource");
    *ppData = mappedResource.pData;
    return S_OK;
}

void D3D11Resource::UAVBarrier(ID3D11DeviceContext* context) {
    TRACE("D3D11Resource::UAVBarrier %p", context);
          // TODO:

}

void D3D11Resource::AliasingBarrier(ID3D11DeviceContext* context,
                                    D3D11Resource* pResourceAfter) {
    TRACE("D3D11Resource::AliasingBarrier %p, %p", context, pResourceAfter);
          // TODO:


}

DXGI_FORMAT D3D11Resource::GetViewFormat(DXGI_FORMAT format) {
    TRACE("Getting view format for %s", GetDXGIFormatName(format));
    FIXME("Implement format conversion for typeless formats");
    return format;
}

void D3D11Resource::StoreInDeviceMap() {
    // Store the mapping between D3D12 and D3D11 resources
    TRACE("Storing resource mapping - D3D11: %p, D3D12: %p", m_resource.Get(), this);
    
    // Set private data to allow lookup of D3D11 resource from D3D12 resource
    GUID resourceMapGuid = { 0xdc8e63f3, 0xd12b, 0x4952, { 0xb4, 0x7b, 0x5e, 0x45, 0x02, 0x6a, 0x86, 0x2d } };
    ID3D11Resource* d3d11Resource = m_resource.Get();
    UINT dataSize = sizeof(ID3D11Resource*);
    
    if (FAILED(SetPrivateData(resourceMapGuid, dataSize, &d3d11Resource))) {
        ERR("Failed to store D3D11 resource mapping");
    }
}

// IUnknown methods
HRESULT STDMETHODCALLTYPE D3D11Resource::QueryInterface(REFIID riid,
                                                        void** ppvObject) {
    TRACE("D3D11Resource::QueryInterface called: %s, %p",
          debugstr_guid(&riid).c_str(), ppvObject);

    if (!ppvObject) {
        return E_POINTER;
    }

    if (riid == __uuidof(ID3D12Resource) || riid == __uuidof(IUnknown)) {
        TRACE("D3D11Resource::QueryInterface returning ID3D12Resource");
        AddRef();
        *ppvObject = this;
        return S_OK;
    }

    WARN("Unknown interface %s.", debugstr_guid(&riid).c_str());
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE D3D11Resource::AddRef() {
    ULONG ref = InterlockedIncrement(&m_refCount);
    TRACE("%p increasing refcount to %u.", this, ref);
    return ref;
}

ULONG STDMETHODCALLTYPE D3D11Resource::Release() {
    ULONG ref = InterlockedDecrement(&m_refCount);
    TRACE("%p decreasing refcount to %u.", this, ref);
    if (ref == 0) {
        delete this;
    }
    return ref;
}

// ID3D12Object methods
HRESULT STDMETHODCALLTYPE D3D11Resource::GetPrivateData(REFGUID guid,
                                                        UINT* pDataSize,
                                                        void* pData) {
    TRACE("D3D11Resource::GetPrivateData called: %s, %p, %p",
          debugstr_guid(&guid).c_str(), pDataSize, pData);
    return m_resource->GetPrivateData(guid, pDataSize, pData);
}

HRESULT STDMETHODCALLTYPE D3D11Resource::SetPrivateData(REFGUID guid,
                                                        UINT DataSize,
                                                        const void* pData) {
    TRACE("D3D11Resource::SetPrivateData %s, %u, %p",
          debugstr_guid(&guid).c_str(), DataSize, pData);
    return m_resource->SetPrivateData(guid, DataSize, pData);
}

HRESULT STDMETHODCALLTYPE
D3D11Resource::SetPrivateDataInterface(REFGUID guid, const IUnknown* pData) {
    TRACE("D3D11Resource::SetPrivateDataInterface %s, %p",
          debugstr_guid(&guid).c_str(), pData);
    return m_resource->SetPrivateDataInterface(guid, pData);
}

HRESULT STDMETHODCALLTYPE D3D11Resource::SetName(LPCWSTR Name) {
    if (!Name) {
        TRACE("D3D11Resource::SetName called with null name");
        return E_INVALIDARG;
    }

    try {
        TRACE("D3D11Resource::SetName %s", debugstr_w(Name).c_str());
        UINT nameSize = static_cast<UINT>((wcslen(Name) + 1) * sizeof(WCHAR));
        return m_resource->SetPrivateData(WKPDID_D3DDebugObjectName, nameSize,
                                          Name);
    } catch (...) {
        TRACE("D3D11Resource::SetName failed with exception");
        return E_FAIL;
    }
}

// ID3D12DeviceChild methods
HRESULT STDMETHODCALLTYPE D3D11Resource::GetDevice(REFIID riid,
                                                   void** ppvDevice) {
    TRACE("D3D11Resource::GetDevice %s, %p", debugstr_guid(&riid).c_str(),
          ppvDevice);
    return m_device->QueryInterface(riid, ppvDevice);
}

// ID3D12Resource methods
void STDMETHODCALLTYPE D3D11Resource::Unmap(UINT Subresource,
                                            const D3D12_RANGE* pWrittenRange) {
    TRACE("D3D11Resource::Unmap %u, %p", Subresource, pWrittenRange);
    m_device->GetD3D11Context()->Unmap(m_resource.Get(), Subresource);
}

D3D12_RESOURCE_DESC* STDMETHODCALLTYPE
D3D11Resource::GetDesc(D3D12_RESOURCE_DESC* pDesc) {
    if (pDesc) {
        TRACE("D3D11Resource::GetDesc(%p)", pDesc);
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

D3D12_GPU_VIRTUAL_ADDRESS STDMETHODCALLTYPE
D3D11Resource::GetGPUVirtualAddress() {
    TRACE("D3D11Resource::GetGPUVirtualAddress called");
    return m_gpuAddress;
}

HRESULT STDMETHODCALLTYPE D3D11Resource::WriteToSubresource(
    UINT DstSubresource, const D3D12_BOX* pDstBox, const void* pSrcData,
    UINT SrcRowPitch, UINT SrcDepthPitch) {
    TRACE("D3D11Resource::WriteToSubresource called %u, %p, %p, %u, %u",
          DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);

    m_device->GetD3D11Context()->UpdateSubresource(
        m_resource.Get(), DstSubresource,
        reinterpret_cast<const D3D11_BOX*>(pDstBox), pSrcData, SrcRowPitch,
        SrcDepthPitch);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE D3D11Resource::ReadFromSubresource(
    void* pDstData, UINT DstRowPitch, UINT DstDepthPitch, UINT SrcSubresource,
    const D3D12_BOX* pSrcBox) {
    TRACE("D3D11Resource::ReadFromSubresource %p, %u, %u, %u, %p", pDstData,
          DstRowPitch, DstDepthPitch, SrcSubresource, pSrcBox);

    // D3D11 doesn't have a direct equivalent for reading from a subresource
    // We need to create a staging resource and copy the data
    FIXME("ReadFromSubresource not implemented yet.");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Resource::GetHeapProperties(
    D3D12_HEAP_PROPERTIES* pHeapProperties, D3D12_HEAP_FLAGS* pHeapFlags) {
    TRACE("D3D11Resource::GetHeapProperties %p, %p", pHeapProperties,
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
