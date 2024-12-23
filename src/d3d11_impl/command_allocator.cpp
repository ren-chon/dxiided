#include "d3d11_impl/command_allocator.hpp"

#include "common/debug.hpp"
#include "d3d11_impl/device.hpp"

namespace dxiided {

WrappedD3D12ToD3D11CommandAllocator::WrappedD3D12ToD3D11CommandAllocator(
    WrappedD3D12ToD3D11Device* device, D3D12_COMMAND_LIST_TYPE type)
    : m_device(device), m_type(type), m_refcount(1) {
    // Get the immediate context from the D3D11 device
    ID3D11Device* d3d11Device = device->GetD3D11Device();
    d3d11Device->GetImmediateContext(m_immediateContext.GetAddressOf());
}

HRESULT WrappedD3D12ToD3D11CommandAllocator::Create(
    WrappedD3D12ToD3D11Device* device, D3D12_COMMAND_LIST_TYPE type,
    REFIID riid, void** ppCommandAllocator) {
    TRACE("WrappedD3D12ToD3D11CommandAllocator::Create called");
    TRACE("  device: %p, type: %d, riid: %s, ppCommandAllocator: %p", device,
          type, debugstr_guid(&riid).c_str(), ppCommandAllocator);

    if (!device || !ppCommandAllocator) {
        return E_INVALIDARG;
    }

    if (riid != __uuidof(ID3D12CommandAllocator) && riid != IID_IUnknown) {
        return E_NOINTERFACE;
    }

    *ppCommandAllocator = new WrappedD3D12ToD3D11CommandAllocator(device, type);
    return S_OK;
}

// IUnknown methods
HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11CommandAllocator::QueryInterface(
    REFIID riid, void** ppvObject) {
    TRACE("WrappedD3D12ToD3D11CommandAllocator::QueryInterface called");
    TRACE("  riid: %s, ppvObject: %p", debugstr_guid(&riid).c_str(), ppvObject);

    if (!ppvObject) {
        return E_POINTER;
    }

    if (riid == __uuidof(ID3D12CommandAllocator) || riid == IID_IUnknown) {
        AddRef();
        *ppvObject = this;
        return S_OK;
    }
    WARN("Unknown interface %s.", debugstr_guid(&riid).c_str());
    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE WrappedD3D12ToD3D11CommandAllocator::AddRef() {
    TRACE("WrappedD3D12ToD3D11CommandAllocator::AddRef called");
    return ++m_refcount;
}

ULONG STDMETHODCALLTYPE WrappedD3D12ToD3D11CommandAllocator::Release() {
    TRACE("WrappedD3D12ToD3D11CommandAllocator::Release called");
    ULONG ref = --m_refcount;
    if (ref == 0) {
        delete this;
    }
    return ref;
}

// ID3D12Object methods
HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11CommandAllocator::GetPrivateData(
    REFGUID guid, UINT* pDataSize, void* pData) {
    FIXME("WrappedD3D12ToD3D11CommandAllocator::GetPrivateData called");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11CommandAllocator::SetPrivateData(
    REFGUID guid, UINT DataSize, const void* pData) {
    FIXME("WrappedD3D12ToD3D11CommandAllocator::SetPrivateData called");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11CommandAllocator::SetPrivateDataInterface(
    REFGUID guid, const IUnknown* pData) {
    FIXME("WrappedD3D12ToD3D11CommandAllocator::SetPrivateDataInterface called");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11CommandAllocator::SetName(LPCWSTR Name) {
    TRACE("WrappedD3D12ToD3D11CommandAllocator::SetName called, Name: %ls",
         Name);
    return S_OK;  // Names are just for debugging, so we can safely ignore
}

// ID3D12DeviceChild methods
HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11CommandAllocator::GetDevice(REFIID riid, void** ppvDevice) {
    TRACE("WrappedD3D12ToD3D11CommandAllocator::GetDevice %s, %p",
          riid, ppvDevice);
    return m_device->QueryInterface(riid, ppvDevice);
}

// ID3D12CommandAllocator methods
HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11CommandAllocator::Reset() {
    TRACE("WrappedD3D12ToD3D11CommandAllocator::Reset called");
    std::lock_guard<std::mutex> lock(m_mutex);

    // In D3D11, we don't need to explicitly reset command allocators
    // The immediate context handles command buffer management internally
    // We'll just flush the immediate context to ensure all commands are
    // processed
    m_immediateContext->Flush();

    return S_OK;
}

}  // namespace dxiided