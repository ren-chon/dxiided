#pragma once

#include <d3d11.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <mutex>
#include <memory>
#include <vector>
#include <atomic>
#include "common/debug.hpp"

namespace dxiided {

class WrappedD3D12ToD3D11Device;

class WrappedD3D12ToD3D11CommandAllocator : public ID3D12CommandAllocator {
public:
    static HRESULT Create(WrappedD3D12ToD3D11Device* device,
                         D3D12_COMMAND_LIST_TYPE type,
                         REFIID riid,
                         void** ppCommandAllocator);

    virtual ~WrappedD3D12ToD3D11CommandAllocator() = default;

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

    // ID3D12CommandAllocator methods
    HRESULT STDMETHODCALLTYPE Reset() override;

private:
    WrappedD3D12ToD3D11CommandAllocator(WrappedD3D12ToD3D11Device* device,
                                       D3D12_COMMAND_LIST_TYPE type);
    
    WrappedD3D12ToD3D11Device* m_device;
    D3D12_COMMAND_LIST_TYPE m_type;
    std::atomic<ULONG> m_refcount;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_immediateContext;
    std::mutex m_mutex;
};

}  // namespace dxiided
