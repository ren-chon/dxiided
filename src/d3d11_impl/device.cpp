#include "d3d11_impl/device.hpp"

#include <d3d11.h>
#include <d3d11_2.h>
#include <dxgi1_2.h>

#include "d3d11_impl/command_allocator.hpp"
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

WrappedD3D12ToD3D11Device::WrappedD3D12ToD3D11Device(Microsoft::WRL::ComPtr<ID3D11Device> device,
                         Microsoft::WRL::ComPtr<ID3D11DeviceContext> context,
                         D3D_FEATURE_LEVEL feature_level)
    : m_d3d11Device(device),
      m_d3d11Context(context),
      m_featureLevel(feature_level) {}

HRESULT WrappedD3D12ToD3D11Device::Create(IUnknown* adapter,
                            D3D_FEATURE_LEVEL minimum_feature_level,
                            REFIID riid, void** device) {
    if (!device) {
        ERR("Invalid device pointer.");
        return E_INVALIDARG;
    }

    *device = nullptr;

    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
    if (adapter) {
        TRACE("  Attempting to get DXGI adapter from provided adapter %p",
              adapter);
        HRESULT hr = adapter->QueryInterface(IID_PPV_ARGS(&dxgi_adapter));
        if (FAILED(hr)) {
            ERR("Failed to get DXGI adapter (hr=%#x).", hr);
            // Try to get the adapter's IID for debugging
            IID iid;
            if (SUCCEEDED(
                    adapter->QueryInterface(IID_IUnknown, (void**)&iid))) {
                ERR("Adapter implements IID: %s",
                    debugstr_guid(&iid).c_str());
            }
            return E_INVALIDARG;
        }
        TRACE("  Successfully got DXGI adapter %p", dxgi_adapter.Get());
    } else {
        TRACE("  No adapter provided, using default");
    }

    // Create D3D11 device
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_context;
    D3D_FEATURE_LEVEL feature_level;

    // Only enable BGRA support, don't enable debug by default
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    TRACE("  Creating D3D11 device with flags 0x%x", flags);

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
        ERR("D3D11CreateDevice failed with error %#x.", hr);
        return hr;
    }
    TRACE("  D3D11 device created successfully with feature level %#x",
          feature_level);

    // Create our device wrapper
    WrappedD3D12ToD3D11Device* d3d12_device =
        new WrappedD3D12ToD3D11Device(d3d11_device, d3d11_context, feature_level);
    if (!d3d12_device) {
        ERR("Failed to allocate device wrapper.");
        return E_OUTOFMEMORY;
    }
    TRACE("  Created D3D12 device wrapper %p", d3d12_device);

    // Query for the requested interface
    hr = d3d12_device->QueryInterface(riid, device);
    if (FAILED(hr)) {
        ERR("Failed to query for requested interface %s.",
            debugstr_guid(&riid).c_str());
        d3d12_device->Release();
        return hr;
    }
    TRACE("  Successfully queried for interface %s",
          debugstr_guid(&riid).c_str());
    return S_OK;
}

// IUnknown methods
HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::QueryInterface(REFIID riid,
                                                      void** ppvObject) {
    TRACE("WrappedD3D12ToD3D11Device::QueryInterface called for %s, %p",
          debugstr_guid(&riid).c_str(), ppvObject);

    if (ppvObject == nullptr) return E_POINTER;

    *ppvObject = nullptr;

    // D3D11 interfaces
    if (IsEqualGUID(riid, __uuidof(ID3D11Device)) ||
        IsEqualGUID(riid, __uuidof(ID3D11Device1)) ||
        IsEqualGUID(riid, __uuidof(ID3D11Device2))) {
        TRACE("Returning ID3D11Device interface");
        *ppvObject = reinterpret_cast<ID3D11Device*>(this);
        AddRef();
        return S_OK;
    }

    // D3D12 interfaces
    if (IsEqualGUID(riid, __uuidof(ID3D12Device)) ||
        IsEqualGUID(riid, __uuidof(ID3D12Device1)) ||
        IsEqualGUID(riid, __uuidof(ID3D12Device2))) {
        TRACE("Returning ID3D12Device interface");
        *ppvObject = static_cast<ID3D12Device2*>(this);
        AddRef();
        return S_OK;
    }

    // IUnknown - use ID3D12Device2 as primary interface
    if (IsEqualGUID(riid, __uuidof(IUnknown))) {
        TRACE("Returning IUnknown interface");
        *ppvObject = static_cast<ID3D12Device2*>(this);
        AddRef();
        return S_OK;
    }

    // Resource interfaces
    if (IsEqualGUID(riid, __uuidof(ID3D12Resource))) {
        TRACE("Handle ID3D12Resource interface request");
        if (m_d3d11Device) {
            return m_d3d11Device->QueryInterface(riid, ppvObject);
        }
        return E_NOINTERFACE;
    }

    WARN("Unknown interface query %s", debugstr_guid(&riid).c_str());
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::AddRef() {
    TRACE("WrappedD3D12ToD3D11Device::AddRef called on object %p", this);
    ULONG ref = InterlockedIncrement(&m_refCount);
    TRACE("  New refcount: %lu", ref);
    return ref;
}

ULONG STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::Release() {
    TRACE("WrappedD3D12ToD3D11Device::Release called on object %p", this);
    ULONG ref = InterlockedDecrement(&m_refCount);
    TRACE("  New refcount: %lu", ref);
    if (ref == 0) {
        TRACE("  Deleting object %p", this);
        delete this;
    }
    return ref;
}

// ID3D12Object methods
HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::GetPrivateData(REFGUID guid,
                                                      UINT* pDataSize,
                                                      void* pData) {
    return m_d3d11Device->GetPrivateData(guid, pDataSize, pData);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::SetPrivateData(REFGUID guid,
                                                      UINT DataSize,
                                                      const void* pData) {
    return m_d3d11Device->SetPrivateData(guid, DataSize, pData);
}

HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11Device::SetPrivateDataInterface(REFGUID guid, const IUnknown* pData) {
    return m_d3d11Device->SetPrivateDataInterface(guid, pData);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::SetName(LPCWSTR Name) {
    return m_d3d11Device->SetPrivateData(
        WKPDID_D3DDebugObjectName,
        static_cast<UINT>((wcslen(Name) + 1) * sizeof(WCHAR)), Name);
}

// ID3D12Device methods
UINT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::GetNodeCount() {
    TRACE("WrappedD3D12ToD3D11Device::GetNodeCount called on object %p", this);
    return 1;  // D3D11 doesn't support multiple nodes
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateCommandQueue(
    const D3D12_COMMAND_QUEUE_DESC* pDesc, REFIID riid, void** ppCommandQueue) {
    TRACE("WrappedD3D12ToD3D11Device::CreateCommandQueue called on object %p", this);
    TRACE("  Desc: %p, riid: %s, ppCommandQueue: %p", pDesc,
          debugstr_guid(&riid).c_str(), ppCommandQueue);

    return WrappedD3D12ToD3D11CommandQueue::Create(this, pDesc, riid, ppCommandQueue);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateCommandAllocator(
    D3D12_COMMAND_LIST_TYPE type, REFIID riid, void** ppCommandAllocator) {
    TRACE("WrappedD3D12ToD3D11Device::CreateCommandAllocator called on object %p", this);
    TRACE("  Type: %d, riid: %s, ppCommandAllocator: %p", type,
          debugstr_guid(&riid).c_str(), ppCommandAllocator);
          
    return WrappedD3D12ToD3D11CommandAllocator::Create(this, type, riid, ppCommandAllocator);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateGraphicsPipelineState(
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc, REFIID riid,
    void** ppPipelineState) {
    // TODO: Implement graphics pipeline state creation
    FIXME("Graphics pipeline state creation not implemented yet.");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateComputePipelineState(
    const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc, REFIID riid,
    void** ppPipelineState) {
    // TODO: Implement compute pipeline state creation
    FIXME("Compute pipeline state creation not implemented yet.");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateCommandList(
    UINT nodeMask, D3D12_COMMAND_LIST_TYPE type,
    ID3D12CommandAllocator* pCommandAllocator,
    ID3D12PipelineState* pInitialState, REFIID riid, void** ppCommandList) {
    // TODO: Implement command list creation
    FIXME("Command list creation not implemented yet.");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateDescriptorHeap(
    const D3D12_DESCRIPTOR_HEAP_DESC* pDescriptorHeapDesc, REFIID riid,
    void** ppvHeap) {
    TRACE("WrappedD3D12ToD3D11Device::CreateDescriptorHeap(%p, %s, %p)",
          pDescriptorHeapDesc, debugstr_guid(&riid).c_str(), ppvHeap);
    return WrappedD3D12ToD3D11DescriptorHeap::Create(this, pDescriptorHeapDesc, riid,
                                       ppvHeap);
}

UINT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::GetDescriptorHandleIncrementSize(
    D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapType) {
    TRACE("WrappedD3D12ToD3D11Device::GetDescriptorHandleIncrementSize called");
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
            ERR("Unknown descriptor heap type %d.", DescriptorHeapType);
            return 0;
    }
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CheckFeatureSupport(
    D3D12_FEATURE Feature, void* pFeatureSupportData,
    UINT FeatureSupportDataSize) {
    TRACE("WrappedD3D12ToD3D11Device::CheckFeatureSupport BEGIN on object %p", this);
    TRACE("  Feature: 0x%x (%d)", Feature, Feature);
    TRACE("  pFeatureSupportData: %p", pFeatureSupportData);
    TRACE("  FeatureSupportDataSize: %u", FeatureSupportDataSize);

    if (!pFeatureSupportData) {
        ERR("Invalid feature support data pointer");
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

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateRootSignature(
    UINT nodeMask, const void* pBlobWithRootSignature, SIZE_T blobLengthInBytes,
    REFIID riid, void** ppvRootSignature) {
    TRACE("WrappedD3D12ToD3D11Device::CreateRootSignature(%u, %p, %zu, %s, %p)", nodeMask,
          pBlobWithRootSignature, blobLengthInBytes,
          debugstr_guid(&riid).c_str(), ppvRootSignature);
    return E_NOTIMPL;
}

void STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateConstantBufferView(
    const D3D12_CONSTANT_BUFFER_VIEW_DESC* pDesc,
    D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) {
    TRACE("WrappedD3D12ToD3D11Device::CreateConstantBufferView called");
    TRACE("  BufferLocation: %p", pDesc->BufferLocation);
    TRACE("  SizeInBytes: %u", pDesc->SizeInBytes);

    if (!pDesc) {
        ERR("No constant buffer view description provided.");
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

void STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateShaderResourceView(
    ID3D12Resource* pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC* pDesc,
    D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) {
    TRACE("WrappedD3D12ToD3D11Device::CreateShaderResourceView called");
    TRACE("  Resource: %p", pResource);
    TRACE("  Format: %p", pDesc->Format);
    TRACE("  ViewDimension: %d", pDesc->ViewDimension);
    TRACE("  Shader4ComponentMapping: %d", pDesc->Shader4ComponentMapping);
    TRACE("  Buffer FirstElement: %d", pDesc->Buffer.FirstElement);
    TRACE("  Buffer NumElements: %d", pDesc->Buffer.NumElements);
    TRACE("  Texture2D MipLevels: %d", pDesc->Texture2D.MipLevels);
    TRACE("  DestDescriptor: %p", (void*)DestDescriptor.ptr);

    if (!pResource) {
        ERR("No resource provided for shader resource view.");
        return;
    }

    ID3D11Resource* d3d11Resource = GetD3D11Resource(pResource);
    if (!d3d11Resource) {
        ERR("D3D11 resource not found for D3D12 resource %p", pResource);
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
            
            TRACE("D3D11 Resource properties:");
            TRACE("  Format: %d", desc.Format);
            TRACE("  BindFlags: %d", desc.BindFlags);
            TRACE("  MipLevels: %d", desc.MipLevels);
            
            if (!(desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)) {
                ERR("Resource was not created with D3D11_BIND_SHADER_RESOURCE flag (flags=%d)", desc.BindFlags);
                return;
            }
        }
    }

    TRACE("Creating SRV with format %d, dimension %d", pDesc->Format, pDesc->ViewDimension);

    // Create default SRV description if none provided
    D3D11_SHADER_RESOURCE_VIEW_DESC d3d11Desc = {};
    if (pDesc) {
        TRACE("pDesc provided");
        // Convert D3D12 description to D3D11
        d3d11Desc.Format = static_cast<DXGI_FORMAT>(pDesc->Format);
        d3d11Desc.ViewDimension =
            static_cast<D3D11_SRV_DIMENSION>(pDesc->ViewDimension);

        switch (pDesc->ViewDimension) {
            case D3D12_SRV_DIMENSION_TEXTURE2D:
                TRACE("D3D12_SRV_DIMENSION_TEXTURE2D matched");
                d3d11Desc.Texture2D.MostDetailedMip = pDesc->Texture2D.MostDetailedMip;
                d3d11Desc.Texture2D.MipLevels = pDesc->Texture2D.MipLevels;  // Use D3D12's requested mip levels
                break;
            default:
                ERR("Unsupported view dimension: %d", pDesc->ViewDimension);
                return;
        }
    } else {
        TRACE("No pDesc provided");
        // Get resource properties
        D3D12_RESOURCE_DESC resDesc = {};
        pResource->GetDesc(&resDesc);

        d3d11Desc.Format = static_cast<DXGI_FORMAT>(resDesc.Format);
        d3d11Desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        d3d11Desc.Texture2D.MostDetailedMip = 0;
        d3d11Desc.Texture2D.MipLevels = -1;  // Use all mips
    }

    TRACE("Store view in descriptor heap");
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    HRESULT hr = m_d3d11Device->CreateShaderResourceView(
        d3d11Resource,
        &d3d11Desc,  // Always use our translated description
        &srv);
    if (FAILED(hr)) {
        ERR("Failed to create D3D11 shader resource view, hr %#x", hr);
        return;
    }

    TRACE("Store view in descriptor heap");
    // Store view in descriptor heap
    auto* descriptor =
        reinterpret_cast<ID3D11ShaderResourceView**>(DestDescriptor.ptr);
    *descriptor = srv.Detach();
}

void STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateUnorderedAccessView(
    ID3D12Resource* pResource, ID3D12Resource* pCounterResource,
    const D3D12_UNORDERED_ACCESS_VIEW_DESC* pDesc,
    D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) {
    TRACE("WrappedD3D12ToD3D11Device::CreateUnorderedAccessView called");
    TRACE("  Resource: %p", pResource);
    TRACE("  CounterResource: %p", pCounterResource);
    TRACE("  Format: %p", pDesc->Format);
    TRACE("  ViewDimension: %d", pDesc->ViewDimension);
    TRACE("  DestDescriptor: %p", (void*)DestDescriptor.ptr);

    if (!pResource) {
        ERR("No resource provided for unordered access view.");
        return;
    }

    ID3D11Resource* d3d11Resource = GetD3D11Resource(pResource);
    if (!d3d11Resource) {
        ERR("D3D11 resource not found for D3D12 resource %p", pResource);
        return;
    }

    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
    HRESULT hr =
        m_d3d11Device->CreateUnorderedAccessView(d3d11Resource, nullptr, &uav);
    if (FAILED(hr)) {
        ERR("Failed to create D3D11 unordered access view, hr %#x", hr);
        return;
    }

    // Store view in descriptor heap
    auto* descriptor =
        reinterpret_cast<ID3D11UnorderedAccessView**>(DestDescriptor.ptr);
    *descriptor = uav.Detach();
}

void STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateRenderTargetView(
    ID3D12Resource* pResource, const D3D12_RENDER_TARGET_VIEW_DESC* pDesc,
    D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) {
    TRACE("WrappedD3D12ToD3D11Device::CreateRenderTargetView called");
    TRACE("  Resource: %p", pResource);
    TRACE("  Format: %p", pDesc->Format);
    TRACE("  ViewDimension: %d", pDesc->ViewDimension);
    TRACE("  DestDescriptor: %p", (void*)DestDescriptor.ptr);

    if (!pResource) {
        ERR("No resource provided for render target view.");
        return;
    }

    ID3D11Resource* d3d11Resource = GetD3D11Resource(pResource);
    if (!d3d11Resource) {
        ERR("D3D11 resource not found for D3D12 resource %p", pResource);
        return;
    }

    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
    HRESULT hr =
        m_d3d11Device->CreateRenderTargetView(d3d11Resource, nullptr, &rtv);
    if (FAILED(hr)) {
        ERR("Failed to create D3D11 render target view, hr %#x", hr);
        return;
    }

    // Store view in descriptor heap
    auto* descriptor =
        reinterpret_cast<ID3D11RenderTargetView**>(DestDescriptor.ptr);
    *descriptor = rtv.Detach();
}

void STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateDepthStencilView(
    ID3D12Resource* pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC* pDesc,
    D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) {
    TRACE("WrappedD3D12ToD3D11Device::CreateDepthStencilView called");
    TRACE("  Resource: %p", pResource);
    TRACE("  Format: %p", pDesc->Format);
    TRACE("  ViewDimension: %d", pDesc->ViewDimension);
    TRACE("  DestDescriptor: %p", (void*)DestDescriptor.ptr);

    if (!pResource) {
        ERR("No resource provided for depth stencil view.");
        return;
    }

    ID3D11Resource* d3d11Resource = GetD3D11Resource(pResource);
    if (!d3d11Resource) {
        ERR("D3D11 resource not found for D3D12 resource %p", pResource);
        return;
    }

    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv;
    HRESULT hr =
        m_d3d11Device->CreateDepthStencilView(d3d11Resource, nullptr, &dsv);
    if (FAILED(hr)) {
        ERR("Failed to create D3D11 depth stencil view, hr %#x", hr);
        return;
    }

    // Store view in descriptor heap
    auto* descriptor =
        reinterpret_cast<ID3D11DepthStencilView**>(DestDescriptor.ptr);
    *descriptor = dsv.Detach();
}

void STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateSampler(const D3D12_SAMPLER_DESC* pDesc,
                           D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) {
    TRACE("WrappedD3D12ToD3D11Device::CreateSampler called");
    TRACE("  Filter: %d", pDesc->Filter);
    TRACE("  AddressU: %d", pDesc->AddressU);
    TRACE("  AddressV: %d", pDesc->AddressV);
    TRACE("  AddressW: %d", pDesc->AddressW);
    TRACE("  MipLODBias: %f", pDesc->MipLODBias);
    TRACE("  MaxAnisotropy: %d", pDesc->MaxAnisotropy);
    TRACE("  ComparisonFunc: %d", pDesc->ComparisonFunc);
    TRACE("  BorderColor: %f %f %f %f", pDesc->BorderColor[0],
          pDesc->BorderColor[1], pDesc->BorderColor[2], pDesc->BorderColor[3]);
    TRACE("  MinLOD: %f", pDesc->MinLOD);
    TRACE("  MaxLOD: %f", pDesc->MaxLOD);

    if (!pDesc) {
        ERR("No sampler description provided.");
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
        ERR("Failed to create D3D11 sampler state, hr %#x", hr);
        return;
    }

    // Store sampler in descriptor heap
    auto* descriptor =
        reinterpret_cast<ID3D11SamplerState**>(DestDescriptor.ptr);
    *descriptor = sampler.Detach();
}

void STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CopyDescriptors(
    UINT NumDestDescriptorRanges,
    const D3D12_CPU_DESCRIPTOR_HANDLE* pDestDescriptorRangeStarts,
    const UINT* pDestDescriptorRangeSizes, UINT NumSrcDescriptorRanges,
    const D3D12_CPU_DESCRIPTOR_HANDLE* pSrcDescriptorRangeStarts,
    const UINT* pSrcDescriptorRangeSizes,
    D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType) {
    TRACE("WrappedD3D12ToD3D11Device::CopyDescriptors(%u, %p, %p, %u, %p, %p, %d)",
          NumDestDescriptorRanges, pDestDescriptorRangeStarts,
          pDestDescriptorRangeSizes, NumSrcDescriptorRanges,
          pSrcDescriptorRangeStarts, pSrcDescriptorRangeSizes,
          DescriptorHeapsType);
}

void STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CopyDescriptorsSimple(
    UINT NumDescriptors, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptorRangeStart,
    D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptorRangeStart,
    D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType) {
    TRACE("WrappedD3D12ToD3D11Device::CopyDescriptorsSimple(%u, %p, %p, %d)",
          NumDescriptors, (void*)DestDescriptorRangeStart.ptr,
          (void*)SrcDescriptorRangeStart.ptr, DescriptorHeapsType);
}

D3D12_RESOURCE_ALLOCATION_INFO* STDMETHODCALLTYPE
WrappedD3D12ToD3D11Device::GetResourceAllocationInfo(
    D3D12_RESOURCE_ALLOCATION_INFO* info, UINT visibleMask,
    UINT numResourceDescs, const D3D12_RESOURCE_DESC* pResourceDescs) {
    TRACE("WrappedD3D12ToD3D11Device::GetResourceAllocationInfo(%p, %u, %u, %p)", info,
          visibleMask, numResourceDescs, pResourceDescs);
    return info;
}

D3D12_HEAP_PROPERTIES* STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::GetCustomHeapProperties(
    D3D12_HEAP_PROPERTIES* props, UINT nodeMask, D3D12_HEAP_TYPE heapType) {
    TRACE("WrappedD3D12ToD3D11Device::GetCustomHeapProperties(%p, %u, %d)", props, nodeMask,
          heapType);
    return props;
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateCommittedResource(
    const D3D12_HEAP_PROPERTIES* pHeapProperties, D3D12_HEAP_FLAGS HeapFlags,
    const D3D12_RESOURCE_DESC* pDesc,
    D3D12_RESOURCE_STATES InitialResourceState,
    const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riidResource,
    void** ppvResource) {
    TRACE("WrappedD3D12ToD3D11Device::CreateCommittedResource called");
    TRACE("  pHeapProperties: %p", pHeapProperties);
    TRACE("  Height: %d", pDesc->Height);
    TRACE("  Width: %d", pDesc->Width);
    TRACE("  HeapFlags: %d", HeapFlags);
    TRACE("  Alignment: %p", pDesc->Alignment);
    TRACE("  DepthOrArraySize: %p", pDesc->DepthOrArraySize);
    TRACE("  Flags: %p", pDesc->Flags);
    TRACE("  Dimension: %p", pDesc->Dimension);
    TRACE("  Format: %p", pDesc->Format);
    TRACE("  SampleDesc.Count: %p", pDesc->SampleDesc.Count);
    TRACE("  SampleDesc.Quality: %p", pDesc->SampleDesc.Quality);
    TRACE("  Layout: %p", pDesc->Layout);
    TRACE("  InitialResourceState: %d", InitialResourceState);
    TRACE("  pOptimizedClearValue: %p", pOptimizedClearValue);
    TRACE("  riidResource: %s", debugstr_guid(&riidResource).c_str());
    TRACE("  ppvResource: %p", ppvResource);
    

    if (!pHeapProperties || !pDesc || !ppvResource) {
        ERR("Invalid parameters");
        return E_INVALIDARG;
    }

    return WrappedD3D12ToD3D11Resource::Create(
        this, pHeapProperties, HeapFlags, pDesc,
        InitialResourceState, pOptimizedClearValue,
        riidResource, ppvResource);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateHeap(const D3D12_HEAP_DESC* pDesc,
                                                  REFIID riid, void** ppvHeap) {
    TRACE("WrappedD3D12ToD3D11Device::CreateHeap called");
    TRACE("  Flags: %p", pDesc->Flags);
    TRACE("  Size: %llu", pDesc->SizeInBytes);
    TRACE("  Alignment: %llu", pDesc->Alignment);
    TRACE("  CPUPageProperty: %d", pDesc->Properties.CPUPageProperty);
    TRACE("  MemoryPoolPreference: %d",
          pDesc->Properties.MemoryPoolPreference);
    TRACE("  CreationNodeMask: %u", pDesc->Properties.CreationNodeMask);
    TRACE("  Type: %u", pDesc->Properties.Type);
    TRACE("  VisibleNodeMask: %u", pDesc->Properties.VisibleNodeMask);
    TRACE("  riid: %s", debugstr_guid(&riid).c_str());
    TRACE("  ppvHeap: %p", ppvHeap);

    if (!pDesc || !ppvHeap) {
        ERR("Invalid parameters: pDesc=%p, ppvHeap=%p", pDesc, ppvHeap);
        return E_INVALIDARG;
    }

    // In D3D11, heaps are managed implicitly
    // We'll create a simple heap object that tracks the properties
    return WrappedD3D12ToD3D11Heap::Create(this, *pDesc, riid, ppvHeap);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreatePlacedResource(
    ID3D12Heap* pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC* pDesc,
    D3D12_RESOURCE_STATES InitialState,
    const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riid,
    void** ppvResource) {
    TRACE("WrappedD3D12ToD3D11Device::CreatePlacedResource called for %s",
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

    WrappedD3D12ToD3D11Heap* heap = static_cast<WrappedD3D12ToD3D11Heap*>(pHeap);
    if (!heap) {
        ERR("Invalid heap");
        return E_INVALIDARG;
    }

    D3D12_HEAP_DESC heapDesc;
    pHeap->GetDesc(&heapDesc);

    // For upload heaps, we need to ensure the resource is created with the right CPU access flags
    if (heapDesc.Properties.Type == D3D12_HEAP_TYPE_UPLOAD) {
        D3D11_BUFFER_DESC bufferDesc = {};
        bufferDesc.ByteWidth = static_cast<UINT>(pDesc->Width);
        bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_INDEX_BUFFER | D3D11_BIND_CONSTANT_BUFFER;
        bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        bufferDesc.MiscFlags = 0;
        bufferDesc.StructureByteStride = 0;

        Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
        HRESULT hr = m_d3d11Device->CreateBuffer(&bufferDesc, nullptr, &buffer);
        if (FAILED(hr)) {
            ERR("Failed to create D3D11 buffer for placed resource, hr %#x", hr);
            return hr;
        }

        return WrappedD3D12ToD3D11Resource::Create(this, buffer.Get(), pDesc,
                                   InitialState, riid, ppvResource);
    }

    return WrappedD3D12ToD3D11Resource::Create(this, heap->GetD3D11Buffer(), pDesc,
                               InitialState, riid, ppvResource);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateReservedResource(
    const D3D12_RESOURCE_DESC* pDesc, D3D12_RESOURCE_STATES InitialState,
    const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riid,
    void** ppvResource) {
    TRACE("WrappedD3D12ToD3D11Device::CreateReservedResource(%p, %u, %p, %s, %p)", pDesc,
          InitialState, pOptimizedClearValue, debugstr_guid(&riid).c_str(),
          ppvResource);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateSharedHandle(
    ID3D12DeviceChild* pObject, const SECURITY_ATTRIBUTES* pAttributes,
    DWORD Access, LPCWSTR Name, HANDLE* pHandle) {
    TRACE("WrappedD3D12ToD3D11Device::CreateSharedHandle(%p, %p, %u, %s, %p)", pObject,
          pAttributes, Access, debugstr_w(Name).c_str(), pHandle);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::OpenSharedHandle(HANDLE NTHandle,
                                                        REFIID riid,
                                                        void** ppvObj) {
    TRACE("WrappedD3D12ToD3D11Device::OpenSharedHandle(%p, %s, %p)", NTHandle,
          debugstr_guid(&riid).c_str(), ppvObj);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11Device::OpenSharedHandleByName(LPCWSTR Name, DWORD Access, HANDLE* pNTHandle) {
    TRACE("WrappedD3D12ToD3D11Device::OpenSharedHandleByName(%s, %u, %p)",
          debugstr_w(Name).c_str(), Access, pNTHandle);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::MakeResident(UINT NumObjects,
                                             ID3D12Pageable* const* ppObjects) {
    TRACE("WrappedD3D12ToD3D11Device::MakeResident(%u, %p)", NumObjects, ppObjects);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::Evict(UINT NumObjects,
                                             ID3D12Pageable* const* ppObjects) {
    TRACE("WrappedD3D12ToD3D11Device::Evict(%u, %p)", NumObjects, ppObjects);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateFence(UINT64 InitialValue,
                                                   D3D12_FENCE_FLAGS Flags,
                                                   REFIID riid,
                                                   void** ppFence) {
    TRACE("WrappedD3D12ToD3D11Device::CreateFence(%llu, %u, %s, %p)", InitialValue, Flags,
          debugstr_guid(&riid).c_str(), ppFence);
    return WrappedD3D12ToD3D11Fence::Create(this, InitialValue, Flags, riid, ppFence);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::GetDeviceRemovedReason() {
    TRACE("WrappedD3D12ToD3D11Device::GetDeviceRemovedReason() called");

    if (!m_d3d11Device) {
        ERR("No D3D11 device");
        return DXGI_ERROR_DEVICE_REMOVED;
    }

    // Check the underlying D3D11 device state
    HRESULT hr = m_d3d11Device->GetDeviceRemovedReason();
    if (FAILED(hr)) {
        ERR("D3D11 device removed with reason: %#x", hr);
        return hr;
    }

    return S_OK;
}

void STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::GetCopyableFootprints(
    const D3D12_RESOURCE_DESC* pResourceDesc, UINT FirstSubresource,
    UINT NumSubresources, UINT64 BaseOffset,
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pLayouts, UINT* pNumRows,
    UINT64* pRowSizeInBytes, UINT64* pTotalBytes) {
    TRACE(
        "WrappedD3D12ToD3D11Device::GetCopyableFootprints(%p, %u, %u, %llu, %p, %p, %p, "
        "%p)",
        pResourceDesc, FirstSubresource, NumSubresources, BaseOffset, pLayouts,
        pNumRows, pRowSizeInBytes, pTotalBytes);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateQueryHeap(
    const D3D12_QUERY_HEAP_DESC* pDesc, REFIID riid, void** ppvHeap) {
    TRACE("WrappedD3D12ToD3D11Device::CreateQueryHeap(%p, %s, %p)", pDesc,
          debugstr_guid(&riid).c_str(), ppvHeap);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::SetStablePowerState(BOOL Enable) {
    TRACE("WrappedD3D12ToD3D11Device::SetStablePowerState(%d)", Enable);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11Device::CreateCommandSignature(const D3D12_COMMAND_SIGNATURE_DESC* pDesc,
                                    ID3D12RootSignature* pRootSignature,
                                    REFIID riid, void** ppvCommandSignature) {
    TRACE("WrappedD3D12ToD3D11Device::CreateCommandSignature(%p, %p, %s, %p)", pDesc,
          pRootSignature, debugstr_guid(&riid).c_str(), ppvCommandSignature);
    return E_NOTIMPL;
}

void STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::GetResourceTiling(
    ID3D12Resource* pTiledResource, UINT* pNumTilesForEntireResource,
    D3D12_PACKED_MIP_INFO* pPackedMipDesc,
    D3D12_TILE_SHAPE* pStandardTileShapeForNonPackedMips,
    UINT* pNumSubresourceTilings, UINT FirstSubresourceTilingToGet,
    D3D12_SUBRESOURCE_TILING* pSubresourceTilingsForNonPackedMips) {
    TRACE("WrappedD3D12ToD3D11Device::GetResourceTiling(%p, %p, %p, %p, %p, %u, %p)",
          pTiledResource, pNumTilesForEntireResource, pPackedMipDesc,
          pStandardTileShapeForNonPackedMips, pNumSubresourceTilings,
          FirstSubresourceTilingToGet, pSubresourceTilingsForNonPackedMips);
}

LUID* STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::GetAdapterLuid(LUID* pLuid) {
    TRACE("WrappedD3D12ToD3D11Device::GetAdapterLuid(%p)", pLuid);
    return pLuid;
}

// ID3D12Device1 methods
HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::SetEventOnMultipleFenceCompletion(
    ID3D12Fence* const* ppFences, const UINT64* pFenceValues, UINT NumFences,
    D3D12_MULTIPLE_FENCE_WAIT_FLAGS Flags, HANDLE hEvent) {
    TRACE(
        "WrappedD3D12ToD3D11Device::SetEventOnMultipleFenceCompletion called on object %p",
        this);
    WARN("Multiple fence completion events are not supported.");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11Device::CreatePipelineState(const D3D12_PIPELINE_STATE_STREAM_DESC* pDesc,
                                 REFIID riid, void** ppPipelineState) {
    TRACE("WrappedD3D12ToD3D11Device::CreatePipelineState called on object %p", this);
    TRACE("  Desc: %p, riid: %s, ppPipelineState: %p", pDesc,
          debugstr_guid(&riid).c_str(), ppPipelineState);
    return E_NOTIMPL;
}

// ID3D12Device2 methods
HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreatePipelineLibrary(
    const void* pLibraryBlob, SIZE_T BlobLengthInBytes, REFIID riid,
    void** ppPipelineLibrary) {
    TRACE("WrappedD3D12ToD3D11Device::CreatePipelineLibrary called on object %p", this);
    TRACE("  Blob: %p, length: %zu, riid: %s, ppPipelineLibrary: %p",
          pLibraryBlob, BlobLengthInBytes, debugstr_guid(&riid).c_str(),
          ppPipelineLibrary);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::SetResidencyPriority(
    UINT NumObjects, ID3D12Pageable* const* ppObjects,
    const D3D12_RESIDENCY_PRIORITY* pPriorities) {
    TRACE("WrappedD3D12ToD3D11Device::SetResidencyPriority called on object %p", this);
    TRACE("  NumObjects: %u, ppObjects: %p, pPriorities: %p", NumObjects,
          ppObjects, pPriorities);
    return E_NOTIMPL;
}

// ID3D12DebugDevice methods
HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11Device::SetFeatureMask(D3D12_DEBUG_FEATURE Mask) {
    TRACE("WrappedD3D12ToD3D11Device::SetFeatureMask called on object %p", this);
    TRACE("  Mask: %d", Mask);
    return E_NOTIMPL;
}

D3D12_DEBUG_FEATURE STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::GetFeatureMask() {
    TRACE("WrappedD3D12ToD3D11Device::GetFeatureMask called on object %p", this);
    return D3D12_DEBUG_FEATURE_NONE;
}

HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11Device::ReportLiveDeviceObjects(D3D12_RLDO_FLAGS Flags) {
    TRACE("WrappedD3D12ToD3D11Device::ReportLiveDeviceObjects called on object %p", this);
    TRACE("  Flags: %d", Flags);
    return S_OK;  // Pretend we reported
}

// ID3D11Device methods
HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateBuffer(
    const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData,
    ID3D11Buffer** ppBuffer) {
        TRACE("WrappedD3D12ToD3D11Device::CreateBuffer called on object %p", this);
    return m_d3d11Device->CreateBuffer(pDesc, pInitialData, ppBuffer);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateTexture1D(
    const D3D11_TEXTURE1D_DESC* pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture1D** ppTexture1D) {
        TRACE("WrappedD3D12ToD3D11Device::CreateTexture1D called on object %p", this);
    return m_d3d11Device->CreateTexture1D(pDesc, pInitialData, ppTexture1D);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateTexture2D(
    const D3D11_TEXTURE2D_DESC* pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture2D** ppTexture2D) {
        TRACE("WrappedD3D12ToD3D11Device::CreateTexture2D called on object %p", this);
    return m_d3d11Device->CreateTexture2D(pDesc, pInitialData, ppTexture2D);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateTexture3D(
    const D3D11_TEXTURE3D_DESC* pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture3D** ppTexture3D) {
        TRACE("WrappedD3D12ToD3D11Device::CreateTexture3D called on object %p", this);
    return m_d3d11Device->CreateTexture3D(pDesc, pInitialData, ppTexture3D);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateShaderResourceView(
    ID3D11Resource* pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc,
    ID3D11ShaderResourceView** ppSRView) {
        TRACE("WrappedD3D12ToD3D11Device::CreateShaderResourceView called on object %p", this);
    return m_d3d11Device->CreateShaderResourceView(pResource, pDesc, ppSRView);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateUnorderedAccessView(
    ID3D11Resource* pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc,
    ID3D11UnorderedAccessView** ppUAView) {
        TRACE("WrappedD3D12ToD3D11Device::CreateUnorderedAccessView called on object %p", this);
    return m_d3d11Device->CreateUnorderedAccessView(pResource, pDesc, ppUAView);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateRenderTargetView(
    ID3D11Resource* pResource, const D3D11_RENDER_TARGET_VIEW_DESC* pDesc,
    ID3D11RenderTargetView** ppRTView) {
        TRACE("WrappedD3D12ToD3D11Device::CreateRenderTargetView called on object %p", this);
    return m_d3d11Device->CreateRenderTargetView(pResource, pDesc, ppRTView);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateDepthStencilView(
    ID3D11Resource* pResource, const D3D11_DEPTH_STENCIL_VIEW_DESC* pDesc,
    ID3D11DepthStencilView** ppDepthStencilView) {
        TRACE("WrappedD3D12ToD3D11Device::CreateDepthStencilView called on object %p", this);
    return m_d3d11Device->CreateDepthStencilView(pResource, pDesc,
                                                 ppDepthStencilView);
}
HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateInputLayout(
    const D3D11_INPUT_ELEMENT_DESC* pInputElementDescs, UINT NumElements,
    const void* pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength,
    ID3D11InputLayout** ppInputLayout) {
        TRACE("WrappedD3D12ToD3D11Device::CreateInputLayout called on object %p", this);
    return m_d3d11Device->CreateInputLayout(pInputElementDescs, NumElements,
                                            pShaderBytecodeWithInputSignature,
                                            BytecodeLength, ppInputLayout);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateVertexShader(
    const void* pShaderBytecode, SIZE_T BytecodeLength,
    ID3D11ClassLinkage* pClassLinkage, ID3D11VertexShader** ppVertexShader) {
        TRACE("WrappedD3D12ToD3D11Device::CreateVertexShader called on object %p", this);
    return m_d3d11Device->CreateVertexShader(pShaderBytecode, BytecodeLength,
                                             pClassLinkage, ppVertexShader);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateGeometryShader(
    const void* pShaderBytecode, SIZE_T BytecodeLength,
    ID3D11ClassLinkage* pClassLinkage,
    ID3D11GeometryShader** ppGeometryShader) {
        TRACE("WrappedD3D12ToD3D11Device::CreateGeometryShader called on object %p", this);
    return m_d3d11Device->CreateGeometryShader(pShaderBytecode, BytecodeLength,
                                               pClassLinkage, ppGeometryShader);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateGeometryShaderWithStreamOutput(
    const void* pShaderBytecode, SIZE_T BytecodeLength,
    const D3D11_SO_DECLARATION_ENTRY* pSODeclaration, UINT NumEntries,
    const UINT* pBufferStrides, UINT NumStrides, UINT RasterizedStream,
    ID3D11ClassLinkage* pClassLinkage,
    ID3D11GeometryShader** ppGeometryShader) {
        TRACE("WrappedD3D12ToD3D11Device::CreateGeometryShaderWithStreamOutput called on object %p", this);
    return m_d3d11Device->CreateGeometryShaderWithStreamOutput(
        pShaderBytecode, BytecodeLength, pSODeclaration, NumEntries,
        pBufferStrides, NumStrides, RasterizedStream, pClassLinkage,
        ppGeometryShader);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreatePixelShader(
    const void* pShaderBytecode, SIZE_T BytecodeLength,
    ID3D11ClassLinkage* pClassLinkage, ID3D11PixelShader** ppPixelShader) {
        TRACE("WrappedD3D12ToD3D11Device::CreatePixelShader called on object %p", this);
    return m_d3d11Device->CreatePixelShader(pShaderBytecode, BytecodeLength,
                                            pClassLinkage, ppPixelShader);
}
HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateHullShader(
    const void* pShaderBytecode, SIZE_T BytecodeLength,
    ID3D11ClassLinkage* pClassLinkage, ID3D11HullShader** ppHullShader) {
        TRACE("WrappedD3D12ToD3D11Device::CreateHullShader called on object %p", this);
    return m_d3d11Device->CreateHullShader(pShaderBytecode, BytecodeLength,
                                           pClassLinkage, ppHullShader);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateDomainShader(
    const void* pShaderBytecode, SIZE_T BytecodeLength,
    ID3D11ClassLinkage* pClassLinkage, ID3D11DomainShader** ppDomainShader) {
        TRACE("WrappedD3D12ToD3D11Device::CreateDomainShader called on object %p", this);
    return m_d3d11Device->CreateDomainShader(pShaderBytecode, BytecodeLength,
                                             pClassLinkage, ppDomainShader);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateComputeShader(
    const void* pShaderBytecode, SIZE_T BytecodeLength,
    ID3D11ClassLinkage* pClassLinkage, ID3D11ComputeShader** ppComputeShader) {
        TRACE("WrappedD3D12ToD3D11Device::CreateComputeShader called on object %p", this);
    return m_d3d11Device->CreateComputeShader(pShaderBytecode, BytecodeLength,
                                              pClassLinkage, ppComputeShader);
}

HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11Device::CreateClassLinkage(ID3D11ClassLinkage** ppLinkage) {
        TRACE("WrappedD3D12ToD3D11Device::CreateClassLinkage called on object %p", this);
    return m_d3d11Device->CreateClassLinkage(ppLinkage);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateBlendState(
    const D3D11_BLEND_DESC* pBlendStateDesc, ID3D11BlendState** ppBlendState) {
        TRACE("WrappedD3D12ToD3D11Device::CreateBlendState called on object %p", this);
    return m_d3d11Device->CreateBlendState(pBlendStateDesc, ppBlendState);
}
HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateDepthStencilState(
    const D3D11_DEPTH_STENCIL_DESC* pDepthStencilDesc,
    ID3D11DepthStencilState** ppDepthStencilState) {
        TRACE("WrappedD3D12ToD3D11Device::CreateDepthStencilState called on object %p", this);
    return m_d3d11Device->CreateDepthStencilState(pDepthStencilDesc,
                                                  ppDepthStencilState);
}

HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11Device::CreateRasterizerState(const D3D11_RASTERIZER_DESC* pRasterizerDesc,
                                   ID3D11RasterizerState** ppRasterizerState) {
    TRACE("WrappedD3D12ToD3D11Device::CreateRasterizerState called on object %p", this);
    return m_d3d11Device->CreateRasterizerState(pRasterizerDesc,
                                                ppRasterizerState);
}

HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11Device::CreateSamplerState(const D3D11_SAMPLER_DESC* pSamplerDesc,
                                ID3D11SamplerState** ppSamplerState) {
    TRACE("WrappedD3D12ToD3D11Device::CreateSamplerState called on object %p", this);
    return m_d3d11Device->CreateSamplerState(pSamplerDesc, ppSamplerState);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateQuery(
    const D3D11_QUERY_DESC* pQueryDesc, ID3D11Query** ppQuery) {
    TRACE("WrappedD3D12ToD3D11Device::CreateQuery called on object %p", this);
    return m_d3d11Device->CreateQuery(pQueryDesc, ppQuery);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreatePredicate(
    const D3D11_QUERY_DESC* pPredicateDesc, ID3D11Predicate** ppPredicate) {
    TRACE("WrappedD3D12ToD3D11Device::CreatePredicate called on object %p", this);
    return m_d3d11Device->CreatePredicate(pPredicateDesc, ppPredicate);
}
HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateCounter(
    const D3D11_COUNTER_DESC* pCounterDesc, ID3D11Counter** ppCounter) {
    TRACE("WrappedD3D12ToD3D11Device::CreateCounter called on object %p", this);
    return m_d3d11Device->CreateCounter(pCounterDesc, ppCounter);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateDeferredContext(
    UINT ContextFlags, ID3D11DeviceContext** ppDeferredContext) {
    return m_d3d11Device->CreateDeferredContext(ContextFlags,
                                                ppDeferredContext);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::OpenSharedResource(
    HANDLE hResource, REFIID ReturnedInterface, void** ppResource) {
    TRACE("WrappedD3D12ToD3D11Device::OpenSharedResource called on object %p", this);
    return m_d3d11Device->OpenSharedResource(hResource, ReturnedInterface,
                                             ppResource);
}

HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11Device::CheckFormatSupport(DXGI_FORMAT Format, UINT* pFormatSupport) {
    TRACE("WrappedD3D12ToD3D11Device::CheckFormatSupport called on object %p", this);
    return m_d3d11Device->CheckFormatSupport(Format, pFormatSupport);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CheckMultisampleQualityLevels(
    DXGI_FORMAT Format, UINT SampleCount, UINT* pNumQualityLevels) {
    TRACE("WrappedD3D12ToD3D11Device::CheckMultisampleQualityLevels called on object %p", this);
    return m_d3d11Device->CheckMultisampleQualityLevels(Format, SampleCount,
                                                        pNumQualityLevels);
}
void STDMETHODCALLTYPE
WrappedD3D12ToD3D11Device::GetImmediateContext(ID3D11DeviceContext** ppImmediateContext) {
    TRACE("WrappedD3D12ToD3D11Device::GetImmediateContext called on object %p", this);
    m_d3d11Device->GetImmediateContext(ppImmediateContext);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::SetExceptionMode(UINT RaiseFlags) {
    TRACE("WrappedD3D12ToD3D11Device::SetExceptionMode called on object %p", this);
    return m_d3d11Device->SetExceptionMode(RaiseFlags);
}

UINT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::GetExceptionMode() {
    TRACE("WrappedD3D12ToD3D11Device::GetExceptionMode called on object %p", this);
    return m_d3d11Device->GetExceptionMode();
}

// ID3D11Device1 methods
HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateDeferredContext1(
    UINT ContextFlags, ID3D11DeviceContext1** ppDeferredContext) {
    TRACE("WrappedD3D12ToD3D11Device::CreateDeferredContext1 called on object %p", this);
    return m_d3d11Device1 ? m_d3d11Device1->CreateDeferredContext1(
                                ContextFlags, ppDeferredContext)
                          : E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11Device::CreateBlendState1(const D3D11_BLEND_DESC1* pBlendStateDesc,
                               ID3D11BlendState1** ppBlendState) {
    TRACE("WrappedD3D12ToD3D11Device::CreateBlendState1 called on object %p", this);
    return m_d3d11Device1 ? m_d3d11Device1->CreateBlendState1(pBlendStateDesc,
                                                              ppBlendState)
                          : E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateRasterizerState1(
    const D3D11_RASTERIZER_DESC1* pRasterizerDesc,
    ID3D11RasterizerState1** ppRasterizerState) {
    TRACE("WrappedD3D12ToD3D11Device::CreateRasterizerState1 called on object %p", this);
    return m_d3d11Device1 ? m_d3d11Device1->CreateRasterizerState1(
                                pRasterizerDesc, ppRasterizerState)
                          : E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateDeviceContextState(
    UINT Flags, const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels,
    UINT SDKVersion, REFIID EmulatedInterface,
    D3D_FEATURE_LEVEL* pChosenFeatureLevel,
    ID3DDeviceContextState** ppContextState) {
    TRACE("WrappedD3D12ToD3D11Device::CreateDeviceContextState called on object %p", this);
    return m_d3d11Device1
               ? m_d3d11Device1->CreateDeviceContextState(
                     Flags, pFeatureLevels, FeatureLevels, SDKVersion,
                     EmulatedInterface, pChosenFeatureLevel, ppContextState)
               : E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::OpenSharedResource1(
    HANDLE hResource, REFIID returnedInterface, void** ppResource) {
    TRACE("WrappedD3D12ToD3D11Device::OpenSharedResource1 called on object %p", this);
    return m_d3d11Device1 ? m_d3d11Device1->OpenSharedResource1(
                                hResource, returnedInterface, ppResource)
                          : E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::OpenSharedResourceByName(
    LPCWSTR lpName, DWORD dwDesiredAccess, REFIID returnedInterface,
    void** ppResource) {
    TRACE("WrappedD3D12ToD3D11Device::OpenSharedResourceByName called on object %p", this);
    TRACE("  lpName: %ls", lpName);
    TRACE("  dwDesiredAccess: %lu", dwDesiredAccess);
    TRACE("  returnedInterface: %p", returnedInterface);
    TRACE("  ppResource: %p", ppResource);
    return m_d3d11Device1
               ? m_d3d11Device1->OpenSharedResourceByName(
                     lpName, dwDesiredAccess, returnedInterface, ppResource)
               : E_NOTIMPL;
}
// ID3D11Device1 methods
void STDMETHODCALLTYPE
WrappedD3D12ToD3D11Device::GetImmediateContext1(ID3D11DeviceContext1** ppImmediateContext) {
    TRACE("WrappedD3D12ToD3D11Device::GetImmediateContext1 called on object %p", this);
    if (m_d3d11Device1) {
        m_d3d11Device1->GetImmediateContext1(ppImmediateContext);
    } else {
        *ppImmediateContext = nullptr;
    }
}

// ID3D11Device2 methods
void STDMETHODCALLTYPE
WrappedD3D12ToD3D11Device::GetImmediateContext2(ID3D11DeviceContext2** ppImmediateContext) {
    TRACE("WrappedD3D12ToD3D11Device::GetImmediateContext2 called on object %p", this);
    if (m_d3d11Device2) {
        m_d3d11Device2->GetImmediateContext2(ppImmediateContext);
    } else {
        *ppImmediateContext = nullptr;
    }
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CreateDeferredContext2(
    UINT ContextFlags, ID3D11DeviceContext2** ppDeferredContext) {
    TRACE("WrappedD3D12ToD3D11Device::CreateDeferredContext2 called on object %p", this);
    return m_d3d11Device2 ? m_d3d11Device2->CreateDeferredContext2(
                                ContextFlags, ppDeferredContext)
                          : E_NOTIMPL;
}

void STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::GetResourceTiling(
    ID3D11Resource* pTiledResource, UINT* pNumTilesForEntireResource,
    D3D11_PACKED_MIP_DESC* pPackedMipDesc,
    D3D11_TILE_SHAPE* pStandardTileShapeForNonPackedMips,
    UINT* pNumSubresourceTilings, UINT FirstSubresourceTilingToGet,
    D3D11_SUBRESOURCE_TILING* pSubresourceTilingsForNonPackedMips) {
    TRACE("WrappedD3D12ToD3D11Device::GetResourceTiling called on object %p", this);
    if (m_d3d11Device2) {
        m_d3d11Device2->GetResourceTiling(
            pTiledResource, pNumTilesForEntireResource, pPackedMipDesc,
            pStandardTileShapeForNonPackedMips, pNumSubresourceTilings,
            FirstSubresourceTilingToGet, pSubresourceTilingsForNonPackedMips);
    }
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CheckMultisampleQualityLevels1(
    DXGI_FORMAT Format, UINT SampleCount, UINT Flags, UINT* pNumQualityLevels) {
    TRACE("WrappedD3D12ToD3D11Device::CheckMultisampleQualityLevels1 called on object %p",
          this);
    return m_d3d11Device2 ? m_d3d11Device2->CheckMultisampleQualityLevels1(
                                Format, SampleCount, Flags, pNumQualityLevels)
                          : E_NOTIMPL;
}

void STDMETHODCALLTYPE
WrappedD3D12ToD3D11Device::CheckCounterInfo(D3D11_COUNTER_INFO* pCounterInfo) {
    TRACE("WrappedD3D12ToD3D11Device::CheckCounterInfo called on object %p", this);
    if (m_d3d11Device) m_d3d11Device->CheckCounterInfo(pCounterInfo);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CheckCounter(
    const D3D11_COUNTER_DESC* pDesc, D3D11_COUNTER_TYPE* pType,
    UINT* pActiveCounters, LPSTR szName, UINT* pNameLength, LPSTR szUnits,
    UINT* pUnitsLength, LPSTR szDescription, UINT* pDescriptionLength) {
    TRACE("WrappedD3D12ToD3D11Device::CheckCounter called on object %p", this);
    return m_d3d11Device
               ? m_d3d11Device->CheckCounter(
                     pDesc, pType, pActiveCounters, szName, pNameLength,
                     szUnits, pUnitsLength, szDescription, pDescriptionLength)
               : E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::CheckFeatureSupport(
    D3D11_FEATURE Feature, void* pFeatureSupportData,
    UINT FeatureSupportDataSize) {
    TRACE("WrappedD3D12ToD3D11Device::CheckFeatureSupport called on object %p", this);
    return m_d3d11Device
               ? m_d3d11Device->CheckFeatureSupport(
                     Feature, pFeatureSupportData, FeatureSupportDataSize)
               : E_NOTIMPL;
}

D3D_FEATURE_LEVEL STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::GetFeatureLevel() {
    TRACE("WrappedD3D12ToD3D11Device::GetFeatureLevel called on object %p", this);
    return m_d3d11Device ? m_d3d11Device->GetFeatureLevel()
                         : D3D_FEATURE_LEVEL_11_0;
}

UINT STDMETHODCALLTYPE WrappedD3D12ToD3D11Device::GetCreationFlags() {
    TRACE("WrappedD3D12ToD3D11Device::GetCreationFlags called on object %p", this);
    return m_d3d11Device ? m_d3d11Device->GetCreationFlags() : 0;
}

ID3D11Resource* WrappedD3D12ToD3D11Device::GetD3D11Resource(ID3D12Resource* d3d12Resource) {
    TRACE("WrappedD3D12ToD3D11Device::GetD3D11Resource called on object %p", this);
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

    ERR("D3D11 resource not found for D3D12 resource %p", d3d12Resource);
    return nullptr;
}

}  // namespace dxiided
