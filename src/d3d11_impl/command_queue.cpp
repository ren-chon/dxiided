#include "d3d11_impl/command_queue.hpp"
#include "d3d11_impl/command_list.hpp"
#include "d3d11_impl/device.hpp"
#include "d3d11_impl/swap_chain.hpp"

namespace dxiided {

HRESULT D3D11CommandQueue::Create(D3D11Device* device,
                                  const D3D12_COMMAND_QUEUE_DESC* desc,
                                  REFIID riid, void** command_queue) {
    TRACE("D3D11CommandQueue::Create %p, %p, %s, %p", device, desc,
          debugstr_guid(&riid).c_str(), command_queue);

    if (!device || !desc || !command_queue) {
        return E_INVALIDARG;
    }

    Microsoft::WRL::ComPtr<D3D11CommandQueue> queue =
        new D3D11CommandQueue(device, desc);

    return queue.CopyTo(reinterpret_cast<ID3D12CommandQueue**>(command_queue));
}

D3D11CommandQueue::D3D11CommandQueue(D3D11Device* device,
                                     const D3D12_COMMAND_QUEUE_DESC* desc)
    : m_device(device), m_desc(*desc) {
    TRACE(
        "D3D11CommandQueue::D3D11CommandQueue called %p, Type=%d, Priority=%d, "
        "Flags=%d",
        device, desc->Type, desc->Priority, desc->Flags);

    m_immediateContext =
        Microsoft::WRL::ComPtr<ID3D11DeviceContext>(device->GetD3D11Context());
}

// IUnknown methods
HRESULT STDMETHODCALLTYPE D3D11CommandQueue::QueryInterface(REFIID riid,
                                                            void** ppvObject) {
    TRACE("D3D11CommandQueue::QueryInterface(%s, %p)",
          debugstr_guid(&riid).c_str(), ppvObject);

    if (!ppvObject) {
        return E_POINTER;
    }

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D12Object) ||
        riid == __uuidof(ID3D12DeviceChild) ||
        riid == __uuidof(ID3D12CommandQueue)) {
        *ppvObject = this;
        AddRef();
        return S_OK;
    }
    if (riid == IID_IWineDXGISwapChainFactory) {
        TRACE("Returning IWineDXGISwapChainFactory interface");
        *ppvObject = static_cast<IWineDXGISwapChainFactory*>(this);
        AddRef();
        return S_OK;
    }

    WARN("Unknown interface %s.", debugstr_guid(&riid).c_str());
    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE D3D11CommandQueue::AddRef() {
    ULONG ref = InterlockedIncrement(&m_refCount);
    TRACE("D3D11CommandQueue::AddRef %p increasing refcount to %u.", this,
          ref);
    return ref;
}

ULONG STDMETHODCALLTYPE D3D11CommandQueue::Release() {
    ULONG ref = InterlockedDecrement(&m_refCount);
    TRACE("D3D11CommandQueue::Release %p decreasing refcount to %u.", this,
          ref);
    if (ref == 0) {
        delete this;
    }
    return ref;
}

// ID3D12Object methods
HRESULT STDMETHODCALLTYPE D3D11CommandQueue::GetPrivateData(REFGUID guid,
                                                            UINT* pDataSize,
                                                            void* pData) {
    TRACE("D3D11CommandQueue::GetPrivateData(%s, %p, %p)",
          debugstr_guid(&guid).c_str(), pDataSize, pData);

    if (!pDataSize) {
        return E_INVALIDARG;
    }

    // Get the D3D11 device context's private data
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11Device;
    HRESULT hr = m_device->QueryInterface(__uuidof(ID3D11Device), &d3d11Device);
    if (SUCCEEDED(hr)) {
        return d3d11Device->GetPrivateData(guid, pDataSize, pData);
    }

    *pDataSize = 0;
    return S_OK;  // Return success but with no data for unknown GUIDs
}

HRESULT STDMETHODCALLTYPE D3D11CommandQueue::SetPrivateData(REFGUID guid,
                                                            UINT DataSize,
                                                            const void* pData) {
    TRACE("D3D11CommandQueue::SetPrivateData %s, %u, %p",
          debugstr_guid(&guid).c_str(), DataSize, pData);
    return m_immediateContext->SetPrivateData(guid, DataSize, pData);
}

HRESULT STDMETHODCALLTYPE D3D11CommandQueue::SetPrivateDataInterface(
    REFGUID guid, const IUnknown* pData) {
    TRACE("D3D11CommandQueue::SetPrivateDataInterface %s, %p",
          debugstr_guid(&guid).c_str(), pData);
    return m_immediateContext->SetPrivateDataInterface(guid, pData);
}

HRESULT STDMETHODCALLTYPE D3D11CommandQueue::SetName(LPCWSTR Name) {
    TRACE("D3D11CommandQueue::SetName %ls", Name);

    // Convert wide string to narrow string for D3D11 debug name
    char name[1024];
    wcstombs(name, Name, sizeof(name));

    // Set debug name on the immediate context
    return m_immediateContext->SetPrivateData(WKPDID_D3DDebugObjectName,
                                              strlen(name), name);
}

// ID3D12DeviceChild methods
HRESULT STDMETHODCALLTYPE D3D11CommandQueue::GetDevice(REFIID riid,
                                                       void** ppvDevice) {
    TRACE("D3D11CommandQueue::GetDevice %s, %p", debugstr_guid(&riid).c_str(),
          ppvDevice);
    return m_device->QueryInterface(riid, ppvDevice);
}

// ID3D12CommandQueue methods
void STDMETHODCALLTYPE D3D11CommandQueue::UpdateTileMappings(
    ID3D12Resource* pResource, UINT NumResourceRegions,
    const D3D12_TILED_RESOURCE_COORDINATE* pResourceRegionStartCoordinates,
    const D3D12_TILE_REGION_SIZE* pResourceRegionSizes, ID3D12Heap* pHeap,
    UINT NumRanges, const D3D12_TILE_RANGE_FLAGS* pRangeFlags,
    const UINT* pHeapRangeStartOffsets, const UINT* pRangeTileCounts,
    D3D12_TILE_MAPPING_FLAGS Flags) {
    TRACE(
        "D3D11CommandQueue::UpdateTileMappings %p, %u, %p, %p, %p, %u, %p, %p, "
        "%p, %d",
        pResource, NumResourceRegions, pResourceRegionStartCoordinates,
        pResourceRegionSizes, pHeap, NumRanges, pRangeFlags,
        pHeapRangeStartOffsets, pRangeTileCounts, Flags);
    // D3D11 doesn't support tiled resources in the same way
    FIXME("Tiled resource mapping not implemented.");
}

void STDMETHODCALLTYPE D3D11CommandQueue::CopyTileMappings(
    ID3D12Resource* pDstResource,
    const D3D12_TILED_RESOURCE_COORDINATE* pDstRegionStartCoordinate,
    ID3D12Resource* pSrcResource,
    const D3D12_TILED_RESOURCE_COORDINATE* pSrcRegionStartCoordinate,
    const D3D12_TILE_REGION_SIZE* pRegionSize, D3D12_TILE_MAPPING_FLAGS Flags) {
    TRACE("D3D11CommandQueue::CopyTileMappings %p, %p, %p, %p, %p, %d",
          pDstResource, pDstRegionStartCoordinate, pSrcResource,
          pSrcRegionStartCoordinate, pRegionSize, Flags);
    // D3D11 doesn't support tiled resources in the same way
    FIXME("Tiled resource copy not implemented.");
}

void STDMETHODCALLTYPE D3D11CommandQueue::ExecuteCommandLists(
    UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists) {
    TRACE("D3D11CommandQueue::ExecuteCommandLists %u, %p", NumCommandLists,
          ppCommandLists);

    for (UINT i = 0; i < NumCommandLists; i++) {
        auto d3d11_list = static_cast<D3D11CommandList*>(ppCommandLists[i]);
        Microsoft::WRL::ComPtr<ID3D11CommandList> native_list;
        // Get the native D3D11 command list from our wrapper
        if (SUCCEEDED(d3d11_list->GetD3D11CommandList(&native_list))) {
            // Execute the command list on the immediate context
            m_immediateContext->ExecuteCommandList(native_list.Get(), FALSE);
        }
    }
}

void STDMETHODCALLTYPE D3D11CommandQueue::SetMarker(UINT Metadata,
                                                    const void* pData,
                                                    UINT Size) {
    TRACE("D3D11CommandQueue::SetMarker %u, %p, %u", Metadata, pData, Size);
    // TODO: Implement debug markers
    FIXME("Debug markers not implemented.");
}

void STDMETHODCALLTYPE D3D11CommandQueue::BeginEvent(UINT Metadata,
                                                     const void* pData,
                                                     UINT Size) {
    TRACE("D3D11CommandQueue::BeginEvent %u, %p, %u", Metadata, pData, Size);
    // TODO: Implement debug events
    FIXME("Debug events not implemented.");
}

void STDMETHODCALLTYPE D3D11CommandQueue::EndEvent() {
    TRACE("D3D11CommandQueue::EndEvent ");
    // TODO: Implement debug events
    FIXME("Debug events not implemented.");
}

HRESULT STDMETHODCALLTYPE D3D11CommandQueue::Signal(ID3D12Fence* pFence,
                                                    UINT64 Value) {
    TRACE("D3D11CommandQueue::Signal %p, %llu", pFence, Value);
    // TODO: Implement fence synchronization
    FIXME("Fence synchronization not implemented.");
    return S_OK;
}

HRESULT STDMETHODCALLTYPE D3D11CommandQueue::Wait(ID3D12Fence* pFence,
                                                  UINT64 Value) {
    TRACE("D3D11CommandQueue::Wait %p, %llu", pFence, Value);
    // TODO: Implement fence synchronization
    FIXME("Fence synchronization not implemented.");
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
D3D11CommandQueue::GetTimestampFrequency(UINT64* pFrequency) {
    TRACE("D3D11CommandQueue::GetTimestampFrequency %p", pFrequency);
    if (!pFrequency) return E_INVALIDARG;

    // D3D11 doesn't expose timestamp frequency directly
    // Use a reasonable default for now
    *pFrequency = 1000000000;  // 1GHz
    return S_OK;
}

HRESULT STDMETHODCALLTYPE D3D11CommandQueue::GetClockCalibration(
    UINT64* pGpuTimestamp, UINT64* pCpuTimestamp) {
    TRACE("D3D11CommandQueue::GetClockCalibration %p, %p", pGpuTimestamp,
          pCpuTimestamp);
    // D3D11 doesn't support timestamp queries on the command queue
    return E_NOTIMPL;
}

D3D12_COMMAND_QUEUE_DESC* STDMETHODCALLTYPE
D3D11CommandQueue::GetDesc(D3D12_COMMAND_QUEUE_DESC* pDesc) {
    TRACE("D3D11CommandQueue::GetDesc(%p)", pDesc);
    if (pDesc) {
        *pDesc = m_desc;
    }
    return pDesc;
}

// IWineDXGISwapChainFactory implementation
HRESULT STDMETHODCALLTYPE D3D11CommandQueue::create_swapchain(
    IDXGIFactory* factory, HWND window, const DXGI_SWAP_CHAIN_DESC1* desc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen_desc, IDXGIOutput* output,
    IDXGISwapChain1** swapchain) {
    
    TRACE("D3D11CommandQueue::create_swapchain(%p, %p, %p, %p, %p, %p)",
          factory, window, desc, fullscreen_desc, output, swapchain);

    return D3D11SwapChain::Create(m_device, factory, window, desc, 
                                 fullscreen_desc, output, swapchain);
}

}  // namespace dxiided
