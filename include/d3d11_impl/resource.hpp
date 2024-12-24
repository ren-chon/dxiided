#pragma once

#include <d3d11.h>
#include <d3d12.h>
#include <wrl/client.h>

#include <memory>
#include <vector>

#include "common/debug.hpp"

namespace dxiided {

class WrappedD3D12ToD3D11Device;

class WrappedD3D12ToD3D11Resource final : public ID3D12Resource {
   public:
    static HRESULT Create(WrappedD3D12ToD3D11Device* device,
                          const D3D12_HEAP_PROPERTIES* pHeapProperties,
                          D3D12_HEAP_FLAGS HeapFlags,
                          const D3D12_RESOURCE_DESC* pDesc,
                          D3D12_RESOURCE_STATES InitialState,
                          const D3D12_CLEAR_VALUE* pOptimizedClearValue,
                          REFIID riid, void** ppvResource);

    // Create a WrappedD3D12ToD3D11Resource wrapper around an existing D3D11 resource
    static HRESULT Create(WrappedD3D12ToD3D11Device* device,
                         ID3D11Resource* resource,
                         const D3D12_RESOURCE_DESC* pDesc,
                         D3D12_RESOURCE_STATES InitialState,
                         REFIID riid, void** ppvResource);

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
    HRESULT STDMETHODCALLTYPE
    SetPrivateDataInterface(REFGUID guid, const IUnknown* pData) override;
    HRESULT STDMETHODCALLTYPE SetName(LPCWSTR Name) override;

    // ID3D12DeviceChild methods
    HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void** ppvDevice) override;

    // ID3D12Resource methods
    HRESULT STDMETHODCALLTYPE Map(UINT Subresource,
                                  const D3D12_RANGE* pReadRange,
                                  void** ppData) override;
    void STDMETHODCALLTYPE Unmap(UINT Subresource,
                                 const D3D12_RANGE* pWrittenRange) override;
    D3D12_RESOURCE_DESC* STDMETHODCALLTYPE GetDesc(D3D12_RESOURCE_DESC* pDesc) override;
    D3D12_GPU_VIRTUAL_ADDRESS STDMETHODCALLTYPE GetGPUVirtualAddress() override;
    HRESULT STDMETHODCALLTYPE WriteToSubresource(UINT DstSubresource,
                                                 const D3D12_BOX* pDstBox,
                                                 const void* pSrcData,
                                                 UINT SrcRowPitch,
                                                 UINT SrcDepthPitch) override;
    HRESULT STDMETHODCALLTYPE
    ReadFromSubresource(void* pDstData, UINT DstRowPitch, UINT DstDepthPitch,
                        UINT SrcSubresource, const D3D12_BOX* pSrcBox) override;
    HRESULT STDMETHODCALLTYPE
    GetHeapProperties(D3D12_HEAP_PROPERTIES* pHeapProperties,
                      D3D12_HEAP_FLAGS* pHeapFlags) override;

    // Resource state tracking
    D3D12_RESOURCE_STATES GetCurrentState() const { return m_currentState; }
    void TransitionTo(ID3D11DeviceContext* context,
                      D3D12_RESOURCE_STATES newState);
    void UAVBarrier(ID3D11DeviceContext* context);
    void AliasingBarrier(ID3D11DeviceContext* context,
                         WrappedD3D12ToD3D11Resource* pResourceAfter);

    // Helper methods
    ID3D11Resource* GetD3D11Resource() const { return m_resource.Get(); }
    static UINT GetMiscFlags(const D3D12_RESOURCE_DESC* pDesc);
    void StoreInDeviceMap();
    
    // Format handling methods
    DXGI_FORMAT GetFormat() const { return m_format; }
    void SetFormat(DXGI_FORMAT format) { m_format = format; }
    UINT GetD3D11CPUAccessFlags(const D3D12_HEAP_PROPERTIES* pHeapProperties);
 private:
    WrappedD3D12ToD3D11Resource(WrappedD3D12ToD3D11Device* device,
                                const D3D12_HEAP_PROPERTIES* pHeapProperties,
                                D3D12_HEAP_FLAGS HeapFlags,
                                const D3D12_RESOURCE_DESC* pDesc,
                                D3D12_RESOURCE_STATES InitialState);

    // Constructor for wrapping existing D3D11 resource
    WrappedD3D12ToD3D11Resource(WrappedD3D12ToD3D11Device* device,
                                ID3D11Resource* resource,
                                const D3D12_RESOURCE_DESC* pDesc,
                                D3D12_RESOURCE_STATES InitialState);
    ~WrappedD3D12ToD3D11Resource();

    static D3D11_BIND_FLAG GetD3D11BindFlags(const D3D12_RESOURCE_DESC* pDesc);
    static DXGI_FORMAT GetViewFormat(DXGI_FORMAT format);
    static D3D11_USAGE GetD3D11Usage(
        const D3D12_HEAP_PROPERTIES* pHeapProperties);

    WrappedD3D12ToD3D11Device* m_device;
    Microsoft::WRL::ComPtr<ID3D11Resource> m_resource;
    D3D12_RESOURCE_DESC m_desc;
    D3D12_HEAP_PROPERTIES m_heapProperties;
    D3D12_HEAP_FLAGS m_heapFlags;
    D3D12_GPU_VIRTUAL_ADDRESS m_gpuAddress{0};
    LONG m_refCount{1};
    D3D12_RESOURCE_STATES m_currentState{D3D12_RESOURCE_STATE_COMMON};
    D3D12_RESOURCE_STATES m_state;  // Add state member
    bool m_isUAV{false};
    DXGI_FORMAT m_format{DXGI_FORMAT_UNKNOWN};  // Add format member
};

}  // namespace dxiided
