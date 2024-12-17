#pragma once

#include <d3d11.h>
#include <d3d12.h>
#include <wrl/client.h>

#include <atomic>
#include <mutex>

#include "common/debug.hpp"

namespace dxiided {

class D3D11Device;

class D3D11Fence final : public ID3D12Fence {
   public:
    static HRESULT Create(D3D11Device* device, UINT64 initial_value,
                         D3D12_FENCE_FLAGS flags, REFIID riid, void** fence);

    // IUnknown methods
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void** ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    // ID3D12Object methods
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT* pDataSize,
                                           void* pData) override;
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize,
                                           const void* pData) override;
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid,
                                                    const IUnknown* pData) override;
    HRESULT STDMETHODCALLTYPE SetName(LPCWSTR Name) override;

    // ID3D12DeviceChild methods
    HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void** ppvDevice) override;

    // ID3D12Fence methods
    UINT64 STDMETHODCALLTYPE GetCompletedValue() override;
    HRESULT STDMETHODCALLTYPE SetEventOnCompletion(UINT64 Value,
                                                 HANDLE hEvent) override;
    HRESULT STDMETHODCALLTYPE Signal(UINT64 Value) override;

   private:
    D3D11Fence(D3D11Device* device, UINT64 initial_value, D3D12_FENCE_FLAGS flags);

    D3D11Device* m_device;
    D3D12_FENCE_FLAGS m_flags;
    std::atomic<UINT64> m_value;
    std::atomic<UINT64> m_completed_value;
    std::mutex m_mutex;
    std::atomic<ULONG> m_refCount{1};
};

}  // namespace dxiided