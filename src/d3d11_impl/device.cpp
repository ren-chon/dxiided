#include "d3d11_impl/device.hpp"
#include <dxgi1_2.h>

namespace dxiided {

D3D11Device::D3D11Device(Microsoft::WRL::ComPtr<ID3D11Device> device,
                         Microsoft::WRL::ComPtr<ID3D11DeviceContext> context,
                         D3D_FEATURE_LEVEL feature_level)
    : m_d3d11Device(device)
    , m_d3d11Context(context)
    , m_featureLevel(feature_level) {
}

HRESULT D3D11Device::Create(IUnknown* adapter,
                           D3D_FEATURE_LEVEL minimum_feature_level,
                           REFIID riid,
                           void** device) {
    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
    if (adapter) {
        if (FAILED(adapter->QueryInterface(IID_PPV_ARGS(&dxgi_adapter)))) {
            ERR("Failed to get DXGI adapter.\n");
            return E_INVALIDARG;
        }
    }

    // Create D3D11 device
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_context;
    D3D_FEATURE_LEVEL feature_level;

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    // Try feature levels in descending order
    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    HRESULT hr = D3D11CreateDevice(
        dxgi_adapter.Get(),
        dxgi_adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        feature_levels,
        ARRAYSIZE(feature_levels),
        D3D11_SDK_VERSION,
        &d3d11_device,
        &feature_level,
        &d3d11_context);

    if (FAILED(hr)) {
        ERR("D3D11CreateDevice failed with error %#x.\n", hr);
        return hr;
    }

    if (feature_level < minimum_feature_level) {
        WARN("Created device with feature level %#x, which is lower than requested %#x.\n",
             feature_level, minimum_feature_level);
    }

    // Create our device wrapper
    Microsoft::WRL::ComPtr<D3D11Device> d3d12_device = 
        new D3D11Device(d3d11_device, d3d11_context, feature_level);

    hr = d3d12_device.CopyTo(reinterpret_cast<ID3D12Device**>(device));
    if (FAILED(hr)) {
        ERR("Failed to copy device pointer.\n");
        return hr;
    }

    return S_OK;
}

// IUnknown methods
HRESULT STDMETHODCALLTYPE D3D11Device::QueryInterface(REFIID riid, void** ppvObject) {
    if (!ppvObject) {
        return E_POINTER;
    }

    if (riid == __uuidof(ID3D12Device2) || 
        riid == __uuidof(ID3D12Device) || 
        riid == __uuidof(IUnknown)) {
        AddRef();
        *ppvObject = this;
        return S_OK;
    }

    WARN("Unknown interface %s.\n", debugstr_guid(&riid).c_str());
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE D3D11Device::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

ULONG STDMETHODCALLTYPE D3D11Device::Release() {
    ULONG ref = InterlockedDecrement(&m_refCount);
    if (ref == 0) {
        delete this;
    }
    return ref;
}

// ID3D12Object methods
HRESULT STDMETHODCALLTYPE D3D11Device::GetPrivateData(REFGUID guid, UINT* pDataSize, void* pData) {
    return m_d3d11Device->GetPrivateData(guid, pDataSize, pData);
}

HRESULT STDMETHODCALLTYPE D3D11Device::SetPrivateData(REFGUID guid, UINT DataSize, const void* pData) {
    return m_d3d11Device->SetPrivateData(guid, DataSize, pData);
}

HRESULT STDMETHODCALLTYPE D3D11Device::SetPrivateDataInterface(REFGUID guid, const IUnknown* pData) {
    return m_d3d11Device->SetPrivateDataInterface(guid, pData);
}

HRESULT STDMETHODCALLTYPE D3D11Device::SetName(LPCWSTR Name) {
    return m_d3d11Device->SetPrivateData(WKPDID_D3DDebugObjectName,
        static_cast<UINT>((wcslen(Name) + 1) * sizeof(WCHAR)), Name);
}

// ID3D12Device methods
UINT STDMETHODCALLTYPE D3D11Device::GetNodeCount() {
    return 1; // D3D11 doesn't support multiple nodes
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateCommandQueue(
    const D3D12_COMMAND_QUEUE_DESC* pDesc, REFIID riid, void** ppCommandQueue) {
    TRACE("%p, %s, %p\n", pDesc, debugstr_guid(&riid).c_str(), ppCommandQueue);
    
    if (!pDesc || !ppCommandQueue) {
        ERR("Invalid arguments.\n");
        return E_INVALIDARG;
    }

    if (pDesc->Type != D3D12_COMMAND_LIST_TYPE_DIRECT &&
        pDesc->Type != D3D12_COMMAND_LIST_TYPE_COMPUTE &&
        pDesc->Type != D3D12_COMMAND_LIST_TYPE_COPY) {
        ERR("Invalid command queue type %d.\n", pDesc->Type);
        return E_INVALIDARG;
    }

    return D3D11CommandQueue::Create(this, pDesc, riid, ppCommandQueue);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateCommandAllocator(
    D3D12_COMMAND_LIST_TYPE type,
    REFIID riid,
    void** ppCommandAllocator) {
    
    // TODO: Implement command allocator creation
    FIXME("Command allocator creation not implemented yet.\n");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateGraphicsPipelineState(
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc,
    REFIID riid,
    void** ppPipelineState) {
    
    // TODO: Implement graphics pipeline state creation
    FIXME("Graphics pipeline state creation not implemented yet.\n");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateComputePipelineState(
    const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc,
    REFIID riid,
    void** ppPipelineState) {
    
    // TODO: Implement compute pipeline state creation
    FIXME("Compute pipeline state creation not implemented yet.\n");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateCommandList(
    UINT nodeMask,
    D3D12_COMMAND_LIST_TYPE type,
    ID3D12CommandAllocator* pCommandAllocator,
    ID3D12PipelineState* pInitialState,
    REFIID riid,
    void** ppCommandList) {
    
    // TODO: Implement command list creation
    FIXME("Command list creation not implemented yet.\n");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateDescriptorHeap(
    const D3D12_DESCRIPTOR_HEAP_DESC* pDescriptorHeapDesc,
    REFIID riid,
    void** ppvHeap) {
    
    // TODO: Implement descriptor heap creation
    FIXME("Descriptor heap creation not implemented yet.\n");
    return E_NOTIMPL;
}

UINT STDMETHODCALLTYPE D3D11Device::GetDescriptorHandleIncrementSize(
    D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapType) {
    
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
    TRACE("(%d, %p, %u)\n", Feature, pFeatureSupportData, FeatureSupportDataSize);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateRootSignature(
    UINT nodeMask, const void* pBlobWithRootSignature,
    SIZE_T blobLengthInBytes, REFIID riid, void** ppvRootSignature) {
    TRACE("(%u, %p, %zu, %s, %p)\n", nodeMask, pBlobWithRootSignature,
          blobLengthInBytes, debugstr_guid(&riid).c_str(), ppvRootSignature);
    return E_NOTIMPL;
}

void STDMETHODCALLTYPE D3D11Device::CreateConstantBufferView(
    const D3D12_CONSTANT_BUFFER_VIEW_DESC* pDesc,
    D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) {
    TRACE("(%p, %p)\n", pDesc, (void*)DestDescriptor.ptr);
}

void STDMETHODCALLTYPE D3D11Device::CreateShaderResourceView(
    ID3D12Resource* pResource,
    const D3D12_SHADER_RESOURCE_VIEW_DESC* pDesc,
    D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) {
    TRACE("(%p, %p, %p)\n", pResource, pDesc, (void*)DestDescriptor.ptr);
}

void STDMETHODCALLTYPE D3D11Device::CreateUnorderedAccessView(
    ID3D12Resource* pResource, ID3D12Resource* pCounterResource,
    const D3D12_UNORDERED_ACCESS_VIEW_DESC* pDesc,
    D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) {
    TRACE("(%p, %p, %p, %p)\n", pResource, pCounterResource, pDesc,
          (void*)DestDescriptor.ptr);
}

void STDMETHODCALLTYPE D3D11Device::CreateRenderTargetView(
    ID3D12Resource* pResource,
    const D3D12_RENDER_TARGET_VIEW_DESC* pDesc,
    D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) {
    TRACE("(%p, %p, %p)\n", pResource, pDesc, (void*)DestDescriptor.ptr);
}

void STDMETHODCALLTYPE D3D11Device::CreateDepthStencilView(
    ID3D12Resource* pResource,
    const D3D12_DEPTH_STENCIL_VIEW_DESC* pDesc,
    D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) {
    TRACE("(%p, %p, %p)\n", pResource, pDesc, (void*)DestDescriptor.ptr);
}

void STDMETHODCALLTYPE D3D11Device::CreateSampler(
    const D3D12_SAMPLER_DESC* pDesc,
    D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) {
    TRACE("(%p, %p)\n", pDesc, (void*)DestDescriptor.ptr);
}

void STDMETHODCALLTYPE D3D11Device::CopyDescriptors(
    UINT NumDestDescriptorRanges,
    const D3D12_CPU_DESCRIPTOR_HANDLE* pDestDescriptorRangeStarts,
    const UINT* pDestDescriptorRangeSizes,
    UINT NumSrcDescriptorRanges,
    const D3D12_CPU_DESCRIPTOR_HANDLE* pSrcDescriptorRangeStarts,
    const UINT* pSrcDescriptorRangeSizes,
    D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType) {
    TRACE("(%u, %p, %p, %u, %p, %p, %d)\n", NumDestDescriptorRanges,
          pDestDescriptorRangeStarts, pDestDescriptorRangeSizes,
          NumSrcDescriptorRanges, pSrcDescriptorRangeStarts,
          pSrcDescriptorRangeSizes, DescriptorHeapsType);
}

void STDMETHODCALLTYPE D3D11Device::CopyDescriptorsSimple(
    UINT NumDescriptors,
    D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptorRangeStart,
    D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptorRangeStart,
    D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType) {
    TRACE("(%u, %p, %p, %d)\n", NumDescriptors,
          (void*)DestDescriptorRangeStart.ptr,
          (void*)SrcDescriptorRangeStart.ptr, DescriptorHeapsType);
}

D3D12_RESOURCE_ALLOCATION_INFO* STDMETHODCALLTYPE
D3D11Device::GetResourceAllocationInfo(
    D3D12_RESOURCE_ALLOCATION_INFO* info, UINT visibleMask,
    UINT numResourceDescs, const D3D12_RESOURCE_DESC* pResourceDescs) {
    TRACE("(%p, %u, %u, %p)\n", info, visibleMask, numResourceDescs,
          pResourceDescs);
    return info;
}

D3D12_HEAP_PROPERTIES* STDMETHODCALLTYPE D3D11Device::GetCustomHeapProperties(
    D3D12_HEAP_PROPERTIES* props, UINT nodeMask, D3D12_HEAP_TYPE heapType) {
    TRACE("(%p, %u, %d)\n", props, nodeMask, heapType);
    return props;
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateCommittedResource(
    const D3D12_HEAP_PROPERTIES* pHeapProperties,
    D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC* pDesc,
    D3D12_RESOURCE_STATES InitialResourceState,
    const D3D12_CLEAR_VALUE* pOptimizedClearValue,
    REFIID riidResource, void** ppvResource) {
    TRACE("(%p, %u, %p, %u, %p, %s, %p)\n", pHeapProperties, HeapFlags, pDesc,
          InitialResourceState, pOptimizedClearValue,
          debugstr_guid(&riidResource).c_str(), ppvResource);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateHeap(
    const D3D12_HEAP_DESC* pDesc, REFIID riid, void** ppvHeap) {
    TRACE("(%p, %s, %p)\n", pDesc, debugstr_guid(&riid).c_str(), ppvHeap);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreatePlacedResource(
    ID3D12Heap* pHeap, UINT64 HeapOffset,
    const D3D12_RESOURCE_DESC* pDesc,
    D3D12_RESOURCE_STATES InitialState,
    const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riid,
    void** ppvResource) {
    TRACE("(%p, %llu, %p, %u, %p, %s, %p)\n", pHeap, HeapOffset, pDesc,
          InitialState, pOptimizedClearValue, debugstr_guid(&riid).c_str(),
          ppvResource);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateReservedResource(
    const D3D12_RESOURCE_DESC* pDesc,
    D3D12_RESOURCE_STATES InitialState,
    const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riid,
    void** ppvResource) {
    TRACE("(%p, %u, %p, %s, %p)\n", pDesc, InitialState,
          pOptimizedClearValue, debugstr_guid(&riid).c_str(), ppvResource);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateSharedHandle(
    ID3D12DeviceChild* pObject,
    const SECURITY_ATTRIBUTES* pAttributes, DWORD Access,
    LPCWSTR Name, HANDLE* pHandle) {
    TRACE("(%p, %p, %u, %s, %p)\n", pObject, pAttributes, Access,
          debugstr_w(Name).c_str(), pHandle);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::OpenSharedHandle(
    HANDLE NTHandle, REFIID riid, void** ppvObj) {
    TRACE("(%p, %s, %p)\n", NTHandle, debugstr_guid(&riid).c_str(), ppvObj);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::OpenSharedHandleByName(
    LPCWSTR Name, DWORD Access, HANDLE* pNTHandle) {
    TRACE("(%s, %u, %p)\n", debugstr_w(Name).c_str(), Access, pNTHandle);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::MakeResident(
    UINT NumObjects, ID3D12Pageable* const* ppObjects) {
    TRACE("(%u, %p)\n", NumObjects, ppObjects);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::Evict(
    UINT NumObjects, ID3D12Pageable* const* ppObjects) {
    TRACE("(%u, %p)\n", NumObjects, ppObjects);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateFence(
    UINT64 InitialValue, D3D12_FENCE_FLAGS Flags,
    REFIID riid, void** ppFence) {
    TRACE("(%llu, %u, %s, %p)\n", InitialValue, Flags,
          debugstr_guid(&riid).c_str(), ppFence);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::GetDeviceRemovedReason() {
    TRACE("()\n");
    return S_OK;
}

void STDMETHODCALLTYPE D3D11Device::GetCopyableFootprints(
    const D3D12_RESOURCE_DESC* pResourceDesc,
    UINT FirstSubresource, UINT NumSubresources,
    UINT64 BaseOffset,
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pLayouts,
    UINT* pNumRows, UINT64* pRowSizeInBytes,
    UINT64* pTotalBytes) {
    TRACE("(%p, %u, %u, %llu, %p, %p, %p, %p)\n", pResourceDesc,
          FirstSubresource, NumSubresources, BaseOffset, pLayouts,
          pNumRows, pRowSizeInBytes, pTotalBytes);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateQueryHeap(
    const D3D12_QUERY_HEAP_DESC* pDesc,
    REFIID riid, void** ppvHeap) {
    TRACE("(%p, %s, %p)\n", pDesc, debugstr_guid(&riid).c_str(), ppvHeap);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::SetStablePowerState(BOOL Enable) {
    TRACE("(%d)\n", Enable);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateCommandSignature(
    const D3D12_COMMAND_SIGNATURE_DESC* pDesc,
    ID3D12RootSignature* pRootSignature,
    REFIID riid, void** ppvCommandSignature) {
    TRACE("(%p, %p, %s, %p)\n", pDesc, pRootSignature,
          debugstr_guid(&riid).c_str(), ppvCommandSignature);
    return E_NOTIMPL;
}

void STDMETHODCALLTYPE D3D11Device::GetResourceTiling(
    ID3D12Resource* pTiledResource,
    UINT* pNumTilesForEntireResource,
    D3D12_PACKED_MIP_INFO* pPackedMipDesc,
    D3D12_TILE_SHAPE* pStandardTileShapeForNonPackedMips,
    UINT* pNumSubresourceTilings,
    UINT FirstSubresourceTilingToGet,
    D3D12_SUBRESOURCE_TILING* pSubresourceTilingsForNonPackedMips) {
    TRACE("(%p, %p, %p, %p, %p, %u, %p)\n", pTiledResource,
          pNumTilesForEntireResource, pPackedMipDesc,
          pStandardTileShapeForNonPackedMips, pNumSubresourceTilings,
          FirstSubresourceTilingToGet, pSubresourceTilingsForNonPackedMips);
}

LUID* STDMETHODCALLTYPE D3D11Device::GetAdapterLuid(LUID* pLuid) {
    TRACE("(%p)\n", pLuid);
    return pLuid;
}

// ID3D12Device1 methods
HRESULT STDMETHODCALLTYPE D3D11Device::SetEventOnMultipleFenceCompletion(
    ID3D12Fence* const* ppFences,
    const UINT64* pFenceValues,
    UINT NumFences,
    D3D12_MULTIPLE_FENCE_WAIT_FLAGS Flags,
    HANDLE hEvent) {
    WARN("Multiple fence completion events are not supported.\n");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreatePipelineState(
    const D3D12_PIPELINE_STATE_STREAM_DESC* pDesc,
    REFIID riid,
    void** ppPipelineState) {
    WARN("Pipeline state streams are not supported.\n");
    return E_NOTIMPL;
}

// ID3D12Device2 methods
HRESULT STDMETHODCALLTYPE D3D11Device::CreatePipelineLibrary(
    const void* pLibraryBlob,
    SIZE_T BlobLengthInBytes,
    REFIID riid,
    void** ppPipelineLibrary) {
    WARN("Pipeline libraries are not supported.\n");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Device::SetResidencyPriority(
    UINT NumObjects,
    ID3D12Pageable* const* ppObjects,
    const D3D12_RESIDENCY_PRIORITY* pPriorities) {
    WARN("Residency priorities are not supported.\n");
    return E_NOTIMPL;
}

} // namespace dxiided
