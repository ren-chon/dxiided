#include "d3d11_impl/fence.hpp"

#include "d3d11_impl/device.hpp"

namespace dxiided {

HRESULT WrappedD3D12ToD3D11Fence::Create(WrappedD3D12ToD3D11Device* device, UINT64 initial_value,
                          D3D12_FENCE_FLAGS flags, REFIID riid, void** fence) {
    TRACE("WrappedD3D12ToD3D11Fence::Create(%p, %llu, %u, %s, %p)", device, initial_value, flags,
          debugstr_guid(&riid).c_str(), fence);

    if (!device || !fence) {
        return E_INVALIDARG;
    }

    Microsoft::WRL::ComPtr<WrappedD3D12ToD3D11Fence> d3d11_fence =
        new WrappedD3D12ToD3D11Fence(device, initial_value, flags);

    return d3d11_fence.CopyTo(reinterpret_cast<ID3D12Fence**>(fence));
}

WrappedD3D12ToD3D11Fence::WrappedD3D12ToD3D11Fence(WrappedD3D12ToD3D11Device* device, UINT64 initial_value,
                       D3D12_FENCE_FLAGS flags)
    : m_device(device),
      m_flags(flags),
      m_value(initial_value),
      m_completed_value(initial_value) {
    TRACE("WrappedD3D12ToD3D11Fence::WrappedD3D12ToD3D11Fence(%p, %llu, %u)", device, initial_value, flags);
}

// IUnknown methods
HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Fence::QueryInterface(REFIID riid,
                                                    void** ppvObject) {
    TRACE("WrappedD3D12ToD3D11Fence::QueryInterface called: %s, %p",
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

    WARN("WrappedD3D12ToD3D11Fence::QueryInterface: Unknown interface query %s",
         debugstr_guid(&riid).c_str());
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE WrappedD3D12ToD3D11Fence::AddRef() {
    ULONG refCount = ++m_refCount;
    TRACE("WrappedD3D12ToD3D11Fence::AddRef %p increasing refcount to %lu.", this, refCount);
    return refCount;
}

ULONG STDMETHODCALLTYPE WrappedD3D12ToD3D11Fence::Release() {
    ULONG refCount = --m_refCount;
    TRACE("WrappedD3D12ToD3D11Fence::Release %p decreasing refcount to %lu.", this, refCount);
    if (refCount == 0) {
        delete this;
    }
    return refCount;
}

// ID3D12Object methods
HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Fence::GetPrivateData(REFGUID guid,
                                                    UINT* pDataSize,
                                                    void* pData) {
    // Not implemented
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Fence::SetPrivateData(REFGUID guid, UINT DataSize,
                                                    const void* pData) {
    FIXME("WrappedD3D12ToD3D11Fence::SetPrivateData(%s, %u, %p)", debugstr_guid(&guid).c_str(), DataSize, pData);
    // Not implemented
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Fence::SetPrivateDataInterface(
    REFGUID guid, const IUnknown* pData) {
    FIXME("WrappedD3D12ToD3D11Fence::SetPrivateDataInterface not implemented");
    // Not implemented
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Fence::SetName(LPCWSTR Name) {
    TRACE("WrappedD3D12ToD3D11Fence::SetName %ls", Name);
    return S_OK;
}

// ID3D12DeviceChild methods
HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Fence::GetDevice(REFIID riid, void** ppvDevice) {
    TRACE("WrappedD3D12ToD3D11Fence::GetDevice %s, %p", debugstr_guid(&riid).c_str(), ppvDevice);
    return m_device->QueryInterface(riid, ppvDevice);
}

// ID3D12Fence methods
UINT64 STDMETHODCALLTYPE WrappedD3D12ToD3D11Fence::GetCompletedValue() {
    TRACE("WrappedD3D12ToD3D11Fence::GetCompletedValue");
    return m_completed_value.load();
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Fence::SetEventOnCompletion(UINT64 Value,
                                                          HANDLE hEvent) {
                                                            TRACE("WrappedD3D12ToD3D11Fence::SetEventOnCompletion %llu, %p", Value, hEvent);
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_completed_value >= Value) {
        if (hEvent) {
            SetEvent(hEvent);
        }
        return S_OK;
    }

    // Store event for later completion
    m_pendingEvents.push_back({Value, hEvent});
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Fence::Signal(UINT64 Value) {
    TRACE("WrappedD3D12ToD3D11Fence::Signal %llu", Value);
    std::lock_guard<std::mutex> lock(m_mutex);
    m_value = Value;
    m_completed_value = Value;
    
    // Signal any pending events that have reached their value
    auto it = m_pendingEvents.begin();
    while (it != m_pendingEvents.end()) {
        if (Value >= it->first) {
            if (it->second) {
                SetEvent(it->second);
            }
            it = m_pendingEvents.erase(it);
        } else {
            ++it;
        }
    }
    TRACE("  %u pending events remaining", m_pendingEvents.size());
    return S_OK;
}

}  // namespace dxiided