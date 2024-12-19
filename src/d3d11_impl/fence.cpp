#include "d3d11_impl/fence.hpp"

#include "d3d11_impl/device.hpp"

namespace dxiided {

HRESULT D3D11Fence::Create(D3D11Device* device, UINT64 initial_value,
                          D3D12_FENCE_FLAGS flags, REFIID riid, void** fence) {
    TRACE("D3D11Fence::Create(%p, %llu, %u, %s, %p)", device, initial_value, flags,
          debugstr_guid(&riid).c_str(), fence);

    if (!device || !fence) {
        return E_INVALIDARG;
    }

    Microsoft::WRL::ComPtr<D3D11Fence> d3d11_fence =
        new D3D11Fence(device, initial_value, flags);

    return d3d11_fence.CopyTo(reinterpret_cast<ID3D12Fence**>(fence));
}

D3D11Fence::D3D11Fence(D3D11Device* device, UINT64 initial_value,
                       D3D12_FENCE_FLAGS flags)
    : m_device(device),
      m_flags(flags),
      m_value(initial_value),
      m_completed_value(initial_value) {
    TRACE("D3D11Fence::D3D11Fence(%p, %llu, %u)", device, initial_value, flags);
}

// IUnknown methods
HRESULT STDMETHODCALLTYPE D3D11Fence::QueryInterface(REFIID riid,
                                                    void** ppvObject) {
    TRACE("D3D11Fence::QueryInterface called: %s, %p",
          debugstr_guid(&riid).c_str(), ppvObject);

    if (!ppvObject) {
        return E_POINTER;
    }

    if (riid == __uuidof(ID3D12Fence) || riid == __uuidof(ID3D12DeviceChild) ||
        riid == __uuidof(ID3D12Object) || riid == __uuidof(IUnknown)) {
        AddRef();
        *ppvObject = this;
        return S_OK;
    }

    WARN("D3D11Fence::QueryInterface: Unknown interface query %s",
         debugstr_guid(&riid).c_str());
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE D3D11Fence::AddRef() {
    ULONG refCount = ++m_refCount;
    TRACE("D3D11Fence::AddRef %p increasing refcount to %lu.", this, refCount);
    return refCount;
}

ULONG STDMETHODCALLTYPE D3D11Fence::Release() {
    ULONG refCount = --m_refCount;
    TRACE("D3D11Fence::Release %p decreasing refcount to %lu.", this, refCount);
    if (refCount == 0) {
        delete this;
    }
    return refCount;
}

// ID3D12Object methods
HRESULT STDMETHODCALLTYPE D3D11Fence::GetPrivateData(REFGUID guid,
                                                    UINT* pDataSize,
                                                    void* pData) {
    // Not implemented
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Fence::SetPrivateData(REFGUID guid, UINT DataSize,
                                                    const void* pData) {
    // Not implemented
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Fence::SetPrivateDataInterface(
    REFGUID guid, const IUnknown* pData) {
    // Not implemented
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11Fence::SetName(LPCWSTR Name) {
    TRACE("D3D11Fence::SetName %ls", Name);
    return S_OK;
}

// ID3D12DeviceChild methods
HRESULT STDMETHODCALLTYPE D3D11Fence::GetDevice(REFIID riid, void** ppvDevice) {
    return m_device->QueryInterface(riid, ppvDevice);
}

// ID3D12Fence methods
UINT64 STDMETHODCALLTYPE D3D11Fence::GetCompletedValue() {
    return m_completed_value.load();
}

HRESULT STDMETHODCALLTYPE D3D11Fence::SetEventOnCompletion(UINT64 Value,
                                                          HANDLE hEvent) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_completed_value >= Value) {
        SetEvent(hEvent);
        return S_OK;
    }

    // TODO: Store event and value for later completion
    return S_OK;
}

HRESULT STDMETHODCALLTYPE D3D11Fence::Signal(UINT64 Value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_value = Value;
    m_completed_value = Value;
    return S_OK;
}

}  // namespace dxiided