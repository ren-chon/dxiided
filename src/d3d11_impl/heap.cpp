#include "d3d11_impl/heap.hpp"
#include "d3d11_impl/device.hpp"

namespace dxiided {

HRESULT WrappedD3D12ToD3D11Heap::Create(WrappedD3D12ToD3D11Device* device, const D3D12_HEAP_DESC& desc,
                         REFIID riid, void** ppvHeap) {
    if (!device || !ppvHeap) {
        return E_INVALIDARG;
    }

    Microsoft::WRL::ComPtr<WrappedD3D12ToD3D11Heap> heap(
        new WrappedD3D12ToD3D11Heap(device, desc));
    if (!heap) {
        return E_OUTOFMEMORY;
    }

    return heap->QueryInterface(riid, ppvHeap);
}

WrappedD3D12ToD3D11Heap::WrappedD3D12ToD3D11Heap(WrappedD3D12ToD3D11Device* device, const D3D12_HEAP_DESC& desc)
    : m_device(device), m_desc(desc) {
    TRACE("WrappedD3D12ToD3D11Heap::WrappedD3D12ToD3D11Heap(%p, %p)", device, &desc);
    TRACE(" SizeInBytes: %llu", desc.SizeInBytes);
    TRACE(" Properties.Type: %d", desc.Properties.Type);
    TRACE(" Properties.CPUPageProperty: %d", desc.Properties.CPUPageProperty);
    TRACE(" Properties.MemoryPoolPreference: %d", desc.Properties.MemoryPoolPreference);
    TRACE(" Alignment: %llu", desc.Alignment);
    TRACE(" Flags: %#x", desc.Flags);

    // Create a D3D11 buffer to back this heap
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.ByteWidth = static_cast<UINT>(desc.SizeInBytes);
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    bufferDesc.CPUAccessFlags = 0;
    bufferDesc.MiscFlags = 0;
    bufferDesc.StructureByteStride = 0;

    HRESULT hr = device->GetD3D11Device()->CreateBuffer(&bufferDesc, nullptr, &m_buffer);
    if (FAILED(hr)) {
        ERR("Failed to create buffer for heap, hr %#x", hr);
    }
}

// IUnknown methods
HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Heap::QueryInterface(REFIID riid, void** ppvObject) {
    TRACE("WrappedD3D12ToD3D11Heap::QueryInterface(%s, %p)", debugstr_guid(&riid).c_str(), ppvObject);

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

ULONG STDMETHODCALLTYPE WrappedD3D12ToD3D11Heap::AddRef() {
    TRACE("WrappedD3D12ToD3D11Heap::AddRef()");
    return InterlockedIncrement(&m_refCount);
}

ULONG STDMETHODCALLTYPE WrappedD3D12ToD3D11Heap::Release() {
    TRACE("WrappedD3D12ToD3D11Heap::Release()");
    ULONG ref = InterlockedDecrement(&m_refCount);
    if (ref == 0) {
        delete this;
    }
    return ref;
}

// ID3D12Object methods
HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Heap::GetPrivateData(REFGUID guid, UINT* pDataSize, void* pData) {
    TRACE("WrappedD3D12ToD3D11Heap::GetPrivateData(%s, %p, %p)", debugstr_guid(&guid).c_str(), pDataSize, pData);
    FIXME("WrappedD3D12ToD3D11Heap::GetPrivateData not implemented");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Heap::SetPrivateData(REFGUID guid, UINT DataSize, const void* pData) {
    TRACE("WrappedD3D12ToD3D11Heap::SetPrivateData(%s, %u, %p)", debugstr_guid(&guid).c_str(), DataSize, pData);
    FIXME("WrappedD3D12ToD3D11Heap::SetPrivateData not implemented");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Heap::SetPrivateDataInterface(REFGUID guid, const IUnknown* pData) {
    TRACE("WrappedD3D12ToD3D11Heap::SetPrivateDataInterface(%s, %p)", debugstr_guid(&guid).c_str(), pData);
    FIXME("WrappedD3D12ToD3D11Heap::SetPrivateDataInterface not implemented");
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Heap::SetName(LPCWSTR Name) {
    TRACE("WrappedD3D12ToD3D11Heap::SetName(%ls)", Name);
    FIXME("WrappedD3D12ToD3D11Heap::SetName not implemented");
    return E_NOTIMPL;
}

// ID3D12DeviceChild methods
HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11Heap::GetDevice(REFIID riid, void** ppvDevice) {
    TRACE("WrappedD3D12ToD3D11Heap::GetDevice(%s, %p)", debugstr_guid(&riid).c_str(), ppvDevice);
    return m_device->QueryInterface(riid, ppvDevice);
}

// ID3D12Heap methods
D3D12_HEAP_DESC* STDMETHODCALLTYPE WrappedD3D12ToD3D11Heap::GetDesc(D3D12_HEAP_DESC* desc) {
    TRACE("WrappedD3D12ToD3D11Heap::GetDesc(%p)", desc);
    *desc = m_desc;
    return desc;
}

}  // namespace dxiided