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
    TRACE(
        "D3D11Resource::Create called with: %p, %p, %#x, %p, %#x, %p, %s, %p\n",
        device, pHeapProperties, HeapFlags, pDesc, InitialState,
        pOptimizedClearValue, debugstr_guid(&riid).c_str(), ppvResource);

    if (!device || !pHeapProperties || !pDesc || !ppvResource) {
        return E_INVALIDARG;
    }

    Microsoft::WRL::ComPtr<D3D11Resource> resource = new D3D11Resource(
        device, pHeapProperties, HeapFlags, pDesc, InitialState);

    if (!resource->GetD3D11Resource()) {
        ERR("Failed to create D3D11 resource.\n");
        return E_FAIL;
    }

    return resource.CopyTo(reinterpret_cast<ID3D12Resource**>(ppvResource));
}

D3D11Resource::D3D11Resource(D3D11Device* device,
                             const D3D12_HEAP_PROPERTIES* pHeapProperties,
                             D3D12_HEAP_FLAGS HeapFlags,
                             const D3D12_RESOURCE_DESC* pDesc,
                             D3D12_RESOURCE_STATES InitialState)
    : m_device(device),
      m_desc(*pDesc),
      m_heapProperties(*pHeapProperties),
      m_heapFlags(HeapFlags),
      m_currentState(InitialState) {
    TRACE("Creating resource type=%d, format=%d, width=%llu, height=%u\n",
          pDesc->Dimension, pDesc->Format, pDesc->Width, pDesc->Height);

    D3D11_BIND_FLAG bindFlags = GetD3D11BindFlags(pDesc);
    D3D11_USAGE usage = GetD3D11Usage(pHeapProperties);

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
            bufferDesc.MiscFlags = 0;
            bufferDesc.StructureByteStride = 0;

            Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
            HRESULT hr = m_device->GetD3D11Device()->CreateBuffer(
                &bufferDesc, nullptr, &buffer);
            if (FAILED(hr)) {
                ERR("Failed to create buffer, hr %#x.\n", hr);
                return;
            }
            m_resource = buffer;
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
                ERR("Failed to create texture 1D, hr %#x.\n", hr);
                return;
            }
            m_resource = texture;
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
                ERR("Failed to create texture 2D, hr %#x.\n", hr);
                return;
            }
            m_resource = texture;
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
                ERR("Failed to create texture 3D, hr %#x.\n", hr);
                return;
            }
            m_resource = texture;
            break;
        }
        default:
            ERR("Unsupported resource dimension %d.\n", pDesc->Dimension);
            break;
    }
}

UINT D3D11Resource::GetMiscFlags(const D3D12_RESOURCE_DESC* pDesc) {
    TRACE("GetMiscFlags called\n");
    TRACE("  Alignment: %llu\n", pDesc->Alignment);
    TRACE("  Dimension: %x\n", pDesc->Dimension);
    TRACE("  DepthOrArraySize: %x\n", pDesc->DepthOrArraySize);
    TRACE("  MipLevels: %x\n", pDesc->MipLevels);
    TRACE("  Format: %x\n", pDesc->Format);
    TRACE("  Flags: %x\n", pDesc->Flags);
    TRACE("  Height: %x\n", pDesc->Height);
    TRACE("  Width: %x\n", pDesc->Width);
    TRACE("  SampleDesc Count: %x\n", pDesc->SampleDesc.Count);
    TRACE("  SampleDesc Quality: %x\n", pDesc->SampleDesc.Quality);

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

    TRACE("GetMiscFlags returns %#x\n", flags);
    return flags;
}

void D3D11Resource::TransitionTo(ID3D11DeviceContext* context,
                                 D3D12_RESOURCE_STATES newState) {
    TRACE("D3D11Resource::TransitionTo %p, %#x -> %#x\n", context,
          m_currentState, newState);

    // No need to transition if states are the same
    if (m_currentState == newState) {
        return;
    }

    // Handle UAV barriers
    if (newState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS ||
        m_currentState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        context->CSSetUnorderedAccessViews(0, 1, nullptr, nullptr);
        m_isUAV = (newState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }

    // Handle transitions requiring synchronization
    if ((m_currentState == D3D12_RESOURCE_STATE_RENDER_TARGET &&
         newState != D3D12_RESOURCE_STATE_RENDER_TARGET) ||
        (m_currentState != D3D12_RESOURCE_STATE_RENDER_TARGET &&
         newState == D3D12_RESOURCE_STATE_RENDER_TARGET)) {
        context->Flush();
    }

    // Handle depth-stencil transitions
    if ((m_currentState == D3D12_RESOURCE_STATE_DEPTH_WRITE &&
         newState != D3D12_RESOURCE_STATE_DEPTH_WRITE) ||
        (m_currentState != D3D12_RESOURCE_STATE_DEPTH_WRITE &&
         newState == D3D12_RESOURCE_STATE_DEPTH_WRITE)) {
        context->Flush();
    }

    m_currentState = newState;
}

void D3D11Resource::UAVBarrier(ID3D11DeviceContext* context) {
    TRACE("D3D11Resource::UAVBarrier %p\n", context);

    if (m_isUAV) {
        // Ensure UAV writes are completed
        context->CSSetUnorderedAccessViews(0, 1, nullptr, nullptr);
        context->Flush();
    }
}

void D3D11Resource::AliasingBarrier(ID3D11DeviceContext* context,
                                    D3D11Resource* pResourceAfter) {
    TRACE("D3D11Resource::AliasingBarrier %p, %p\n", context, pResourceAfter);

    // Ensure all operations on both resources are completed
    context->Flush();
}

D3D11_BIND_FLAG D3D11Resource::GetD3D11BindFlags(
    const D3D12_RESOURCE_DESC* pDesc) {
    TRACE("D3D11Resource::GetD3D11BindFlags called");
    D3D11_BIND_FLAG flags = static_cast<D3D11_BIND_FLAG>(0);

    if (pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) {
        flags = static_cast<D3D11_BIND_FLAG>(flags | D3D11_BIND_RENDER_TARGET);
    }
    if (pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) {
        flags = static_cast<D3D11_BIND_FLAG>(flags | D3D11_BIND_DEPTH_STENCIL);
    }
    if (pDesc->Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) {
        flags =
            static_cast<D3D11_BIND_FLAG>(flags | D3D11_BIND_UNORDERED_ACCESS);
    }
    if (!(pDesc->Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)) {
        flags =
            static_cast<D3D11_BIND_FLAG>(flags | D3D11_BIND_SHADER_RESOURCE);
    }
    if (pDesc->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
        flags = static_cast<D3D11_BIND_FLAG>(flags | D3D11_BIND_VERTEX_BUFFER |
                                             D3D11_BIND_INDEX_BUFFER |
                                             D3D11_BIND_CONSTANT_BUFFER);
    }

    return flags;
}

DXGI_FORMAT D3D11Resource::GetViewFormat(DXGI_FORMAT format) {
    TRACE("D3D11Resource::GetViewFormat called with %d\n", format);
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
    const D3D12_HEAP_PROPERTIES* pHeapProperties) {
    TRACE("D3D11Resource::GetD3D11Usage called");
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

// IUnknown methods
HRESULT STDMETHODCALLTYPE D3D11Resource::QueryInterface(REFIID riid,
                                                        void** ppvObject) {
    TRACE("D3D11Resource::QueryInterface called: %s, %p\n",
          debugstr_guid(&riid).c_str(), ppvObject);

    if (!ppvObject) {
        return E_POINTER;
    }

    if (riid == __uuidof(ID3D12Resource) || riid == __uuidof(IUnknown)) {
        TRACE("D3D11Resource::QueryInterface returning ID3D12Resource\n");
        AddRef();
        *ppvObject = this;
        return S_OK;
    }

    WARN("Unknown interface %s.\n", debugstr_guid(&riid).c_str());
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE D3D11Resource::AddRef() {
    ULONG ref = InterlockedIncrement(&m_refCount);
    TRACE("%p increasing refcount to %u.\n", this, ref);
    return ref;
}

ULONG STDMETHODCALLTYPE D3D11Resource::Release() {
    ULONG ref = InterlockedDecrement(&m_refCount);
    TRACE("%p decreasing refcount to %u.\n", this, ref);
    if (ref == 0) {
        delete this;
    }
    return ref;
}

// ID3D12Object methods
HRESULT STDMETHODCALLTYPE D3D11Resource::GetPrivateData(REFGUID guid,
                                                        UINT* pDataSize,
                                                        void* pData) {
    TRACE("D3D11Resource::GetPrivateData called: %s, %p, %p\n",
          debugstr_guid(&guid).c_str(), pDataSize, pData);
    return m_resource->GetPrivateData(guid, pDataSize, pData);
}

HRESULT STDMETHODCALLTYPE D3D11Resource::SetPrivateData(REFGUID guid,
                                                        UINT DataSize,
                                                        const void* pData) {
    TRACE("D3D11Resource::SetPrivateData %s, %u, %p\n",
          debugstr_guid(&guid).c_str(), DataSize, pData);
    return m_resource->SetPrivateData(guid, DataSize, pData);
}

HRESULT STDMETHODCALLTYPE
D3D11Resource::SetPrivateDataInterface(REFGUID guid, const IUnknown* pData) {
    TRACE("D3D11Resource::SetPrivateDataInterface %s, %p\n",
          debugstr_guid(&guid).c_str(), pData);
    return m_resource->SetPrivateDataInterface(guid, pData);
}

HRESULT STDMETHODCALLTYPE D3D11Resource::SetName(LPCWSTR Name) {
    TRACE("D3D11Resource::SetName %s\n", debugstr_w(Name).c_str());
    return m_resource->SetPrivateData(
        WKPDID_D3DDebugObjectName,
        static_cast<UINT>((wcslen(Name) + 1) * sizeof(WCHAR)), Name);
}

// ID3D12DeviceChild methods
HRESULT STDMETHODCALLTYPE D3D11Resource::GetDevice(REFIID riid,
                                                   void** ppvDevice) {
    TRACE("D3D11Resource::GetDevice %s, %p\n", debugstr_guid(&riid).c_str(),
          ppvDevice);
    return m_device->QueryInterface(riid, ppvDevice);
}

// ID3D12Resource methods
HRESULT STDMETHODCALLTYPE D3D11Resource::Map(UINT Subresource,
                                             const D3D12_RANGE* pReadRange,
                                             void** ppData) {
    TRACE("D3D11Resource::Map %u, %p, %p\n", Subresource, pReadRange, ppData);

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = m_device->GetD3D11Context()->Map(m_resource.Get(), Subresource,
                                                  D3D11_MAP_WRITE_DISCARD, 0,
                                                  &mappedResource);

    if (SUCCEEDED(hr)) {
        *ppData = mappedResource.pData;
    }

    return hr;
}

void STDMETHODCALLTYPE D3D11Resource::Unmap(UINT Subresource,
                                            const D3D12_RANGE* pWrittenRange) {
    TRACE("D3D11Resource::Unmap %u, %p\n", Subresource, pWrittenRange);
    m_device->GetD3D11Context()->Unmap(m_resource.Get(), Subresource);
}

D3D12_RESOURCE_DESC* STDMETHODCALLTYPE
D3D12_RESOURCE_DESC* STDMETHODCALLTYPE D3D11Resource::GetDesc(D3D12_RESOURCE_DESC* desc) {
    TRACE("D3D11Resource::GetDesc(%p)\n", desc);
    TRACE("  Dimension: %d\n", desc->Dimension);
    TRACE("  Alignment: %llu\n", desc->Alignment);
    TRACE("  Width: %llu\n", desc->Width);
    TRACE("  Height: %u\n", desc->Height);
    TRACE("  DepthOrArraySize: %hu\n", desc->DepthOrArraySize);
    TRACE("  MipLevels: %hu\n", desc->MipLevels);
    TRACE("  Format: %d\n", desc->Format);
    TRACE("  SampleDesc.Count: %u\n", desc->SampleDesc.Count);
    TRACE("  SampleDesc.Quality: %u\n", desc->SampleDesc.Quality);
    TRACE("  Layout: %d\n", desc->Layout);
    *desc = m_desc;
    return desc;
}

D3D12_GPU_VIRTUAL_ADDRESS STDMETHODCALLTYPE
D3D11Resource::GetGPUVirtualAddress() {
    TRACE("D3D11Resource::GetGPUVirtualAddress called\n");
    return m_gpuAddress;
}

HRESULT STDMETHODCALLTYPE D3D11Resource::WriteToSubresource(
    UINT DstSubresource, const D3D12_BOX* pDstBox, const void* pSrcData,
    UINT SrcRowPitch, UINT SrcDepthPitch) {
    TRACE("D3D11Resource::WriteToSubresource called %u, %p, %p, %u, %u\n",
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
    TRACE("D3D11Resource::ReadFromSubresource %p, %u, %u, %u, %p\n", pDstData,
          DstRowPitch, DstDepthPitch, SrcSubresource, pSrcBox);

    // D3D11 doesn't have a direct equivalent for reading from a subresource
    // We need to create a staging resource and copy the data
    FIXME("ReadFromSubresource not implemented yet.\n");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Resource::GetHeapProperties(
    D3D12_HEAP_PROPERTIES* pHeapProperties, D3D12_HEAP_FLAGS* pHeapFlags) {
    TRACE("D3D11Resource::GetHeapProperties %p, %p\n", pHeapProperties,
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
