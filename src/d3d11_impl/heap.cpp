#include "d3d11_impl/heap.hpp"
#include "d3d11_impl/device.hpp"

namespace dxiided {

HRESULT D3D11Heap::Create(D3D11Device* device, const D3D12_HEAP_DESC& desc,
                         REFIID riid, void** ppvHeap) {
    if (!device || !ppvHeap) {
        return E_INVALIDARG;
    }

    auto* heap = new D3D11Heap(device, desc);
    if (!heap) {
        return E_OUTOFMEMORY;
    }

    return heap->QueryInterface(riid, ppvHeap);
}

D3D11Heap::D3D11Heap(D3D11Device* device, const D3D12_HEAP_DESC& desc)
    : m_device(device), m_desc(desc) {
    TRACE("D3D11Heap::D3D11Heap(%p, %p)\n", device, &desc);
}

// IUnknown methods
HRESULT STDMETHODCALLTYPE D3D11Heap::QueryInterface(REFIID riid, void** ppvObject) {
    TRACE("D3D11Heap::QueryInterface(%s, %p)\n", debugstr_guid(&riid).c_str(), ppvObject);

    if (!ppvObject) {
        return E_POINTER;
    }

    if (riid == __uuidof(IUnknown) ||
        riid == __uuidof(ID3D12Object) ||
        riid == __uuidof(ID3D12DeviceChild) ||
        riid == __uuidof(ID3D12Heap)) {
        *ppvObject = this;
        AddRef();
        return S_OK;
    }

    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE D3D11Heap::AddRef() {
    TRACE("D3D11Heap::AddRef()\n");
    return InterlockedIncrement(&m_refCount);
}

ULONG STDMETHODCALLTYPE D3D11Heap::Release() {
    TRACE("D3D11Heap::Release()\n");
    ULONG ref = InterlockedDecrement(&m_refCount);
    if (ref == 0) {
        delete this;
    }
    return ref;
}

// ID3D12Object methods
HRESULT STDMETHODCALLTYPE D3D11Heap::GetPrivateData(REFGUID guid, UINT* pDataSize, void* pData) {
    TRACE("D3D11Heap::GetPrivateData(%s, %p, %p)\n", debugstr_guid(&guid).c_str(), pDataSize, pData);
    FIXME("D3D11Heap::GetPrivateData not implemented");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Heap::SetPrivateData(REFGUID guid, UINT DataSize, const void* pData) {
    TRACE("D3D11Heap::SetPrivateData(%s, %u, %p)\n", debugstr_guid(&guid).c_str(), DataSize, pData);
    FIXME("D3D11Heap::SetPrivateData not implemented");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Heap::SetPrivateDataInterface(REFGUID guid, const IUnknown* pData) {
    TRACE("D3D11Heap::SetPrivateDataInterface(%s, %p)\n", debugstr_guid(&guid).c_str(), pData);
    FIXME("D3D11Heap::SetPrivateDataInterface not implemented");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Heap::SetName(LPCWSTR Name) {
    TRACE("D3D11Heap::SetName(%ls)\n", Name);
    FIXME("D3D11Heap::SetName not implemented");
    return E_NOTIMPL;
}

// ID3D12DeviceChild methods
HRESULT STDMETHODCALLTYPE D3D11Heap::GetDevice(REFIID riid, void** ppvDevice) {
    TRACE("D3D11Heap::GetDevice(%s, %p)\n", debugstr_guid(&riid).c_str(), ppvDevice);
    return m_device->QueryInterface(riid, ppvDevice);
}

// ID3D12Heap methods
D3D12_HEAP_DESC* STDMETHODCALLTYPE D3D11Heap::GetDesc(D3D12_HEAP_DESC* desc) {
    TRACE("D3D11Heap::GetDesc(%p)\n", desc);
    *desc = m_desc;
    return desc;
}

}  // namespace dxiided