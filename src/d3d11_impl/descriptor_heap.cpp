#include "d3d11_impl/descriptor_heap.hpp"

#include "d3d11_impl/device.hpp"

namespace dxiided {

HRESULT D3D11DescriptorHeap::Create(D3D11Device* device,
                                    const D3D12_DESCRIPTOR_HEAP_DESC* desc,
                                    REFIID riid, void** ppvHeap) {
    TRACE("D3D11DescriptorHeap::Create(%p, %p, %s, %p)", device, desc,
          debugstr_guid(&riid).c_str(), ppvHeap);

    if (!device || !desc || !ppvHeap) {
        return E_INVALIDARG;
    }

    Microsoft::WRL::ComPtr<D3D11DescriptorHeap> heap =
        new D3D11DescriptorHeap(device, desc);

    return heap.CopyTo(reinterpret_cast<ID3D12DescriptorHeap**>(ppvHeap));
}

D3D11DescriptorHeap::D3D11DescriptorHeap(D3D11Device* device,
                                         const D3D12_DESCRIPTOR_HEAP_DESC* desc)
    : m_device(device), m_desc(*desc) {
    TRACE("D3D11DescriptorHeap::D3D11DescriptorHeap(%p, %p)", device, desc);

    // Calculate storage size based on descriptor count and type
    size_t descriptorSize =
        m_device->GetDescriptorHandleIncrementSize(m_desc.Type);
    m_descriptorStorage.resize(descriptorSize * m_desc.NumDescriptors);

    // Initialize handles
    m_cpuHandle.ptr = reinterpret_cast<SIZE_T>(m_descriptorStorage.data());
    m_gpuHandle.ptr = m_desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                          ? reinterpret_cast<UINT64>(m_descriptorStorage.data())
                          : 0;
}

// IUnknown methods
HRESULT STDMETHODCALLTYPE
D3D11DescriptorHeap::QueryInterface(REFIID riid, void** ppvObject) {
    TRACE("D3D11DescriptorHeap::QueryInterface called: %s, %p",
          debugstr_guid(&riid).c_str(), ppvObject);

    if (!ppvObject) {
        return E_POINTER;
    }

    if (riid == __uuidof(ID3D12DescriptorHeap) ||
        riid == __uuidof(ID3D12DeviceChild) || riid == __uuidof(ID3D12Object) ||
        riid == __uuidof(IUnknown)) {
        AddRef();
        *ppvObject = this;
        return S_OK;
    }

    WARN("D3D11DescriptorHeap::QueryInterface: Unknown interface query %s",
         debugstr_guid(&riid).c_str());
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE D3D11DescriptorHeap::AddRef() {
    ULONG refCount = ++m_refCount;
    TRACE("D3D11DescriptorHeap::AddRef %p increasing refcount to %lu.", this,
          refCount);
    return refCount;
}

ULONG STDMETHODCALLTYPE D3D11DescriptorHeap::Release() {
    ULONG refCount = --m_refCount;
    TRACE("D3D11DescriptorHeap::Release %p decreasing refcount to %lu.", this,
          refCount);
    if (refCount == 0) {
        delete this;
    }
    return refCount;
}

// ID3D12Object methods
HRESULT STDMETHODCALLTYPE D3D11DescriptorHeap::GetPrivateData(REFGUID guid,
                                                              UINT* pDataSize,
                                                              void* pData) {
    TRACE("D3D11DescriptorHeap::GetPrivateData %s, %p, %p",
          debugstr_guid(&guid).c_str(), pDataSize, pData);
          FIXME("D3D11DescriptorHeap::GetPrivateData Not implemented");
    // Not implemented
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11DescriptorHeap::SetPrivateData(
    REFGUID guid, UINT DataSize, const void* pData) {
    TRACE("D3D11DescriptorHeap::SetPrivateData %s, %u, %p",
          debugstr_guid(&guid).c_str(), DataSize, pData);
          FIXME("D3D11DescriptorHeap::SetPrivateData Not implemented");
    // Not implemented
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11DescriptorHeap::SetPrivateDataInterface(
    REFGUID guid, const IUnknown* pData) {
    TRACE("D3D11DescriptorHeap::SetPrivateDataInterface %s, %p",
          debugstr_guid(&guid).c_str(), pData);
    FIXME("D3D11DescriptorHeap::SetPrivateDataInterface Not implemented");
    // Not implemented
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE D3D11DescriptorHeap::SetName(LPCWSTR Name) {
    TRACE("D3D11DescriptorHeap::SetName %ls", Name);
    return S_OK;
}

// ID3D12DeviceChild methods
HRESULT STDMETHODCALLTYPE D3D11DescriptorHeap::GetDevice(REFIID riid,
                                                         void** ppvDevice) {
    TRACE("D3D11DescriptorHeap::GetDevice %s, %p",
          debugstr_guid(&riid).c_str(), ppvDevice);
    return m_device->QueryInterface(riid, ppvDevice);
}

// ID3D12DescriptorHeap methods
D3D12_DESCRIPTOR_HEAP_DESC* STDMETHODCALLTYPE
D3D11DescriptorHeap::GetDesc(D3D12_DESCRIPTOR_HEAP_DESC* desc) {
    TRACE("D3D11DescriptorHeap::GetDesc %p", desc);
    *desc = m_desc;
    return desc;
}

D3D12_CPU_DESCRIPTOR_HANDLE* STDMETHODCALLTYPE
D3D11DescriptorHeap::GetCPUDescriptorHandleForHeapStart(
    D3D12_CPU_DESCRIPTOR_HANDLE* handle) {
    TRACE("D3D11DescriptorHeap::GetCPUDescriptorHandleForHeapStart called");
    *handle = m_cpuHandle;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE* STDMETHODCALLTYPE
D3D11DescriptorHeap::GetGPUDescriptorHandleForHeapStart(
    D3D12_GPU_DESCRIPTOR_HANDLE* handle) {
    TRACE("D3D11DescriptorHeap::GetGPUDescriptorHandleForHeapStart called");
    *handle = m_gpuHandle;
    return handle;
}

}  // namespace dxiided