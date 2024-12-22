#pragma once

#include <d3d11.h>
#include <d3d12.h>
#include <wrl/client.h>

#include <atomic>
#include <memory>
#include <vector>

#include "common/debug.hpp"

namespace dxiided {

class WrappedD3D12ToD3D11Device;

class WrappedD3D12ToD3D11DescriptorHeap final : public ID3D12DescriptorHeap {
public:
    static HRESULT Create(WrappedD3D12ToD3D11Device* device,
                         const D3D12_DESCRIPTOR_HEAP_DESC* desc,
                         REFIID riid, void** ppvHeap);

    WrappedD3D12ToD3D11DescriptorHeap(WrappedD3D12ToD3D11Device* device,
                        const D3D12_DESCRIPTOR_HEAP_DESC* desc);

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

    // ID3D12DescriptorHeap methods
    D3D12_DESCRIPTOR_HEAP_DESC* STDMETHODCALLTYPE GetDesc(
        D3D12_DESCRIPTOR_HEAP_DESC* desc) override;
    D3D12_CPU_DESCRIPTOR_HANDLE* STDMETHODCALLTYPE GetCPUDescriptorHandleForHeapStart(
        D3D12_CPU_DESCRIPTOR_HANDLE* handle) override;
    D3D12_GPU_DESCRIPTOR_HANDLE* STDMETHODCALLTYPE GetGPUDescriptorHandleForHeapStart(
        D3D12_GPU_DESCRIPTOR_HANDLE* handle) override;

private:
    WrappedD3D12ToD3D11Device* const m_device;
    D3D12_DESCRIPTOR_HEAP_DESC m_desc;
    std::atomic<ULONG> m_refCount{1};

    // Storage for descriptors
    std::vector<uint8_t> m_descriptorStorage;
    D3D12_CPU_DESCRIPTOR_HANDLE m_cpuHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE m_gpuHandle;
};

}  // namespace dxiided