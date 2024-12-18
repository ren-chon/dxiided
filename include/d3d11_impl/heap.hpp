#pragma once

#include <d3d11.h>
#include <d3d12.h>
#include <wrl/client.h>

namespace dxiided {

class D3D11Device;

class D3D11Heap final : public ID3D12Heap {
public:
    static HRESULT Create(D3D11Device* device, const D3D12_HEAP_DESC& desc,
                         REFIID riid, void** ppvHeap);

    // IUnknown methods
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    // ID3D12Object methods
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT* pDataSize, void* pData) override;
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize, const void* pData) override;
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown* pData) override;
    HRESULT STDMETHODCALLTYPE SetName(LPCWSTR Name) override;

    // ID3D12DeviceChild methods
    HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void** ppvDevice) override;

    // ID3D12Heap methods
    D3D12_HEAP_DESC* STDMETHODCALLTYPE GetDesc(D3D12_HEAP_DESC* desc) override;

    // D3D11Heap methods
    ID3D11Buffer* GetD3D11Buffer() { return m_buffer.Get(); }

private:
    D3D11Heap(D3D11Device* device, const D3D12_HEAP_DESC& desc);

    D3D11Device* m_device;
    D3D12_HEAP_DESC m_desc;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_buffer;
    LONG m_refCount = 1;
};

}  // namespace dxiided