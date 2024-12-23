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
    TRACE("WrappedD3D12ToD3D11Fence::AddRef");
    return m_ref_count.fetch_add(1, std::memory_order_relaxed) + 1;
}

ULONG STDMETHODCALLTYPE WrappedD3D12ToD3D11Fence::Release() {
    TRACE("WrappedD3D12ToD3D11Fence::Release");
    ULONG ref = m_ref_count.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (ref == 0) {
        // Clear any pending events before destruction
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingEvents.clear();
        delete this;
    }
    return ref;
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
    if (hEvent == nullptr || hEvent == INVALID_HANDLE_VALUE) {
        return E_INVALIDARG;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    
    // If the value has already been reached, signal immediately
    if (Value <= m_completed_value.load(std::memory_order_acquire)) {
        if (!SetEvent(hEvent)) {
            TRACE("  Failed to signal event %p for value %llu", hEvent, Value);
            return E_FAIL;
        }
        return S_OK;
    }

    // Otherwise store for later signaling
    m_pendingEvents[Value] = hEvent;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Fence::Signal(UINT64 Value) {
    try {
        TRACE("WrappedD3D12ToD3D11Fence::Signal %llu", Value);
        
        // Take mutex before any operations to prevent race conditions
        std::unique_lock<std::mutex> lock(m_mutex);
        
        // Update the fence value atomically
        m_value.store(Value, std::memory_order_release);
        
        // Update completed value if new value is higher
        UINT64 completedValue = m_completed_value.load(std::memory_order_acquire);
        if (Value > completedValue) {
            m_completed_value.store(Value, std::memory_order_release);
            
            // Process pending events that have been reached
            std::vector<std::pair<UINT64, HANDLE>> eventsToSignal;
            eventsToSignal.reserve(m_pendingEvents.size());
            
            auto it = m_pendingEvents.begin();
            while (it != m_pendingEvents.end()) {
                if (Value >= it->first) {
                    if (it->second != nullptr && it->second != INVALID_HANDLE_VALUE) {
                        eventsToSignal.emplace_back(it->first, it->second);
                    }
                    it = m_pendingEvents.erase(it);
                } else {
                    ++it;
                }
            }
            
            // Release lock before signaling events
            lock.unlock();
            
            // Signal events outside the lock
            for (const auto& [eventValue, event] : eventsToSignal) {
                TRACE("  Signaling event %p for value %llu", event, eventValue);
                if (!SetEvent(event)) {
                    WARN("Failed to signal event %p for value %llu: %lu", 
                         event, eventValue, GetLastError());
                }
            }
        }
        
        TRACE("  %zu pending events remaining", m_pendingEvents.size());
        return S_OK;
    } catch (const std::exception& e) {
        WARN("Exception in WrappedD3D12ToD3D11Fence::Signal: %s", e.what());
        return E_FAIL;
    } catch (...) {
        WARN("Unknown exception in WrappedD3D12ToD3D11Fence::Signal");
        return E_FAIL;
    }
}

}  // namespace dxiided