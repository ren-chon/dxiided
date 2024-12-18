#include "d3d11_impl/device.hpp"

#include <d3d11.h>
#include <d3d11_2.h>
#include <dxgi1_2.h>

#include "d3d11_impl/descriptor_heap.hpp"
#include "d3d11_impl/device_features.hpp"
#include "d3d11_impl/resource.hpp"
#include "d3d11_impl/fence.hpp"
#include "d3d11_impl/heap.hpp"

const GUID IID_IUnknown = {0x00000000,
                           0x0000,
                           0x0000,
                           {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};

namespace dxiided {

D3D11Device::D3D11Device(Microsoft::WRL::ComPtr<ID3D11Device> device,
                         Microsoft::WRL::ComPtr<ID3D11DeviceContext> context,
                         D3D_FEATURE_LEVEL feature_level)
    : m_d3d11Device(device),
      m_d3d11Context(context),
      m_featureLevel(feature_level) {}

HRESULT D3D11Device::Create(IUnknown* adapter,
                            D3D_FEATURE_LEVEL minimum_feature_level,
                            REFIID riid, void** device) {
    if (!device) {
        ERR("Invalid device pointer.\n");
        return E_INVALIDARG;
    }

    *device = nullptr;

    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
    if (adapter) {
        TRACE("  Attempting to get DXGI adapter from provided adapter %p\n",
              adapter);
        HRESULT hr = adapter->QueryInterface(IID_PPV_ARGS(&dxgi_adapter));
        if (FAILED(hr)) {
            ERR("Failed to get DXGI adapter (hr=%#x).\n", hr);
            // Try to get the adapter's IID for debugging
            IID iid;
            if (SUCCEEDED(
                    adapter->QueryInterface(IID_IUnknown, (void**)&iid))) {
                ERR("Adapter implements IID: %s\n",
                    debugstr_guid(&iid).c_str());
            }
            return E_INVALIDARG;
        }
        TRACE("  Successfully got DXGI adapter %p\n", dxgi_adapter.Get());
    } else {
        TRACE("  No adapter provided, using default\n");
    }

    // Create D3D11 device
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_context;
    D3D_FEATURE_LEVEL feature_level;

    // Only enable BGRA support, don't enable debug by default
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    TRACE("  Creating D3D11 device with flags 0x%x\n", flags);

    // Try feature levels in descending order
    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0};

    HRESULT hr = D3D11CreateDevice(
        dxgi_adapter.Get(),
        dxgi_adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
        nullptr, flags, feature_levels, ARRAYSIZE(feature_levels),
        D3D11_SDK_VERSION, &d3d11_device, &feature_level, &d3d11_context);

    if (FAILED(hr)) {
        ERR("D3D11CreateDevice failed with error %#x.\n", hr);
        return hr;
    }
    TRACE("  D3D11 device created successfully with feature level %#x\n",
          feature_level);

    // Create our device wrapper
    D3D11Device* d3d12_device =
        new D3D11Device(d3d11_device, d3d11_context, feature_level);
    if (!d3d12_device) {
        ERR("Failed to allocate device wrapper.\n");
        return E_OUTOFMEMORY;
    }
    TRACE("  Created D3D12 device wrapper %p\n", d3d12_device);

    // Query for the requested interface
    hr = d3d12_device->QueryInterface(riid, device);
    if (FAILED(hr)) {
        ERR("Failed to query for requested interface %s.\n",
            debugstr_guid(&riid).c_str());
        d3d12_device->Release();
        return hr;
    }
    TRACE("  Successfully queried for interface %s\n",
          debugstr_guid(&riid).c_str());
    return S_OK;
}

// IUnknown methods
HRESULT STDMETHODCALLTYPE D3D11Device::QueryInterface(REFIID riid,
                                                      void** ppvObject) {
    TRACE("D3D11Device::QueryInterface called for %s, %p\n",
          debugstr_guid(&riid).c_str(), ppvObject);

    if (ppvObject == nullptr) return E_POINTER;

    *ppvObject = nullptr;

    // D3D11 interfaces
    if (IsEqualGUID(riid, __uuidof(ID3D11Device)) ||
        IsEqualGUID(riid, __uuidof(ID3D11Device1)) ||
        IsEqualGUID(riid, __uuidof(ID3D11Device2))) {
        TRACE("Returning ID3D11Device interface\n");
        *ppvObject = reinterpret_cast<ID3D11Device*>(this);
        AddRef();
        return S_OK;
    }

    // D3D12 interfaces
    if (IsEqualGUID(riid, __uuidof(ID3D12Device)) ||
        IsEqualGUID(riid, __uuidof(ID3D12Device1)) ||
        IsEqualGUID(riid, __uuidof(ID3D12Device2))) {
        TRACE("Returning ID3D12Device interface\n");
        *ppvObject = static_cast<ID3D12Device2*>(this);
        AddRef();
        return S_OK;
    }

    // IUnknown - use ID3D12Device2 as primary interface
    if (IsEqualGUID(riid, __uuidof(IUnknown))) {
        TRACE("Returning IUnknown interface\n");
        *ppvObject = static_cast<ID3D12Device2*>(this);
        AddRef();
        return S_OK;
    }

    // Resource interfaces
    if (IsEqualGUID(riid, __uuidof(ID3D12Resource))) {
        TRACE("Handle ID3D12Resource interface request\n");
        if (m_d3d11Device) {
            return m_d3d11Device->QueryInterface(riid, ppvObject);
        }
        return E_NOINTERFACE;
    }

    WARN("Unknown interface query %s\n", debugstr_guid(&riid).c_str());
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE D3D11Device::AddRef() {
    TRACE("D3D11Device::AddRef called on object %p\n", this);
    ULONG ref = InterlockedIncrement(&m_refCount);
    TRACE("  New refcount: %lu\n", ref);
    return ref;
}

ULONG STDMETHODCALLTYPE D3D11Device::Release() {
    TRACE("D3D11Device::Release called on object %p\n", this);
    ULONG ref = InterlockedDecrement(&m_refCount);
    TRACE("  New refcount: %lu\n", ref);
    if (ref == 0) {
        TRACE("  Deleting object %p\n", this);
        delete this;
    }
    return ref;
}

// ID3D12Object methods
HRESULT STDMETHODCALLTYPE D3D11Device::GetPrivateData(REFGUID guid,
                                                      UINT* pDataSize,
                                                      void* pData) {
    return m_d3d11Device->GetPrivateData(guid, pDataSize, pData);
}

HRESULT STDMETHODCALLTYPE D3D11Device::SetPrivateData(REFGUID guid,
                                                      UINT DataSize,
                                                      const void* pData) {
    return m_d3d11Device->SetPrivateData(guid, DataSize, pData);
}

HRESULT STDMETHODCALLTYPE
D3D11Device::SetPrivateDataInterface(REFGUID guid, const IUnknown* pData) {
    return m_d3d11Device->SetPrivateDataInterface(guid, pData);
}

HRESULT STDMETHODCALLTYPE D3D11Device::SetName(LPCWSTR Name) {
    return m_d3d11Device->SetPrivateData(
        WKPDID_D3DDebugObjectName,
        static_cast<UINT>((wcslen(Name) + 1) * sizeof(WCHAR)), Name);
}

// ID3D12Device methods
UINT STDMETHODCALLTYPE D3D11Device::GetNodeCount() {
    TRACE("D3D11Device::GetNodeCount called on object %p\n", this);
    return 1;  // D3D11 doesn't support multiple nodes
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateCommandQueue(
    const D3D12_COMMAND_QUEUE_DESC* pDesc, REFIID riid, void** ppCommandQueue) {
    TRACE("D3D11Device::CreateCommandQueue called on object %p\n", this);
    TRACE("  Desc: %p, riid: %s, ppCommandQueue: %p\n", pDesc,
          debugstr_guid(&riid).c_str(), ppCommandQueue);

    return D3D11CommandQueue::Create(this, pDesc, riid, ppCommandQueue);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateCommandAllocator(
    D3D12_COMMAND_LIST_TYPE type, REFIID riid, void** ppCommandAllocator) {
    TRACE("D3D11Device::CreateCommandAllocator called on object %p\n", this);
    TRACE("  Type: %d, riid: %s, ppCommandAllocator: %p\n", type,
          debugstr_guid(&riid).c_str(), ppCommandAllocator);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateGraphicsPipelineState(
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc, REFIID riid,
    void** ppPipelineState) {
    // TODO: Implement graphics pipeline state creation
    FIXME("Graphics pipeline state creation not implemented yet.\n");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateComputePipelineState(
    const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc, REFIID riid,
    void** ppPipelineState) {
    // TODO: Implement compute pipeline state creation
    FIXME("Compute pipeline state creation not implemented yet.\n");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateCommandList(
    UINT nodeMask, D3D12_COMMAND_LIST_TYPE type,
    ID3D12CommandAllocator* pCommandAllocator,
    ID3D12PipelineState* pInitialState, REFIID riid, void** ppCommandList) {
    // TODO: Implement command list creation
    FIXME("Command list creation not implemented yet.\n");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateDescriptorHeap(
    const D3D12_DESCRIPTOR_HEAP_DESC* pDescriptorHeapDesc, REFIID riid,
    void** ppvHeap) {
    TRACE("D3D11Device::CreateDescriptorHeap(%p, %s, %p)\n",
          pDescriptorHeapDesc, debugstr_guid(&riid).c_str(), ppvHeap);
    return D3D11DescriptorHeap::Create(this, pDescriptorHeapDesc, riid,
                                       ppvHeap);
}

UINT STDMETHODCALLTYPE D3D11Device::GetDescriptorHandleIncrementSize(
    D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapType) {
    TRACE("D3D11Device::GetDescriptorHandleIncrementSize called");
    // TODO: Implement proper descriptor handle increment sizes
    switch (DescriptorHeapType) {
        case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
            return 64;
        case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
            return 16;
        case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:
            return 32;
        case D3D12_DESCRIPTOR_HEAP_TYPE_DSV:
            return 32;
        default:
            ERR("Unknown descriptor heap type %d.\n", DescriptorHeapType);
            return 0;
    }
}

HRESULT STDMETHODCALLTYPE D3D11Device::CheckFeatureSupport(
    D3D12_FEATURE Feature, void* pFeatureSupportData,
    UINT FeatureSupportDataSize) {
    TRACE("D3D11Device::CheckFeatureSupport BEGIN on object %p\n", this);
    TRACE("  Feature: 0x%x (%d)\n", Feature, Feature);
    TRACE("  pFeatureSupportData: %p\n", pFeatureSupportData);
    TRACE("  FeatureSupportDataSize: %u\n", FeatureSupportDataSize);

    if (!pFeatureSupportData) {
        ERR("Invalid feature support data pointer\n");
        return E_INVALIDARG;
    }
    // Zero out memory first
    memset(pFeatureSupportData, 0, FeatureSupportDataSize);
    switch (Feature) {
        case D3D12_FEATURE_SHADER_CACHE: {
            if (FeatureSupportDataSize >=
                sizeof(D3D12_FEATURE_DATA_SHADER_CACHE)) {
                auto* data =
                    (D3D12_FEATURE_DATA_SHADER_CACHE*)pFeatureSupportData;
                data->SupportFlags = D3D12_SHADER_CACHE_SUPPORT_NONE;
                TRACE("Reporting basic shader cache support");
                return S_OK;
            }
            break;
        }
        case D3D12_FEATURE_D3D12_OPTIONS1: {
            if (FeatureSupportDataSize >=
                sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS1)) {
                auto* data =
                    (D3D12_FEATURE_DATA_D3D12_OPTIONS1*)pFeatureSupportData;
                // Set conservative D3D11-compatible values
                data->WaveOps = FALSE;
                data->WaveLaneCountMin = 0;
                data->WaveLaneCountMax = 0;
                data->TotalLaneCount = 0;
                data->ExpandedComputeResourceStates = FALSE;
                data->Int64ShaderOps = FALSE;
                TRACE("Reporting D3D11-compatible D3D12 Options1 features");
                return S_OK;
            }
            break;
        }
        case D3D12_FEATURE_SHADER_MODEL: {
            if (FeatureSupportDataSize >=
                sizeof(D3D12_FEATURE_DATA_SHADER_MODEL)) {
                auto* data =
                    (D3D12_FEATURE_DATA_SHADER_MODEL*)pFeatureSupportData;

                // D3D11 supports up to Shader Model 5.0
                // We'll report 5.1 as it's the minimum supported by D3D12
                // and is backwards compatible with D3D11's 5.0
                data->HighestShaderModel = D3D_SHADER_MODEL_5_1;

                TRACE("Reporting Shader Model 5.1 (D3D11 compatible)");
                return S_OK;
            }
            break;
        }
        case D3D12_FEATURE_FORMAT_SUPPORT: {
            if (FeatureSupportDataSize >=
                sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)) {
                auto* data =
                    (D3D12_FEATURE_DATA_FORMAT_SUPPORT*)pFeatureSupportData;

                // Query D3D11 device for format support
                UINT d3d11Support = 0;
                if (m_d3d11Device) {
                    m_d3d11Device->CheckFormatSupport(data->Format,
                                                      &d3d11Support);
                }

                // Convert D3D11 format support to D3D12
                data->Support1 = D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE |
                                 D3D12_FORMAT_SUPPORT1_RENDER_TARGET |
                                 D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL |
                                 D3D12_FORMAT_SUPPORT1_TEXTURE2D;

                data->Support2 =
                    D3D12_FORMAT_SUPPORT2_NONE;  // Conservative default

                TRACE(
                    "Reporting D3D11-compatible format support for format: %f",
                    data->Format);
                return S_OK;
            }
            break;
        }
        case D3D12_FEATURE_D3D12_OPTIONS: {
            if (FeatureSupportDataSize >=
                sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS)) {
                auto* data =
                    (D3D12_FEATURE_DATA_D3D12_OPTIONS*)pFeatureSupportData;
                // Report conservative D3D11-compatible features
                data->DoublePrecisionFloatShaderOps = FALSE;
                data->OutputMergerLogicOp = FALSE;
                data->MinPrecisionSupport =
                    D3D12_SHADER_MIN_PRECISION_SUPPORT_NONE;
                data->TiledResourcesTier = D3D12_TILED_RESOURCES_TIER_1;
                data->ResourceBindingTier = D3D12_RESOURCE_BINDING_TIER_1;
                data->PSSpecifiedStencilRefSupported = FALSE;
                data->TypedUAVLoadAdditionalFormats = FALSE;
                data->ROVsSupported = FALSE;
                data->ConservativeRasterizationTier =
                    D3D12_CONSERVATIVE_RASTERIZATION_TIER_1;
                TRACE("Reporting basic D3D12 options compatible with D3D11");
                return S_OK;
            }
            break;
        }
        case D3D12_FEATURE_ROOT_SIGNATURE: {
            if (FeatureSupportDataSize >=
                sizeof(D3D12_FEATURE_DATA_ROOT_SIGNATURE)) {
                auto* data =
                    (D3D12_FEATURE_DATA_ROOT_SIGNATURE*)pFeatureSupportData;
                data->HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
                TRACE(
                    "Reporting Root Signature v1.0 support (D3D11 "
                    "compatible)");
                return S_OK;
            }
            break;
        }
        case D3D12_FEATURE_ARCHITECTURE: {
            if (FeatureSupportDataSize >=
                sizeof(D3D12_FEATURE_DATA_ARCHITECTURE)) {
                auto* data =
                    (D3D12_FEATURE_DATA_ARCHITECTURE*)pFeatureSupportData;
                data->NodeIndex = 0;
                data->TileBasedRenderer = FALSE;
                data->UMA = TRUE;  // Unified Memory Architecture
                data->CacheCoherentUMA = TRUE;
                TRACE("Reporting D3D11-compatible architecture features");
                return S_OK;
            }
            break;
        }
        case D3D12_FEATURE_FEATURE_LEVELS: {
            if (FeatureSupportDataSize >=
                sizeof(D3D12_FEATURE_DATA_FEATURE_LEVELS)) {
                auto* data =
                    (D3D12_FEATURE_DATA_FEATURE_LEVELS*)pFeatureSupportData;
                // Report only feature levels that D3D11 supports
                data->MaxSupportedFeatureLevel = D3D_FEATURE_LEVEL_11_0;
                TRACE("Reporting D3D11 feature level support");
                return S_OK;
            }
            break;
        }
        default:
            TRACE("  Unsupported feature requested: %f", Feature);
            return E_NOTIMPL;
    }

    TRACE("  Feature check failed - invalid size or unsupported feature");
    return E_INVALIDARG;
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateRootSignature(
    UINT nodeMask, const void* pBlobWithRootSignature, SIZE_T blobLengthInBytes,
    REFIID riid, void** ppvRootSignature) {
    TRACE("D3D11Device::CreateRootSignature(%u, %p, %zu, %s, %p)\n", nodeMask,
          pBlobWithRootSignature, blobLengthInBytes,
          debugstr_guid(&riid).c_str(), ppvRootSignature);
    return E_NOTIMPL;
}

void STDMETHODCALLTYPE D3D11Device::CreateConstantBufferView(
    const D3D12_CONSTANT_BUFFER_VIEW_DESC* pDesc,
    D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) {
    TRACE("D3D11Device::CreateConstantBufferView called\n");
    TRACE("  BufferLocation: %p\n", pDesc->BufferLocation);
    TRACE("  SizeInBytes: %u\n", pDesc->SizeInBytes);

    if (!pDesc) {
        ERR("No constant buffer view description provided.\n");
        return;
    }

    // Create D3D11 constant buffer view
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.ByteWidth = pDesc->SizeInBytes;
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.CPUAccessFlags = 0;
    bufferDesc.MiscFlags = 0;
    bufferDesc.StructureByteStride = 0;

    // Store view in descriptor heap
    auto* descriptor = reinterpret_cast<D3D11_BUFFER_DESC*>(DestDescriptor.ptr);
    *descriptor = bufferDesc;
}

void STDMETHODCALLTYPE D3D11Device::CreateShaderResourceView(
    ID3D12Resource* pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC* pDesc,
    D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) {
    TRACE("D3D11Device::CreateShaderResourceView called\n");
    TRACE("  Resource: %p\n", pResource);
    TRACE("  Format: %p\n", pDesc->Format);
    TRACE("  ViewDimension: %d\n", pDesc->ViewDimension);
    TRACE("  Shader4ComponentMapping: %d\n", pDesc->Shader4ComponentMapping);
    TRACE("  Buffer FirstElement: %d\n", pDesc->Buffer.FirstElement);
    TRACE("  Buffer NumElements: %d\n", pDesc->Buffer.NumElements);
    TRACE("  Texture2D MipLevels: %d\n", pDesc->Texture2D.MipLevels);
    TRACE("  DestDescriptor: %p\n", (void*)DestDescriptor.ptr);

    if (!pResource) {
        ERR("No resource provided for shader resource view.\n");
        return;
    }

    ID3D11Resource* d3d11Resource = GetD3D11Resource(pResource);
    if (!d3d11Resource) {
        ERR("D3D11 resource not found for D3D12 resource %p\n", pResource);
        return;
    }

    // Check resource bind flags
    D3D11_RESOURCE_DIMENSION dimension;
    d3d11Resource->GetType(&dimension);
    
    if (dimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        if (SUCCEEDED(d3d11Resource->QueryInterface(IID_PPV_ARGS(&texture)))) {
            D3D11_TEXTURE2D_DESC desc;
            texture->GetDesc(&desc);
            
            TRACE("D3D11 Resource properties:\n");
            TRACE("  Format: %d\n", desc.Format);
            TRACE("  BindFlags: %d\n", desc.BindFlags);
            TRACE("  MipLevels: %d\n", desc.MipLevels);
            
            if (!(desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)) {
                ERR("Resource was not created with D3D11_BIND_SHADER_RESOURCE flag (flags=%d)\n", desc.BindFlags);
                return;
            }
        }
    }

    TRACE("Creating SRV with format %d, dimension %d\n", pDesc->Format, pDesc->ViewDimension);

    // Create default SRV description if none provided
    D3D11_SHADER_RESOURCE_VIEW_DESC d3d11Desc = {};
    if (pDesc) {
        TRACE("pDesc provided\n");
        // Convert D3D12 description to D3D11
        d3d11Desc.Format = static_cast<DXGI_FORMAT>(pDesc->Format);
        d3d11Desc.ViewDimension =
            static_cast<D3D11_SRV_DIMENSION>(pDesc->ViewDimension);

        switch (pDesc->ViewDimension) {
            case D3D12_SRV_DIMENSION_TEXTURE2D:
                TRACE("D3D12_SRV_DIMENSION_TEXTURE2D matched\n");
                d3d11Desc.Texture2D.MostDetailedMip = pDesc->Texture2D.MostDetailedMip;
                d3d11Desc.Texture2D.MipLevels = pDesc->Texture2D.MipLevels;  // Use D3D12's requested mip levels
                break;
            default:
                ERR("Unsupported view dimension: %d\n", pDesc->ViewDimension);
                return;
        }
    } else {
        TRACE("No pDesc provided\n");
        // Get resource properties
        D3D12_RESOURCE_DESC resDesc = {};
        pResource->GetDesc(&resDesc);

        d3d11Desc.Format = static_cast<DXGI_FORMAT>(resDesc.Format);
        d3d11Desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        d3d11Desc.Texture2D.MostDetailedMip = 0;
        d3d11Desc.Texture2D.MipLevels = -1;  // Use all mips
    }

    TRACE("Store view in descriptor heap\n");
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    HRESULT hr = m_d3d11Device->CreateShaderResourceView(
        d3d11Resource,
        &d3d11Desc,  // Always use our translated description
        &srv);
    if (FAILED(hr)) {
        ERR("Failed to create D3D11 shader resource view, hr %#x\n", hr);
        return;
    }

    TRACE("Store view in descriptor heap");
    // Store view in descriptor heap
    auto* descriptor =
        reinterpret_cast<ID3D11ShaderResourceView**>(DestDescriptor.ptr);
    *descriptor = srv.Detach();
}

void STDMETHODCALLTYPE D3D11Device::CreateUnorderedAccessView(
    ID3D12Resource* pResource, ID3D12Resource* pCounterResource,
    const D3D12_UNORDERED_ACCESS_VIEW_DESC* pDesc,
    D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) {
    TRACE("D3D11Device::CreateUnorderedAccessView called\n");
    TRACE("  Resource: %p\n", pResource);
    TRACE("  CounterResource: %p\n", pCounterResource);
    TRACE("  Format: %p\n", pDesc->Format);
    TRACE("  ViewDimension: %d\n", pDesc->ViewDimension);
    TRACE("  DestDescriptor: %p\n", (void*)DestDescriptor.ptr);

    if (!pResource) {
        ERR("No resource provided for unordered access view.\n");
        return;
    }

    ID3D11Resource* d3d11Resource = GetD3D11Resource(pResource);
    if (!d3d11Resource) {
        ERR("D3D11 resource not found for D3D12 resource %p\n", pResource);
        return;
    }

    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
    HRESULT hr =
        m_d3d11Device->CreateUnorderedAccessView(d3d11Resource, nullptr, &uav);
    if (FAILED(hr)) {
        ERR("Failed to create D3D11 unordered access view, hr %#x\n", hr);
        return;
    }

    // Store view in descriptor heap
    auto* descriptor =
        reinterpret_cast<ID3D11UnorderedAccessView**>(DestDescriptor.ptr);
    *descriptor = uav.Detach();
}

void STDMETHODCALLTYPE D3D11Device::CreateRenderTargetView(
    ID3D12Resource* pResource, const D3D12_RENDER_TARGET_VIEW_DESC* pDesc,
    D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) {
    TRACE("D3D11Device::CreateRenderTargetView called\n");
    TRACE("  Resource: %p\n", pResource);
    TRACE("  Format: %p\n", pDesc->Format);
    TRACE("  ViewDimension: %d\n", pDesc->ViewDimension);
    TRACE("  DestDescriptor: %p\n", (void*)DestDescriptor.ptr);

    if (!pResource) {
        ERR("No resource provided for render target view.\n");
        return;
    }

    ID3D11Resource* d3d11Resource = GetD3D11Resource(pResource);
    if (!d3d11Resource) {
        ERR("D3D11 resource not found for D3D12 resource %p\n", pResource);
        return;
    }

    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
    HRESULT hr =
        m_d3d11Device->CreateRenderTargetView(d3d11Resource, nullptr, &rtv);
    if (FAILED(hr)) {
        ERR("Failed to create D3D11 render target view, hr %#x\n", hr);
        return;
    }

    // Store view in descriptor heap
    auto* descriptor =
        reinterpret_cast<ID3D11RenderTargetView**>(DestDescriptor.ptr);
    *descriptor = rtv.Detach();
}

void STDMETHODCALLTYPE D3D11Device::CreateDepthStencilView(
    ID3D12Resource* pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC* pDesc,
    D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) {
    TRACE("D3D11Device::CreateDepthStencilView called\n");
    TRACE("  Resource: %p\n", pResource);
    TRACE("  Format: %p\n", pDesc->Format);
    TRACE("  ViewDimension: %d\n", pDesc->ViewDimension);
    TRACE("  DestDescriptor: %p\n", (void*)DestDescriptor.ptr);

    if (!pResource) {
        ERR("No resource provided for depth stencil view.\n");
        return;
    }

    ID3D11Resource* d3d11Resource = GetD3D11Resource(pResource);
    if (!d3d11Resource) {
        ERR("D3D11 resource not found for D3D12 resource %p\n", pResource);
        return;
    }

    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv;
    HRESULT hr =
        m_d3d11Device->CreateDepthStencilView(d3d11Resource, nullptr, &dsv);
    if (FAILED(hr)) {
        ERR("Failed to create D3D11 depth stencil view, hr %#x\n", hr);
        return;
    }

    // Store view in descriptor heap
    auto* descriptor =
        reinterpret_cast<ID3D11DepthStencilView**>(DestDescriptor.ptr);
    *descriptor = dsv.Detach();
}

void STDMETHODCALLTYPE D3D11Device::CreateSampler(const D3D12_SAMPLER_DESC* pDesc,
                           D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) {
    TRACE("D3D11Device::CreateSampler called");
    TRACE("  Filter: %d\n", pDesc->Filter);
    TRACE("  AddressU: %d\n", pDesc->AddressU);
    TRACE("  AddressV: %d\n", pDesc->AddressV);
    TRACE("  AddressW: %d\n", pDesc->AddressW);
    TRACE("  MipLODBias: %f\n", pDesc->MipLODBias);
    TRACE("  MaxAnisotropy: %d\n", pDesc->MaxAnisotropy);
    TRACE("  ComparisonFunc: %d\n", pDesc->ComparisonFunc);
    TRACE("  BorderColor: %f %f %f %f\n", pDesc->BorderColor[0],
          pDesc->BorderColor[1], pDesc->BorderColor[2], pDesc->BorderColor[3]);
    TRACE("  MinLOD: %f\n", pDesc->MinLOD);
    TRACE("  MaxLOD: %f\n", pDesc->MaxLOD);

    if (!pDesc) {
        ERR("No sampler description provided.\n");
        return;
    }

    // Convert D3D12 sampler desc to D3D11
    D3D11_SAMPLER_DESC d3d11Desc = {};
    d3d11Desc.Filter =
        D3D11_FILTER_MIN_MAG_MIP_LINEAR;  // TODO: Convert from D3D12
    d3d11Desc.AddressU =
        static_cast<D3D11_TEXTURE_ADDRESS_MODE>(pDesc->AddressU);
    d3d11Desc.AddressV =
        static_cast<D3D11_TEXTURE_ADDRESS_MODE>(pDesc->AddressV);
    d3d11Desc.AddressW =
        static_cast<D3D11_TEXTURE_ADDRESS_MODE>(pDesc->AddressW);
    d3d11Desc.MipLODBias = pDesc->MipLODBias;
    d3d11Desc.MaxAnisotropy = pDesc->MaxAnisotropy;
    d3d11Desc.ComparisonFunc =
        static_cast<D3D11_COMPARISON_FUNC>(pDesc->ComparisonFunc);
    memcpy(d3d11Desc.BorderColor, pDesc->BorderColor, sizeof(float) * 4);
    d3d11Desc.MinLOD = pDesc->MinLOD;
    d3d11Desc.MaxLOD = pDesc->MaxLOD;

    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
    HRESULT hr = m_d3d11Device->CreateSamplerState(&d3d11Desc, &sampler);
    if (FAILED(hr)) {
        ERR("Failed to create D3D11 sampler state, hr %#x\n", hr);
        return;
    }

    // Store sampler in descriptor heap
    auto* descriptor =
        reinterpret_cast<ID3D11SamplerState**>(DestDescriptor.ptr);
    *descriptor = sampler.Detach();
}

void STDMETHODCALLTYPE D3D11Device::CopyDescriptors(
    UINT NumDestDescriptorRanges,
    const D3D12_CPU_DESCRIPTOR_HANDLE* pDestDescriptorRangeStarts,
    const UINT* pDestDescriptorRangeSizes, UINT NumSrcDescriptorRanges,
    const D3D12_CPU_DESCRIPTOR_HANDLE* pSrcDescriptorRangeStarts,
    const UINT* pSrcDescriptorRangeSizes,
    D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType) {
    TRACE("D3D11Device::CopyDescriptors(%u, %p, %p, %u, %p, %p, %d)\n",
          NumDestDescriptorRanges, pDestDescriptorRangeStarts,
          pDestDescriptorRangeSizes, NumSrcDescriptorRanges,
          pSrcDescriptorRangeStarts, pSrcDescriptorRangeSizes,
          DescriptorHeapsType);
}

void STDMETHODCALLTYPE D3D11Device::CopyDescriptorsSimple(
    UINT NumDescriptors, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptorRangeStart,
    D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptorRangeStart,
    D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType) {
    TRACE("D3D11Device::CopyDescriptorsSimple(%u, %p, %p, %d)\n",
          NumDescriptors, (void*)DestDescriptorRangeStart.ptr,
          (void*)SrcDescriptorRangeStart.ptr, DescriptorHeapsType);
}

D3D12_RESOURCE_ALLOCATION_INFO* STDMETHODCALLTYPE
D3D11Device::GetResourceAllocationInfo(
    D3D12_RESOURCE_ALLOCATION_INFO* info, UINT visibleMask,
    UINT numResourceDescs, const D3D12_RESOURCE_DESC* pResourceDescs) {
    TRACE("D3D11Device::GetResourceAllocationInfo(%p, %u, %u, %p)\n", info,
          visibleMask, numResourceDescs, pResourceDescs);
    return info;
}

D3D12_HEAP_PROPERTIES* STDMETHODCALLTYPE D3D11Device::GetCustomHeapProperties(
    D3D12_HEAP_PROPERTIES* props, UINT nodeMask, D3D12_HEAP_TYPE heapType) {
    TRACE("D3D11Device::GetCustomHeapProperties(%p, %u, %d)\n", props, nodeMask,
          heapType);
    return props;
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateCommittedResource(
    const D3D12_HEAP_PROPERTIES* pHeapProperties, D3D12_HEAP_FLAGS HeapFlags,
    const D3D12_RESOURCE_DESC* pDesc,
    D3D12_RESOURCE_STATES InitialResourceState,
    const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riidResource,
    void** ppvResource) {
    TRACE("D3D11Device::CreateCommittedResource called");
    TRACE("  pHeapProperties: %p\n", pHeapProperties);
    TRACE("  Height: %d\n", pDesc->Height);
    TRACE("  Width: %d\n", pDesc->Width);
    TRACE("  HeapFlags: %d\n", HeapFlags);
    TRACE("  Alignment: %p\n", pDesc->Alignment);
    TRACE("  DepthOrArraySize: %p\n", pDesc->DepthOrArraySize);
    TRACE("  Flags: %p\n", pDesc->Flags);
    TRACE("  Dimension: %p\n", pDesc->Dimension);
    TRACE("  Format: %p\n", pDesc->Format);
    TRACE("  SampleDesc.Count: %p\n", pDesc->SampleDesc.Count);
    TRACE("  SampleDesc.Quality: %p\n", pDesc->SampleDesc.Quality);
    TRACE("  Layout: %p\n", pDesc->Layout);
    TRACE("  InitialResourceState: %d\n", InitialResourceState);
    TRACE("  pOptimizedClearValue: %p\n", pOptimizedClearValue);
    TRACE("  riidResource: %s\n", debugstr_guid(&riidResource).c_str());
    TRACE("  ppvResource: %p\n", ppvResource);
    

    if (!pHeapProperties || !pDesc || !ppvResource) {
        ERR("Invalid parameters\n");
        return E_INVALIDARG;
    }

    return D3D11Resource::Create(
        this, pHeapProperties, HeapFlags, pDesc,
        InitialResourceState, pOptimizedClearValue,
        riidResource, ppvResource);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateHeap(const D3D12_HEAP_DESC* pDesc,
                                                  REFIID riid, void** ppvHeap) {
    TRACE("D3D11Device::CreateHeap called\n");
    TRACE("  Flags: %p\n", pDesc->Flags);
    TRACE("  Size: %llu\n", pDesc->SizeInBytes);
    TRACE("  Alignment: %llu\n", pDesc->Alignment);
    TRACE("  CPUPageProperty: %d\n", pDesc->Properties.CPUPageProperty);
    TRACE("  MemoryPoolPreference: %d\n",
          pDesc->Properties.MemoryPoolPreference);
    TRACE("  CreationNodeMask: %u\n", pDesc->Properties.CreationNodeMask);
    TRACE("  Type: %u\n", pDesc->Properties.Type);
    TRACE("  VisibleNodeMask: %u\n", pDesc->Properties.VisibleNodeMask);
    TRACE("  riid: %s\n", debugstr_guid(&riid).c_str());
    TRACE("  ppvHeap: %p\n", ppvHeap);

    if (!pDesc || !ppvHeap) {
        ERR("Invalid parameters: pDesc=%p, ppvHeap=%p\n", pDesc, ppvHeap);
        return E_INVALIDARG;
    }

    // In D3D11, heaps are managed implicitly
    // We'll create a simple heap object that tracks the properties
    return D3D11Heap::Create(this, *pDesc, riid, ppvHeap);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreatePlacedResource(
    ID3D12Heap* pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC* pDesc,
    D3D12_RESOURCE_STATES InitialState,
    const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riid,
    void** ppvResource) {
    TRACE("D3D11Device::CreatePlacedResource called for %s\n",
          debugstr_guid(&riid).c_str());
    TRACE("  Dimension: %d", pDesc->Dimension);
    TRACE("  Alignment: %llu", pDesc->Alignment);
    TRACE("  Width: %llu", pDesc->Width);
    TRACE("  Height: %u", pDesc->Height);
    TRACE("  DepthOrArraySize: %u", pDesc->DepthOrArraySize);
    TRACE("  MipLevels: %u", pDesc->MipLevels);
    TRACE("  Format: %d", pDesc->Format);
    TRACE("  Count: %u", pDesc->SampleDesc.Count);
    TRACE("  Quality: %u", pDesc->SampleDesc.Quality);
    TRACE("  Layout: %d", pDesc->Layout);
    TRACE("  Flags: %u", pDesc->Flags);
    
    if (!pHeap || !pDesc || !ppvResource) {
        ERR("Invalid parameters");
        return E_INVALIDARG;
    }

    // Get the D3D11Heap from the D3D12Heap
    D3D11Heap* heap = static_cast<D3D11Heap*>(pHeap);
    if (!heap) {
        ERR("Invalid heap\n");
        return E_INVALIDARG;
    }

    // Get heap properties from the D3D12 heap
    D3D12_HEAP_DESC heapDesc;
    pHeap->GetDesc(&heapDesc);

    // Create a D3D11 resource using the heap's buffer
    return D3D11Resource::Create(this, heap->GetD3D11Buffer(), pDesc,
                               InitialState, riid, ppvResource);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateReservedResource(
    const D3D12_RESOURCE_DESC* pDesc, D3D12_RESOURCE_STATES InitialState,
    const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riid,
    void** ppvResource) {
    TRACE("D3D11Device::CreateReservedResource(%p, %u, %p, %s, %p)\n", pDesc,
          InitialState, pOptimizedClearValue, debugstr_guid(&riid).c_str(),
          ppvResource);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateSharedHandle(
    ID3D12DeviceChild* pObject, const SECURITY_ATTRIBUTES* pAttributes,
    DWORD Access, LPCWSTR Name, HANDLE* pHandle) {
    TRACE("D3D11Device::CreateSharedHandle(%p, %p, %u, %s, %p)\n", pObject,
          pAttributes, Access, debugstr_w(Name).c_str(), pHandle);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::OpenSharedHandle(HANDLE NTHandle,
                                                        REFIID riid,
                                                        void** ppvObj) {
    TRACE("D3D11Device::OpenSharedHandle(%p, %s, %p)\n", NTHandle,
          debugstr_guid(&riid).c_str(), ppvObj);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
D3D11Device::OpenSharedHandleByName(LPCWSTR Name, DWORD Access, HANDLE* pNTHandle) {
    TRACE("D3D11Device::OpenSharedHandleByName(%s, %u, %p)\n",
          debugstr_w(Name).c_str(), Access, pNTHandle);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::MakeResident(UINT NumObjects,
                                             ID3D12Pageable* const* ppObjects) {
    TRACE("D3D11Device::MakeResident(%u, %p)\n", NumObjects, ppObjects);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::Evict(UINT NumObjects,
                                             ID3D12Pageable* const* ppObjects) {
    TRACE("D3D11Device::Evict(%u, %p)\n", NumObjects, ppObjects);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateFence(UINT64 InitialValue,
                                                   D3D12_FENCE_FLAGS Flags,
                                                   REFIID riid,
                                                   void** ppFence) {
    TRACE("D3D11Device::CreateFence(%llu, %u, %s, %p)\n", InitialValue, Flags,
          debugstr_guid(&riid).c_str(), ppFence);
    return D3D11Fence::Create(this, InitialValue, Flags, riid, ppFence);
}

HRESULT STDMETHODCALLTYPE D3D11Device::GetDeviceRemovedReason() {
    TRACE("D3D11Device::GetDeviceRemovedReason() called\n");

    if (!m_d3d11Device) {
        ERR("No D3D11 device\n");
        return DXGI_ERROR_DEVICE_REMOVED;
    }

    // Check the underlying D3D11 device state
    HRESULT hr = m_d3d11Device->GetDeviceRemovedReason();
    if (FAILED(hr)) {
        ERR("D3D11 device removed with reason: %#x\n", hr);
        return hr;
    }

    return S_OK;
}

void STDMETHODCALLTYPE D3D11Device::GetCopyableFootprints(
    const D3D12_RESOURCE_DESC* pResourceDesc, UINT FirstSubresource,
    UINT NumSubresources, UINT64 BaseOffset,
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pLayouts, UINT* pNumRows,
    UINT64* pRowSizeInBytes, UINT64* pTotalBytes) {
    TRACE(
        "D3D11Device::GetCopyableFootprints(%p, %u, %u, %llu, %p, %p, %p, "
        "%p)\n",
        pResourceDesc, FirstSubresource, NumSubresources, BaseOffset, pLayouts,
        pNumRows, pRowSizeInBytes, pTotalBytes);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateQueryHeap(
    const D3D12_QUERY_HEAP_DESC* pDesc, REFIID riid, void** ppvHeap) {
    TRACE("D3D11Device::CreateQueryHeap(%p, %s, %p)\n", pDesc,
          debugstr_guid(&riid).c_str(), ppvHeap);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::SetStablePowerState(BOOL Enable) {
    TRACE("D3D11Device::SetStablePowerState(%d)\n", Enable);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
D3D11Device::CreateCommandSignature(const D3D12_COMMAND_SIGNATURE_DESC* pDesc,
                                    ID3D12RootSignature* pRootSignature,
                                    REFIID riid, void** ppvCommandSignature) {
    TRACE("D3D11Device::CreateCommandSignature(%p, %p, %s, %p)\n", pDesc,
          pRootSignature, debugstr_guid(&riid).c_str(), ppvCommandSignature);
    return E_NOTIMPL;
}

void STDMETHODCALLTYPE D3D11Device::GetResourceTiling(
    ID3D12Resource* pTiledResource, UINT* pNumTilesForEntireResource,
    D3D12_PACKED_MIP_INFO* pPackedMipDesc,
    D3D12_TILE_SHAPE* pStandardTileShapeForNonPackedMips,
    UINT* pNumSubresourceTilings, UINT FirstSubresourceTilingToGet,
    D3D12_SUBRESOURCE_TILING* pSubresourceTilingsForNonPackedMips) {
    TRACE("D3D11Device::GetResourceTiling(%p, %p, %p, %p, %p, %u, %p)\n",
          pTiledResource, pNumTilesForEntireResource, pPackedMipDesc,
          pStandardTileShapeForNonPackedMips, pNumSubresourceTilings,
          FirstSubresourceTilingToGet, pSubresourceTilingsForNonPackedMips);
}

LUID* STDMETHODCALLTYPE D3D11Device::GetAdapterLuid(LUID* pLuid) {
    TRACE("D3D11Device::GetAdapterLuid(%p)\n", pLuid);
    return pLuid;
}

// ID3D12Device1 methods
HRESULT STDMETHODCALLTYPE D3D11Device::SetEventOnMultipleFenceCompletion(
    ID3D12Fence* const* ppFences, const UINT64* pFenceValues, UINT NumFences,
    D3D12_MULTIPLE_FENCE_WAIT_FLAGS Flags, HANDLE hEvent) {
    TRACE(
        "D3D11Device::SetEventOnMultipleFenceCompletion called on object %p\n",
        this);
    WARN("Multiple fence completion events are not supported.\n");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
D3D11Device::CreatePipelineState(const D3D12_PIPELINE_STATE_STREAM_DESC* pDesc,
                                 REFIID riid, void** ppPipelineState) {
    TRACE("D3D11Device::CreatePipelineState called on object %p\n", this);
    TRACE("  Desc: %p, riid: %s, ppPipelineState: %p\n", pDesc,
          debugstr_guid(&riid).c_str(), ppPipelineState);
    return E_NOTIMPL;
}

// ID3D12Device2 methods
HRESULT STDMETHODCALLTYPE D3D11Device::CreatePipelineLibrary(
    const void* pLibraryBlob, SIZE_T BlobLengthInBytes, REFIID riid,
    void** ppPipelineLibrary) {
    TRACE("D3D11Device::CreatePipelineLibrary called on object %p\n", this);
    TRACE("  Blob: %p, length: %zu, riid: %s, ppPipelineLibrary: %p\n",
          pLibraryBlob, BlobLengthInBytes, debugstr_guid(&riid).c_str(),
          ppPipelineLibrary);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::SetResidencyPriority(
    UINT NumObjects, ID3D12Pageable* const* ppObjects,
    const D3D12_RESIDENCY_PRIORITY* pPriorities) {
    TRACE("D3D11Device::SetResidencyPriority called on object %p\n", this);
    TRACE("  NumObjects: %u, ppObjects: %p, pPriorities: %p\n", NumObjects,
          ppObjects, pPriorities);
    return E_NOTIMPL;
}

// ID3D12DebugDevice methods
HRESULT STDMETHODCALLTYPE
D3D11Device::SetFeatureMask(D3D12_DEBUG_FEATURE Mask) {
    TRACE("D3D11Device::SetFeatureMask called on object %p\n", this);
    TRACE("  Mask: %d\n", Mask);
    return E_NOTIMPL;
}

D3D12_DEBUG_FEATURE STDMETHODCALLTYPE D3D11Device::GetFeatureMask() {
    TRACE("D3D11Device::GetFeatureMask called on object %p\n", this);
    return D3D12_DEBUG_FEATURE_NONE;
}

HRESULT STDMETHODCALLTYPE
D3D11Device::ReportLiveDeviceObjects(D3D12_RLDO_FLAGS Flags) {
    TRACE("D3D11Device::ReportLiveDeviceObjects called on object %p\n", this);
    TRACE("  Flags: %d\n", Flags);
    return S_OK;  // Pretend we reported
}

// ID3D11Device methods
HRESULT STDMETHODCALLTYPE D3D11Device::CreateBuffer(
    const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData,
    ID3D11Buffer** ppBuffer) {
        TRACE("D3D11Device::CreateBuffer called on object %p\n", this);
    return m_d3d11Device->CreateBuffer(pDesc, pInitialData, ppBuffer);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateTexture1D(
    const D3D11_TEXTURE1D_DESC* pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture1D** ppTexture1D) {
        TRACE("D3D11Device::CreateTexture1D called on object %p\n", this);
    return m_d3d11Device->CreateTexture1D(pDesc, pInitialData, ppTexture1D);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateTexture2D(
    const D3D11_TEXTURE2D_DESC* pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture2D** ppTexture2D) {
        TRACE("D3D11Device::CreateTexture2D called on object %p\n", this);
    return m_d3d11Device->CreateTexture2D(pDesc, pInitialData, ppTexture2D);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateTexture3D(
    const D3D11_TEXTURE3D_DESC* pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture3D** ppTexture3D) {
        TRACE("D3D11Device::CreateTexture3D called on object %p\n", this);
    return m_d3d11Device->CreateTexture3D(pDesc, pInitialData, ppTexture3D);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateShaderResourceView(
    ID3D11Resource* pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc,
    ID3D11ShaderResourceView** ppSRView) {
        TRACE("D3D11Device::CreateShaderResourceView called on object %p\n", this);
    return m_d3d11Device->CreateShaderResourceView(pResource, pDesc, ppSRView);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateUnorderedAccessView(
    ID3D11Resource* pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc,
    ID3D11UnorderedAccessView** ppUAView) {
        TRACE("D3D11Device::CreateUnorderedAccessView called on object %p\n", this);
    return m_d3d11Device->CreateUnorderedAccessView(pResource, pDesc, ppUAView);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateRenderTargetView(
    ID3D11Resource* pResource, const D3D11_RENDER_TARGET_VIEW_DESC* pDesc,
    ID3D11RenderTargetView** ppRTView) {
        TRACE("D3D11Device::CreateRenderTargetView called on object %p\n", this);
    return m_d3d11Device->CreateRenderTargetView(pResource, pDesc, ppRTView);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateDepthStencilView(
    ID3D11Resource* pResource, const D3D11_DEPTH_STENCIL_VIEW_DESC* pDesc,
    ID3D11DepthStencilView** ppDepthStencilView) {
        TRACE("D3D11Device::CreateDepthStencilView called on object %p\n", this);
    return m_d3d11Device->CreateDepthStencilView(pResource, pDesc,
                                                 ppDepthStencilView);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateInputLayout(
    const D3D11_INPUT_ELEMENT_DESC* pInputElementDescs, UINT NumElements,
    const void* pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength,
    ID3D11InputLayout** ppInputLayout) {
        TRACE("D3D11Device::CreateInputLayout called on object %p\n", this);
    return m_d3d11Device->CreateInputLayout(pInputElementDescs, NumElements,
                                            pShaderBytecodeWithInputSignature,
                                            BytecodeLength, ppInputLayout);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateVertexShader(
    const void* pShaderBytecode, SIZE_T BytecodeLength,
    ID3D11ClassLinkage* pClassLinkage, ID3D11VertexShader** ppVertexShader) {
        TRACE("D3D11Device::CreateVertexShader called on object %p\n", this);
    return m_d3d11Device->CreateVertexShader(pShaderBytecode, BytecodeLength,
                                             pClassLinkage, ppVertexShader);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateGeometryShader(
    const void* pShaderBytecode, SIZE_T BytecodeLength,
    ID3D11ClassLinkage* pClassLinkage,
    ID3D11GeometryShader** ppGeometryShader) {
        TRACE("D3D11Device::CreateGeometryShader called on object %p\n", this);
    return m_d3d11Device->CreateGeometryShader(pShaderBytecode, BytecodeLength,
                                               pClassLinkage, ppGeometryShader);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateGeometryShaderWithStreamOutput(
    const void* pShaderBytecode, SIZE_T BytecodeLength,
    const D3D11_SO_DECLARATION_ENTRY* pSODeclaration, UINT NumEntries,
    const UINT* pBufferStrides, UINT NumStrides, UINT RasterizedStream,
    ID3D11ClassLinkage* pClassLinkage,
    ID3D11GeometryShader** ppGeometryShader) {
        TRACE("D3D11Device::CreateGeometryShaderWithStreamOutput called on object %p\n", this);
    return m_d3d11Device->CreateGeometryShaderWithStreamOutput(
        pShaderBytecode, BytecodeLength, pSODeclaration, NumEntries,
        pBufferStrides, NumStrides, RasterizedStream, pClassLinkage,
        ppGeometryShader);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreatePixelShader(
    const void* pShaderBytecode, SIZE_T BytecodeLength,
    ID3D11ClassLinkage* pClassLinkage, ID3D11PixelShader** ppPixelShader) {
        TRACE("D3D11Device::CreatePixelShader called on object %p\n", this);
    return m_d3d11Device->CreatePixelShader(pShaderBytecode, BytecodeLength,
                                            pClassLinkage, ppPixelShader);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateHullShader(
    const void* pShaderBytecode, SIZE_T BytecodeLength,
    ID3D11ClassLinkage* pClassLinkage, ID3D11HullShader** ppHullShader) {
        TRACE("D3D11Device::CreateHullShader called on object %p\n", this);
    return m_d3d11Device->CreateHullShader(pShaderBytecode, BytecodeLength,
                                           pClassLinkage, ppHullShader);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateDomainShader(
    const void* pShaderBytecode, SIZE_T BytecodeLength,
    ID3D11ClassLinkage* pClassLinkage, ID3D11DomainShader** ppDomainShader) {
        TRACE("D3D11Device::CreateDomainShader called on object %p\n", this);
    return m_d3d11Device->CreateDomainShader(pShaderBytecode, BytecodeLength,
                                             pClassLinkage, ppDomainShader);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateComputeShader(
    const void* pShaderBytecode, SIZE_T BytecodeLength,
    ID3D11ClassLinkage* pClassLinkage, ID3D11ComputeShader** ppComputeShader) {
        TRACE("D3D11Device::CreateComputeShader called on object %p\n", this);
    return m_d3d11Device->CreateComputeShader(pShaderBytecode, BytecodeLength,
                                              pClassLinkage, ppComputeShader);
}

HRESULT STDMETHODCALLTYPE
D3D11Device::CreateClassLinkage(ID3D11ClassLinkage** ppLinkage) {
        TRACE("D3D11Device::CreateClassLinkage called on object %p\n", this);
    return m_d3d11Device->CreateClassLinkage(ppLinkage);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateBlendState(
    const D3D11_BLEND_DESC* pBlendStateDesc, ID3D11BlendState** ppBlendState) {
        TRACE("D3D11Device::CreateBlendState called on object %p\n", this);
    return m_d3d11Device->CreateBlendState(pBlendStateDesc, ppBlendState);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateDepthStencilState(
    const D3D11_DEPTH_STENCIL_DESC* pDepthStencilDesc,
    ID3D11DepthStencilState** ppDepthStencilState) {
        TRACE("D3D11Device::CreateDepthStencilState called on object %p\n", this);
    return m_d3d11Device->CreateDepthStencilState(pDepthStencilDesc,
                                                  ppDepthStencilState);
}

HRESULT STDMETHODCALLTYPE
D3D11Device::CreateRasterizerState(const D3D11_RASTERIZER_DESC* pRasterizerDesc,
                                   ID3D11RasterizerState** ppRasterizerState) {
    TRACE("D3D11Device::CreateRasterizerState called on object %p\n", this);
    return m_d3d11Device->CreateRasterizerState(pRasterizerDesc,
                                                ppRasterizerState);
}

HRESULT STDMETHODCALLTYPE
D3D11Device::CreateSamplerState(const D3D11_SAMPLER_DESC* pSamplerDesc,
                                ID3D11SamplerState** ppSamplerState) {
    TRACE("D3D11Device::CreateSamplerState called on object %p\n", this);
    return m_d3d11Device->CreateSamplerState(pSamplerDesc, ppSamplerState);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateQuery(
    const D3D11_QUERY_DESC* pQueryDesc, ID3D11Query** ppQuery) {
    TRACE("D3D11Device::CreateQuery called on object %p\n", this);
    return m_d3d11Device->CreateQuery(pQueryDesc, ppQuery);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreatePredicate(
    const D3D11_QUERY_DESC* pPredicateDesc, ID3D11Predicate** ppPredicate) {
    TRACE("D3D11Device::CreatePredicate called on object %p\n", this);
    return m_d3d11Device->CreatePredicate(pPredicateDesc, ppPredicate);
}
HRESULT STDMETHODCALLTYPE D3D11Device::CreateCounter(
    const D3D11_COUNTER_DESC* pCounterDesc, ID3D11Counter** ppCounter) {
    TRACE("D3D11Device::CreateCounter called on object %p\n", this);
    return m_d3d11Device->CreateCounter(pCounterDesc, ppCounter);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateDeferredContext(
    UINT ContextFlags, ID3D11DeviceContext** ppDeferredContext) {
    return m_d3d11Device->CreateDeferredContext(ContextFlags,
                                                ppDeferredContext);
}

HRESULT STDMETHODCALLTYPE D3D11Device::OpenSharedResource(
    HANDLE hResource, REFIID ReturnedInterface, void** ppResource) {
    TRACE("D3D11Device::OpenSharedResource called on object %p\n", this);
    return m_d3d11Device->OpenSharedResource(hResource, ReturnedInterface,
                                             ppResource);
}

HRESULT STDMETHODCALLTYPE
D3D11Device::CheckFormatSupport(DXGI_FORMAT Format, UINT* pFormatSupport) {
    TRACE("D3D11Device::CheckFormatSupport called on object %p\n", this);
    return m_d3d11Device->CheckFormatSupport(Format, pFormatSupport);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CheckMultisampleQualityLevels(
    DXGI_FORMAT Format, UINT SampleCount, UINT* pNumQualityLevels) {
    TRACE("D3D11Device::CheckMultisampleQualityLevels called on object %p\n", this);
    return m_d3d11Device->CheckMultisampleQualityLevels(Format, SampleCount,
                                                        pNumQualityLevels);
}
void STDMETHODCALLTYPE
D3D11Device::GetImmediateContext(ID3D11DeviceContext** ppImmediateContext) {
    TRACE("D3D11Device::GetImmediateContext called on object %p\n", this);
    m_d3d11Device->GetImmediateContext(ppImmediateContext);
}

HRESULT STDMETHODCALLTYPE D3D11Device::SetExceptionMode(UINT RaiseFlags) {
    TRACE("D3D11Device::SetExceptionMode called on object %p\n", this);
    return m_d3d11Device->SetExceptionMode(RaiseFlags);
}

UINT STDMETHODCALLTYPE D3D11Device::GetExceptionMode() {
    TRACE("D3D11Device::GetExceptionMode called on object %p\n", this);
    return m_d3d11Device->GetExceptionMode();
}

// ID3D11Device1 methods
HRESULT STDMETHODCALLTYPE D3D11Device::CreateDeferredContext1(
    UINT ContextFlags, ID3D11DeviceContext1** ppDeferredContext) {
    TRACE("D3D11Device::CreateDeferredContext1 called on object %p\n", this);
    return m_d3d11Device1 ? m_d3d11Device1->CreateDeferredContext1(
                                ContextFlags, ppDeferredContext)
                          : E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
D3D11Device::CreateBlendState1(const D3D11_BLEND_DESC1* pBlendStateDesc,
                               ID3D11BlendState1** ppBlendState) {
    TRACE("D3D11Device::CreateBlendState1 called on object %p\n", this);
    return m_d3d11Device1 ? m_d3d11Device1->CreateBlendState1(pBlendStateDesc,
                                                              ppBlendState)
                          : E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateRasterizerState1(
    const D3D11_RASTERIZER_DESC1* pRasterizerDesc,
    ID3D11RasterizerState1** ppRasterizerState) {
    TRACE("D3D11Device::CreateRasterizerState1 called on object %p\n", this);
    return m_d3d11Device1 ? m_d3d11Device1->CreateRasterizerState1(
                                pRasterizerDesc, ppRasterizerState)
                          : E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateDeviceContextState(
    UINT Flags, const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels,
    UINT SDKVersion, REFIID EmulatedInterface,
    D3D_FEATURE_LEVEL* pChosenFeatureLevel,
    ID3DDeviceContextState** ppContextState) {
    TRACE("D3D11Device::CreateDeviceContextState called on object %p\n", this);
    return m_d3d11Device1
               ? m_d3d11Device1->CreateDeviceContextState(
                     Flags, pFeatureLevels, FeatureLevels, SDKVersion,
                     EmulatedInterface, pChosenFeatureLevel, ppContextState)
               : E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::OpenSharedResource1(
    HANDLE hResource, REFIID returnedInterface, void** ppResource) {
    TRACE("D3D11Device::OpenSharedResource1 called on object %p\n", this);
    return m_d3d11Device1 ? m_d3d11Device1->OpenSharedResource1(
                                hResource, returnedInterface, ppResource)
                          : E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::OpenSharedResourceByName(
    LPCWSTR lpName, DWORD dwDesiredAccess, REFIID returnedInterface,
    void** ppResource) {
    TRACE("D3D11Device::OpenSharedResourceByName called on object %p\n", this);
    TRACE("  lpName: %ls\n", lpName);
    TRACE("  dwDesiredAccess: %lu\n", dwDesiredAccess);
    TRACE("  returnedInterface: %p\n", returnedInterface);
    TRACE("  ppResource: %p\n", ppResource);
    return m_d3d11Device1
               ? m_d3d11Device1->OpenSharedResourceByName(
                     lpName, dwDesiredAccess, returnedInterface, ppResource)
               : E_NOTIMPL;
}
// ID3D11Device1 methods
void STDMETHODCALLTYPE
D3D11Device::GetImmediateContext1(ID3D11DeviceContext1** ppImmediateContext) {
    TRACE("D3D11Device::GetImmediateContext1 called on object %p\n", this);
    if (m_d3d11Device1) {
        m_d3d11Device1->GetImmediateContext1(ppImmediateContext);
    } else {
        *ppImmediateContext = nullptr;
    }
}

// ID3D11Device2 methods
void STDMETHODCALLTYPE
D3D11Device::GetImmediateContext2(ID3D11DeviceContext2** ppImmediateContext) {
    TRACE("D3D11Device::GetImmediateContext2 called on object %p\n", this);
    if (m_d3d11Device2) {
        m_d3d11Device2->GetImmediateContext2(ppImmediateContext);
    } else {
        *ppImmediateContext = nullptr;
    }
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateDeferredContext2(
    UINT ContextFlags, ID3D11DeviceContext2** ppDeferredContext) {
    TRACE("D3D11Device::CreateDeferredContext2 called on object %p\n", this);
    return m_d3d11Device2 ? m_d3d11Device2->CreateDeferredContext2(
                                ContextFlags, ppDeferredContext)
                          : E_NOTIMPL;
}

void STDMETHODCALLTYPE D3D11Device::GetResourceTiling(
    ID3D11Resource* pTiledResource, UINT* pNumTilesForEntireResource,
    D3D11_PACKED_MIP_DESC* pPackedMipDesc,
    D3D11_TILE_SHAPE* pStandardTileShapeForNonPackedMips,
    UINT* pNumSubresourceTilings, UINT FirstSubresourceTilingToGet,
    D3D11_SUBRESOURCE_TILING* pSubresourceTilingsForNonPackedMips) {
    TRACE("D3D11Device::GetResourceTiling called on object %p\n", this);
    if (m_d3d11Device2) {
        m_d3d11Device2->GetResourceTiling(
            pTiledResource, pNumTilesForEntireResource, pPackedMipDesc,
            pStandardTileShapeForNonPackedMips, pNumSubresourceTilings,
            FirstSubresourceTilingToGet, pSubresourceTilingsForNonPackedMips);
    }
}

HRESULT STDMETHODCALLTYPE D3D11Device::CheckMultisampleQualityLevels1(
    DXGI_FORMAT Format, UINT SampleCount, UINT Flags, UINT* pNumQualityLevels) {
    TRACE("D3D11Device::CheckMultisampleQualityLevels1 called on object %p\n",
          this);
    return m_d3d11Device2 ? m_d3d11Device2->CheckMultisampleQualityLevels1(
                                Format, SampleCount, Flags, pNumQualityLevels)
                          : E_NOTIMPL;
}

void STDMETHODCALLTYPE
D3D11Device::CheckCounterInfo(D3D11_COUNTER_INFO* pCounterInfo) {
    TRACE("D3D11Device::CheckCounterInfo called on object %p\n", this);
    if (m_d3d11Device) m_d3d11Device->CheckCounterInfo(pCounterInfo);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CheckCounter(
    const D3D11_COUNTER_DESC* pDesc, D3D11_COUNTER_TYPE* pType,
    UINT* pActiveCounters, LPSTR szName, UINT* pNameLength, LPSTR szUnits,
    UINT* pUnitsLength, LPSTR szDescription, UINT* pDescriptionLength) {
    TRACE("D3D11Device::CheckCounter called on object %p\n", this);
    return m_d3d11Device
               ? m_d3d11Device->CheckCounter(
                     pDesc, pType, pActiveCounters, szName, pNameLength,
                     szUnits, pUnitsLength, szDescription, pDescriptionLength)
               : E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::CheckFeatureSupport(
    D3D11_FEATURE Feature, void* pFeatureSupportData,
    UINT FeatureSupportDataSize) {
    TRACE("D3D11Device::CheckFeatureSupport called on object %p\n", this);
    return m_d3d11Device
               ? m_d3d11Device->CheckFeatureSupport(
                     Feature, pFeatureSupportData, FeatureSupportDataSize)
               : E_NOTIMPL;
}

D3D_FEATURE_LEVEL STDMETHODCALLTYPE D3D11Device::GetFeatureLevel() {
    TRACE("D3D11Device::GetFeatureLevel called on object %p\n", this);
    return m_d3d11Device ? m_d3d11Device->GetFeatureLevel()
                         : D3D_FEATURE_LEVEL_11_0;
}

UINT STDMETHODCALLTYPE D3D11Device::GetCreationFlags() {
    TRACE("D3D11Device::GetCreationFlags called on object %p\n", this);
    return m_d3d11Device ? m_d3d11Device->GetCreationFlags() : 0;
}

ID3D11Resource* D3D11Device::GetD3D11Resource(ID3D12Resource* d3d12Resource) {
    TRACE("D3D11Device::GetD3D11Resource called on object %p\n", this);
    if (!d3d12Resource) {
        return nullptr;
    }

    // Try to get the D3D11 resource from the D3D12 resource's private data
    ID3D11Resource* d3d11Resource = nullptr;
    UINT dataSize = sizeof(ID3D11Resource*);

    if (SUCCEEDED(d3d12Resource->GetPrivateData(__uuidof(ID3D11Resource),
                                                &dataSize, &d3d11Resource))) {
        return d3d11Resource;
    }

    ERR("D3D11 resource not found for D3D12 resource %p\n", d3d12Resource);
    return nullptr;
}

}  // namespace dxiided
