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
      m_heapProperties(*pHeapProperties),
      m_heapFlags(HeapFlags),
      m_desc(*pDesc),
      m_currentState(InitialState),
      m_isUAV(false),
      m_refCount(1) {
    TRACE("Creating D3D11Resource with heap type %s",
          GetD3D12HeapTypeName(pHeapProperties->Type));
    TRACE("Creating resource format=%d, width=%llu, height=%u", pDesc->Format,
          pDesc->Width, pDesc->Height);

    // Validate buffer size for UPLOAD heaps
    if (pHeapProperties->Type == D3D12_HEAP_TYPE_UPLOAD &&
        pDesc->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER &&
        pDesc->Width > 256 * 1024 * 1024) {  // 256MB limit
        WARN("Large UPLOAD buffer requested (%llu bytes), may fail to map",
             pDesc->Width);
    }

    D3D11_BIND_FLAG bindFlags = GetD3D11BindFlags(pDesc, pHeapProperties);
    D3D11_USAGE usage = GetD3D11Usage(pHeapProperties, pDesc);

    switch (pDesc->Dimension) {
        case D3D12_RESOURCE_DIMENSION_BUFFER: {
            TRACE("D3D12_RESOURCE_DIMENSION_BUFFER match");
            D3D11_BUFFER_DESC bufferDesc = {};
            bufferDesc.ByteWidth = static_cast<UINT>(pDesc->Width);
            bufferDesc.Usage = usage;
            bufferDesc.BindFlags = bindFlags;
            bufferDesc.CPUAccessFlags =
                (usage == D3D11_USAGE_DYNAMIC) ? D3D11_CPU_ACCESS_WRITE
                : (usage == D3D11_USAGE_STAGING)
                    ? (D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE)
                    : 0;
            bufferDesc.MiscFlags = GetMiscFlags(pDesc);
            bufferDesc.StructureByteStride = 0;

            // For large UPLOAD buffers, use configured strategy
            HRESULT hr = S_OK;
            if (pHeapProperties->Type == D3D12_HEAP_TYPE_UPLOAD &&
                pDesc->Width > 64 * 1024 * 1024) {  // 64MB threshold

                // TODO: Get from config file later
                // For now, use SPLIT_CHUNKS for very large buffers
                LargeBufferStrategy strategy =
                    (pDesc->Width > 256 * 1024 * 1024)
                        ? LargeBufferStrategy::SPLIT_CHUNKS
                        : LargeBufferStrategy::FALLBACK;

                TRACE("Large UPLOAD buffer detected, using %s strategy",
                      GetStrategyName(strategy));

                switch (strategy) {
                    case LargeBufferStrategy::DEFAULT:
                        hr = CreateBufferDefault(bufferDesc);
                        break;

                    case LargeBufferStrategy::FALLBACK:
                        hr = CreateBufferWithFallback(bufferDesc);
                        break;

                    case LargeBufferStrategy::SPLIT_CHUNKS:
                        hr = CreateBufferInChunks(bufferDesc);
                        break;

                    default:
                        ERR("Unknown buffer creation strategy");
                        return;
                }

                if (FAILED(hr)) {
                    ERR("Failed to create large buffer with %s strategy, hr "
                        "%#x",
                        GetStrategyName(strategy), hr);
                    return;
                }

                return;
            }

            // Normal buffer creation for smaller buffers
            hr = CreateBufferDefault(bufferDesc);
            if (FAILED(hr)) {
                ERR("Failed to create buffer, hr %#x.", hr);
                return;
            }
            break;
        }
        case D3D12_RESOURCE_DIMENSION_TEXTURE1D: {
            TRACE("D3D12_RESOURCE_DIMENSION_TEXTURE1D match");
            D3D11_TEXTURE1D_DESC texDesc = {};
            texDesc.Width = static_cast<UINT>(pDesc->Width);
            texDesc.MipLevels = pDesc->MipLevels;
            texDesc.ArraySize = pDesc->DepthOrArraySize;
            texDesc.Format = GetViewFormat(pDesc->Format);
            texDesc.Usage = usage;
            texDesc.BindFlags = bindFlags;
            texDesc.CPUAccessFlags =
                (usage == D3D11_USAGE_DYNAMIC) ? D3D11_CPU_ACCESS_WRITE
                : (usage == D3D11_USAGE_STAGING)
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
            texDesc.Format = GetViewFormat(pDesc->Format);
            texDesc.SampleDesc = pDesc->SampleDesc;
            texDesc.Usage = usage;
            texDesc.BindFlags = bindFlags;
            texDesc.CPUAccessFlags =
                (usage == D3D11_USAGE_DYNAMIC) ? D3D11_CPU_ACCESS_WRITE
                : (usage == D3D11_USAGE_STAGING)
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
            texDesc.Format = GetViewFormat(pDesc->Format);
            texDesc.Usage = usage;
            texDesc.BindFlags = bindFlags;
            texDesc.CPUAccessFlags =
                (usage == D3D11_USAGE_DYNAMIC) ? D3D11_CPU_ACCESS_WRITE
                : (usage == D3D11_USAGE_STAGING)
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

D3D11Resource::D3D11Resource(D3D11Device* device, ID3D11Resource* resource,
                             const D3D12_RESOURCE_DESC* pDesc,
                             D3D12_RESOURCE_STATES InitialState)
    : m_device(device), m_desc(*pDesc), m_currentState(InitialState) {
    if (resource) {
        m_resource = resource;
    }
}

void D3D11Resource::StoreInDeviceMap() {
    try {
        if (m_device && m_resource) {
            // Store the mapping between D3D12 and D3D11 resources in the device
            Microsoft::WRL::ComPtr<ID3D11Device> d3d11Device;
            if (SUCCEEDED(m_device->QueryInterface(
                    __uuidof(ID3D11Device),
                    reinterpret_cast<void**>(d3d11Device.GetAddressOf())))) {
                // Store this D3D12 wrapper as private data on the D3D11
                // resource
                m_resource->SetPrivateDataInterface(
                    __uuidof(ID3D12Resource),
                    static_cast<ID3D12Resource*>(this));

                // Also store the D3D11 resource as private data on this D3D12
                // wrapper
                SetPrivateDataInterface(__uuidof(ID3D11Resource),
                                        m_resource.Get());

                TRACE("Stored D3D11<->D3D12 resource mapping for %p <-> %p",
                      this, m_resource.Get());
            }
        }
    } catch (...) {
        // Log error but don't fail - this is diagnostic only
        TRACE("Failed to store resource mapping");
    }
}

UINT D3D11Resource::GetMiscFlags(const D3D12_RESOURCE_DESC* pDesc) {
    TRACE("GetMiscFlags called");
    TRACE("  Alignment: %#x", pDesc->Alignment);
    TRACE("  Dimension: %d", pDesc->Dimension);
    TRACE("  DepthOrArraySize: %d", pDesc->DepthOrArraySize);
    TRACE("  MipLevels: %d", pDesc->MipLevels);
    TRACE("  Format: %x", pDesc->Format);
    TRACE("  Flags: %x", pDesc->Flags);
    TRACE("  Height: %x", pDesc->Height);
    TRACE("  Width: %x", pDesc->Width);
    TRACE("  SampleDesc Count: %d", pDesc->SampleDesc.Count);
    TRACE("  SampleDesc Quality: %d", pDesc->SampleDesc.Quality);

    UINT flags = 0;

    // Handle shared resources
    if (pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS) {
        TRACE("Resource has D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS");
        flags |= D3D11_RESOURCE_MISC_SHARED;
        
        // For textures, also set shared keyedmutex
        if (pDesc->Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
            flags |= D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
        }
    }

    // Handle structured buffer
    if (pDesc->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER &&
        (pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)) {
        flags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    }

    // Handle texture cube
    if (pDesc->Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D &&
        pDesc->DepthOrArraySize == 6) {
        flags |= D3D11_RESOURCE_MISC_TEXTURECUBE;
    }

    TRACE("GetMiscFlags returns 0x%x", flags);
    return flags;
}

void D3D11Resource::TransitionTo(ID3D11DeviceContext* context,
                                 D3D12_RESOURCE_STATES newState) {
    TRACE("D3D11Resource::TransitionTo %p, %#x -> %#x", context, m_currentState,
          newState);

    // No need to transition if states are the same
    if (m_currentState == newState) {
        return;
    }

    // For dynamic buffers, we need to ensure proper state transitions
    D3D11_RESOURCE_DIMENSION dim;
    m_resource->GetType(&dim);
    if (dim == D3D11_RESOURCE_DIMENSION_BUFFER) {
        Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
        if (SUCCEEDED(m_resource.As(&buffer))) {
            D3D11_BUFFER_DESC desc;
            buffer->GetDesc(&desc);
            if (desc.Usage == D3D11_USAGE_DYNAMIC) {
                // For dynamic buffers, we need to ensure proper synchronization
                // when transitioning to/from GENERIC_READ state
                if (newState == D3D12_RESOURCE_STATE_GENERIC_READ ||
                    m_currentState == D3D12_RESOURCE_STATE_GENERIC_READ) {
                    TRACE("Dynamic buffer state transition %s -> %s",
                          GetD3D12ResourceStateString(m_currentState).c_str(),
                          GetD3D12ResourceStateString(newState).c_str());
                    context->Flush();
                }
            }
        }
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

HRESULT D3D11Resource::CreateBufferDefault(const D3D11_BUFFER_DESC& desc) {
    TRACE("Creating buffer with DEFAULT strategy (size=%u bytes)",
          desc.ByteWidth);

    D3D11_BUFFER_DESC modifiedDesc = desc;
    
    // For UAV buffers, we need to:
    // 1. Add structured buffer support if not a raw buffer
    // 2. Set a valid structure stride (default to 4 bytes)
    if (desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) {
        TRACE("UAV buffer");
        if (!(modifiedDesc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS)) {
            TRACE("Adding structured buffer support");
            modifiedDesc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
            modifiedDesc.StructureByteStride = 4;  // Default to 4-byte stride
        }
    }

    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
    HRESULT hr =
        m_device->GetD3D11Device()->CreateBuffer(&modifiedDesc, nullptr, &buffer);

    if (SUCCEEDED(hr)) {
        TRACE("Created buffer");
        m_resource = buffer;
        StoreInDeviceMap();
    }

    return hr;
}

HRESULT D3D11Resource::CreateBufferWithFallback(const D3D11_BUFFER_DESC& desc) {
    TRACE("Creating buffer with FALLBACK strategy (size=%u bytes)",
          desc.ByteWidth);

    // First try: Default settings
    HRESULT hr = CreateBufferDefault(desc);
    if (SUCCEEDED(hr)) {
        return hr;
    }

    // Second try: Add raw view support
    TRACE("Default creation failed, trying with raw view support");
    D3D11_BUFFER_DESC modifiedDesc = desc;
    modifiedDesc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;

    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
    hr = m_device->GetD3D11Device()->CreateBuffer(&modifiedDesc, nullptr,
                                                  &buffer);
    if (SUCCEEDED(hr)) {
        m_resource = buffer;
        StoreInDeviceMap();
        return hr;
    }

    // Third try: Reduce size to 256MB
    if (desc.ByteWidth > 256 * 1024 * 1024) {
        TRACE("Raw view attempt failed, reducing size to 256MB");
        modifiedDesc.ByteWidth = 256 * 1024 * 1024;
        hr = m_device->GetD3D11Device()->CreateBuffer(&modifiedDesc, nullptr,
                                                      &buffer);
        if (SUCCEEDED(hr)) {
            m_resource = buffer;
            StoreInDeviceMap();
            WARN("Created buffer with reduced size (256MB)");
        }
    }

    return hr;
}

HRESULT D3D11Resource::CreateBufferInChunks(const D3D11_BUFFER_DESC& desc) {
    TRACE("Creating buffer with SPLIT_CHUNKS strategy (size=%u bytes)",
          desc.ByteWidth);

    const UINT64 chunkSize = 128 * 1024 * 1024;  // 128MB chunks
    UINT64 remainingSize = desc.ByteWidth;
    UINT64 currentOffset = 0;

    while (remainingSize > 0) {
        UINT64 currentChunkSize = std::min(remainingSize, chunkSize);

        D3D11_BUFFER_DESC chunkDesc = desc;
        chunkDesc.ByteWidth = static_cast<UINT>(currentChunkSize);

        Microsoft::WRL::ComPtr<ID3D11Buffer> chunkBuffer;
        HRESULT hr = m_device->GetD3D11Device()->CreateBuffer(
            &chunkDesc, nullptr, &chunkBuffer);

        if (FAILED(hr)) {
            ERR("Failed to create chunk buffer %llu (size=%llu), hr %#x",
                m_chunks.size(), currentChunkSize, hr);
            return hr;
        }

        ChunkInfo chunk;
        chunk.offset = currentOffset;
        chunk.size = currentChunkSize;
        chunk.buffer = chunkBuffer;
        m_chunks.push_back(chunk);

        TRACE("Created chunk %zu: offset=%llu, size=%llu", m_chunks.size() - 1,
              currentOffset, currentChunkSize);

        currentOffset += currentChunkSize;
        remainingSize -= currentChunkSize;
    }

    // Store the first chunk as the main resource
    if (!m_chunks.empty()) {
        m_resource = m_chunks[0].buffer;
        StoreInDeviceMap();
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
    TRACE("D3D11Resource::Map %d, %p", Subresource, pReadRange);
    TRACE("Current resource state: %s", GetD3D12ResourceStateName(m_currentState));

    // Don't allow mapping of shared resources directly
    if (m_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS) {
        return E_INVALIDARG;
    }

    D3D11_BUFFER_DESC desc;
    GetBufferDesc(&desc);  

    TRACE("Buffer properties: Usage=%s, ByteWidth=%u, CPUAccessFlags=%#x, BindFlags=%#x",
          GetD3D11UsageName(desc.Usage), desc.ByteWidth,
          desc.CPUAccessFlags, desc.BindFlags);

    // For DEFAULT buffers, we need to use a staging buffer
    if (desc.Usage == D3D11_USAGE_DEFAULT) {
        if (!m_stagingBuffer) {
            // Create staging buffer if it doesn't exist
            D3D11_BUFFER_DESC stagingDesc = desc;
            stagingDesc.Usage = D3D11_USAGE_STAGING;
            stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            stagingDesc.BindFlags = 0;
            stagingDesc.MiscFlags &= ~D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

            HRESULT hr = m_device->GetD3D11Device()->CreateBuffer(
                &stagingDesc, nullptr, &m_stagingBuffer);
            if (FAILED(hr))
                return hr;
        }

        // Map the staging buffer instead
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
        m_device->GetD3D11Device()->GetImmediateContext(&context);
        HRESULT hr = context->Map(
            m_stagingBuffer.Get(), Subresource, D3D11_MAP_WRITE_DISCARD,
            0, &mappedResource);
        if (SUCCEEDED(hr))
            *ppData = mappedResource.pData;
        return hr;
    }

    // For other buffer types, proceed with normal mapping
    if (m_chunks.empty()) {
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
        m_device->GetD3D11Device()->GetImmediateContext(&context);
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT hr = context->Map(
            m_resource.Get(), Subresource, D3D11_MAP_WRITE_DISCARD,
            0, &mappedResource);
        if (SUCCEEDED(hr))
            *ppData = mappedResource.pData;
        return hr;
    } else {
        // Handle chunked buffer mapping
        TRACE("Mapping chunked buffer with %zu chunks", m_chunks.size());
        
        // Map first chunk
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
        m_device->GetD3D11Device()->GetImmediateContext(&context);
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT hr = context->Map(
            m_chunks[0].buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
            &mappedResource);
            
        if (SUCCEEDED(hr)) {
            *ppData = mappedResource.pData;
            TRACE("Successfully mapped first chunk (size=%u)", 
                  desc.ByteWidth / m_chunks.size());
        }
                  
        return hr;
    }
}

void D3D11Resource::UAVBarrier(ID3D11DeviceContext* context) {
    TRACE("D3D11Resource::UAVBarrier %p", context);

    if (m_isUAV) {
        // Ensure UAV writes are completed
        context->CSSetUnorderedAccessViews(0, 1, nullptr, nullptr);
        context->Flush();
    }
}

void D3D11Resource::AliasingBarrier(ID3D11DeviceContext* context,
                                    D3D11Resource* pResourceAfter) {
    TRACE("D3D11Resource::AliasingBarrier %p, %p", context, pResourceAfter);

    // Ensure all operations on both resources are completed
    context->Flush();
}

D3D11_BIND_FLAG D3D11Resource::GetD3D11BindFlags(
    const D3D12_RESOURCE_DESC* pDesc,
    const D3D12_HEAP_PROPERTIES* pHeapProperties) {
    TRACE("D3D11Resource::GetD3D11BindFlags called");
    
    UINT flags = 0;

    // For shared resources, we need additional bind flags
    if (pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS) {
        flags |= D3D11_BIND_SHADER_RESOURCE;
        // For textures, also allow render target binding
        if (pDesc->Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
            flags |= D3D11_BIND_RENDER_TARGET;
        }
    }

    // Handle render target flag
    if (pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) {
        flags |= D3D11_BIND_RENDER_TARGET;
        // For render targets, also allow shader resource if not explicitly denied
        if (!(pDesc->Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)) {
            flags |= D3D11_BIND_SHADER_RESOURCE;
        }
    }

    // Handle depth stencil flag
    if (pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) {
        flags |= D3D11_BIND_DEPTH_STENCIL;
    }

    // Handle unordered access flag
    if (pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) {
        flags |= D3D11_BIND_UNORDERED_ACCESS;
        flags |= D3D11_BIND_SHADER_RESOURCE;  // Include SRV binding for UAV buffers
    }

    // For upload heaps, allow vertex/index/constant buffer binding
    if (pHeapProperties && pHeapProperties->Type == D3D12_HEAP_TYPE_UPLOAD) {
        flags |= (D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_INDEX_BUFFER | D3D11_BIND_CONSTANT_BUFFER);
    }

    // For default heaps with no other flags and not denied shader resource, allow shader resource
    if (flags == 0 && pHeapProperties && 
        pHeapProperties->Type == D3D12_HEAP_TYPE_DEFAULT &&
        !(pDesc->Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)) {
        flags |= D3D11_BIND_SHADER_RESOURCE;
    }

    TRACE("Resource flags: 0x%x -> D3D11 bind flags: 0x%x", pDesc->Flags, flags);
    return static_cast<D3D11_BIND_FLAG>(flags);
}

DXGI_FORMAT D3D11Resource::GetViewFormat(DXGI_FORMAT format) {
    TRACE("D3D11Resource::GetViewFormat called with %d", format);
    switch (format) {
        case DXGI_FORMAT_R32G32B32A32_TYPELESS:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case DXGI_FORMAT_R32G32B32_TYPELESS:
            return DXGI_FORMAT_R32G32B32_FLOAT;
        case DXGI_FORMAT_R16G16B16A16_TYPELESS:
            return DXGI_FORMAT_R16G16B16A16_UNORM;
        case DXGI_FORMAT_R32G32_TYPELESS:
            return DXGI_FORMAT_R32G32_FLOAT;
        case DXGI_FORMAT_R10G10B10A2_TYPELESS:
            return DXGI_FORMAT_R10G10B10A2_UNORM;
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case DXGI_FORMAT_R16G16_TYPELESS:
            return DXGI_FORMAT_R16G16_UNORM;
        case DXGI_FORMAT_R32_TYPELESS:
            return DXGI_FORMAT_R32_FLOAT;
        case DXGI_FORMAT_R8G8_TYPELESS:
            return DXGI_FORMAT_R8G8_UNORM;
        case DXGI_FORMAT_R16_TYPELESS:
            return DXGI_FORMAT_R16_UNORM;
        case DXGI_FORMAT_R8_TYPELESS:
            return DXGI_FORMAT_R8_UNORM;
        default:
            return format;
    }
}

D3D11_USAGE D3D11Resource::GetD3D11Usage(
    const D3D12_HEAP_PROPERTIES* pHeapProperties,
    const D3D12_RESOURCE_DESC* pDesc) {
    TRACE("D3D11Resource::GetD3D11Usage called");
    
    // For shared resources, we need to use DEFAULT
    if (pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS) {
        return D3D11_USAGE_DEFAULT;
    }

    switch (pHeapProperties->Type) {
        case D3D12_HEAP_TYPE_DEFAULT:
            // For textures that need CPU access, use DYNAMIC
            if (pDesc->Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D &&
                (pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)) {
                return D3D11_USAGE_DYNAMIC;
            }
            return D3D11_USAGE_DEFAULT;
            
        case D3D12_HEAP_TYPE_UPLOAD:
            return D3D11_USAGE_DYNAMIC;
            
        case D3D12_HEAP_TYPE_READBACK:
            return D3D11_USAGE_STAGING;
            
        case D3D12_HEAP_TYPE_CUSTOM:
            if (pHeapProperties->CPUPageProperty == D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE) {
                return D3D11_USAGE_DYNAMIC;
            }
            if (pHeapProperties->CPUPageProperty == D3D12_CPU_PAGE_PROPERTY_WRITE_BACK) {
                return D3D11_USAGE_STAGING;
            }
            return D3D11_USAGE_DEFAULT;
            
        default:
            return D3D11_USAGE_DEFAULT;
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
