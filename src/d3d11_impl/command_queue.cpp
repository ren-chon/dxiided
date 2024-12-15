#include "d3d11_impl/command_queue.hpp"

#include "d3d11_impl/command_list.hpp"
#include "d3d11_impl/device.hpp"

namespace dxiided {

HRESULT D3D11CommandQueue::Create(D3D11Device* device,
                                  const D3D12_COMMAND_QUEUE_DESC* desc,
                                  REFIID riid, void** command_queue) {
    TRACE("%p, %p, %s, %p\n", device, desc, debugstr_guid(&riid).c_str(),
          command_queue);

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
    TRACE("%p, Type=%d, Priority=%d, Flags=%d\n", device, desc->Type,
          desc->Priority, desc->Flags);

    m_immediateContext =
        Microsoft::WRL::ComPtr<ID3D11DeviceContext>(device->GetD3D11Context());
}

// IUnknown methods
HRESULT STDMETHODCALLTYPE D3D11CommandQueue::QueryInterface(REFIID riid,
                                                            void** ppvObject) {
    TRACE("%s, %p\n", debugstr_guid(&riid).c_str(), ppvObject);

    if (!ppvObject) {
        return E_POINTER;
    }

    if (riid == __uuidof(ID3D12CommandQueue) || riid == __uuidof(IUnknown)) {
        AddRef();
        *ppvObject = this;
        return S_OK;
    }

    WARN("Unknown interface %s.\n", debugstr_guid(&riid).c_str());
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE D3D11CommandQueue::AddRef() {
    ULONG ref = InterlockedIncrement(&m_refCount);
    TRACE("%p increasing refcount to %u.\n", this, ref);
    return ref;
}

ULONG STDMETHODCALLTYPE D3D11CommandQueue::Release() {
    ULONG ref = InterlockedDecrement(&m_refCount);
    TRACE("%p decreasing refcount to %u.\n", this, ref);
    if (ref == 0) {
        delete this;
    }
    return ref;
}

// ID3D12Object methods
HRESULT STDMETHODCALLTYPE D3D11CommandQueue::GetPrivateData(REFGUID guid,
                                                            UINT* pDataSize,
                                                            void* pData) {
    TRACE("%s, %p, %p\n", debugstr_guid(&guid).c_str(), pDataSize, pData);
    return m_immediateContext->GetPrivateData(guid, pDataSize, pData);
}

HRESULT STDMETHODCALLTYPE D3D11CommandQueue::SetPrivateData(REFGUID guid,
                                                            UINT DataSize,
                                                            const void* pData) {
    TRACE("%s, %u, %p\n", debugstr_guid(&guid).c_str(), DataSize, pData);
    return m_immediateContext->SetPrivateData(guid, DataSize, pData);
}

HRESULT STDMETHODCALLTYPE D3D11CommandQueue::SetPrivateDataInterface(
    REFGUID guid, const IUnknown* pData) {
    TRACE("%s, %p\n", debugstr_guid(&guid).c_str(), pData);
    return m_immediateContext->SetPrivateDataInterface(guid, pData);
}

HRESULT STDMETHODCALLTYPE D3D11CommandQueue::SetName(LPCWSTR Name) {
    TRACE("%s\n", debugstr_w(Name).c_str());
    return m_immediateContext->SetPrivateData(
        WKPDID_D3DDebugObjectName,
        static_cast<UINT>((wcslen(Name) + 1) * sizeof(WCHAR)), Name);
}

// ID3D12DeviceChild methods
HRESULT STDMETHODCALLTYPE D3D11CommandQueue::GetDevice(REFIID riid,
                                                       void** ppvDevice) {
    TRACE("%s, %p\n", debugstr_guid(&riid).c_str(), ppvDevice);
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
    TRACE("%p, %u, %p, %p, %p, %u, %p, %p, %p, %d\n", pResource,
          NumResourceRegions, pResourceRegionStartCoordinates,
          pResourceRegionSizes, pHeap, NumRanges, pRangeFlags,
          pHeapRangeStartOffsets, pRangeTileCounts, Flags);
    // D3D11 doesn't support tiled resources in the same way
    FIXME("Tiled resource mapping not implemented.\n");
}

void STDMETHODCALLTYPE D3D11CommandQueue::CopyTileMappings(
    ID3D12Resource* pDstResource,
    const D3D12_TILED_RESOURCE_COORDINATE* pDstRegionStartCoordinate,
    ID3D12Resource* pSrcResource,
    const D3D12_TILED_RESOURCE_COORDINATE* pSrcRegionStartCoordinate,
    const D3D12_TILE_REGION_SIZE* pRegionSize, D3D12_TILE_MAPPING_FLAGS Flags) {
    TRACE("%p, %p, %p, %p, %p, %d\n", pDstResource, pDstRegionStartCoordinate,
          pSrcResource, pSrcRegionStartCoordinate, pRegionSize, Flags);
    // D3D11 doesn't support tiled resources in the same way
    FIXME("Tiled resource copy not implemented.\n");
}

void STDMETHODCALLTYPE D3D11CommandQueue::ExecuteCommandLists(
    UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists) {
    TRACE("%u, %p\n", NumCommandLists, ppCommandLists);

    for (UINT i = 0; i < NumCommandLists; i++) {
        auto d3d11_list = static_cast<D3D11CommandList*>(ppCommandLists[i]);
        Microsoft::WRL::ComPtr<ID3D11CommandList> native_list;
        // Execute the command list on the immediate context
        m_immediateContext->ExecuteCommandList(native_list.Get(), TRUE);
    }
}

void STDMETHODCALLTYPE D3D11CommandQueue::SetMarker(UINT Metadata,
                                                    const void* pData,
                                                    UINT Size) {
    TRACE("%u, %p, %u\n", Metadata, pData, Size);
    // TODO: Implement debug markers
    FIXME("Debug markers not implemented.\n");
}

void STDMETHODCALLTYPE D3D11CommandQueue::BeginEvent(UINT Metadata,
                                                     const void* pData,
                                                     UINT Size) {
    TRACE("%u, %p, %u\n", Metadata, pData, Size);
    // TODO: Implement debug events
    FIXME("Debug events not implemented.\n");
}

void STDMETHODCALLTYPE D3D11CommandQueue::EndEvent() {
    TRACE("\n");
    // TODO: Implement debug events
    FIXME("Debug events not implemented.\n");
}

HRESULT STDMETHODCALLTYPE D3D11CommandQueue::Signal(ID3D12Fence* pFence,
                                                    UINT64 Value) {
    TRACE("%p, %llu\n", pFence, Value);
    // TODO: Implement fence synchronization
    FIXME("Fence synchronization not implemented.\n");
    return S_OK;
}

HRESULT STDMETHODCALLTYPE D3D11CommandQueue::Wait(ID3D12Fence* pFence,
                                                  UINT64 Value) {
    TRACE("%p, %llu\n", pFence, Value);
    // TODO: Implement fence synchronization
    FIXME("Fence synchronization not implemented.\n");
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
D3D11CommandQueue::GetTimestampFrequency(UINT64* pFrequency) {
    TRACE("%p\n", pFrequency);
    if (!pFrequency) return E_INVALIDARG;

    // D3D11 doesn't expose timestamp frequency directly
    // Use a reasonable default for now
    *pFrequency = 1000000000;  // 1GHz
    return S_OK;
}

HRESULT STDMETHODCALLTYPE D3D11CommandQueue::GetClockCalibration(
    UINT64* pGpuTimestamp, UINT64* pCpuTimestamp) {
    TRACE("%p, %p\n", pGpuTimestamp, pCpuTimestamp);
    // D3D11 doesn't support timestamp queries on the command queue
    return E_NOTIMPL;
}

D3D12_COMMAND_QUEUE_DESC* STDMETHODCALLTYPE
D3D11CommandQueue::GetDesc(D3D12_COMMAND_QUEUE_DESC* desc) {
    TRACE("%p\n", desc);
    if (desc) {
        *desc = m_desc;
    }
    return &m_desc;
}

}  // namespace dxiided
