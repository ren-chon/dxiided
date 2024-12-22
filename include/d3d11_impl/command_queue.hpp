#pragma once

#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <initguid.h>

#include <memory>
#include <queue>
#include <vector>

#include "common/debug.hpp"

namespace dxiided {

class WrappedD3D12ToD3D11Device;
class WrappedD3D12ToD3D11CommandList;

// Define the IWineDXGISwapChainFactory interface GUID
DEFINE_GUID(IID_IWineDXGISwapChainFactory,
    0x53cb4ff0, 0xc25a, 0x4164,
    0xa8, 0x91, 0x0e, 0x83, 0xdb, 0x0a, 0x7a, 0xac);

// Define the interface
interface IWineDXGISwapChainFactory : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE create_swapchain(
        IDXGIFactory* factory,
        HWND window,
        const DXGI_SWAP_CHAIN_DESC1* desc,
        const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen_desc,
        IDXGIOutput* output,
        IDXGISwapChain1** swapchain) = 0;
};


class WrappedD3D12ToD3D11CommandQueue final : public ID3D12CommandQueue,
                               public IWineDXGISwapChainFactory
                               {
   public:
    static HRESULT Create(WrappedD3D12ToD3D11Device* device,
                          const D3D12_COMMAND_QUEUE_DESC* desc, REFIID riid,
                          void** command_queue);

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

    // ID3D12CommandQueue methods
    void STDMETHODCALLTYPE UpdateTileMappings(
        ID3D12Resource* pResource, UINT NumResourceRegions,
        const D3D12_TILED_RESOURCE_COORDINATE* pResourceRegionStartCoordinates,
        const D3D12_TILE_REGION_SIZE* pResourceRegionSizes, ID3D12Heap* pHeap,
        UINT NumRanges, const D3D12_TILE_RANGE_FLAGS* pRangeFlags,
        const UINT* pHeapRangeStartOffsets, const UINT* pRangeTileCounts,
        D3D12_TILE_MAPPING_FLAGS Flags) override;
    void STDMETHODCALLTYPE CopyTileMappings(
        ID3D12Resource* pDstResource,
        const D3D12_TILED_RESOURCE_COORDINATE* pDstRegionStartCoordinate,
        ID3D12Resource* pSrcResource,
        const D3D12_TILED_RESOURCE_COORDINATE* pSrcRegionStartCoordinate,
        const D3D12_TILE_REGION_SIZE* pRegionSize,
        D3D12_TILE_MAPPING_FLAGS Flags) override;
    void STDMETHODCALLTYPE
    ExecuteCommandLists(UINT NumCommandLists,
                        ID3D12CommandList* const* ppCommandLists) override;
    void STDMETHODCALLTYPE SetMarker(UINT Metadata, const void* pData,
                                     UINT Size) override;
    void STDMETHODCALLTYPE BeginEvent(UINT Metadata, const void* pData,
                                      UINT Size) override;
    void STDMETHODCALLTYPE EndEvent() override;
    HRESULT STDMETHODCALLTYPE Signal(ID3D12Fence* pFence,
                                     UINT64 Value) override;
    HRESULT STDMETHODCALLTYPE Wait(ID3D12Fence* pFence, UINT64 Value) override;
    HRESULT STDMETHODCALLTYPE
    GetTimestampFrequency(UINT64* pFrequency) override;
    HRESULT STDMETHODCALLTYPE
    GetClockCalibration(UINT64* pGpuTimestamp, UINT64* pCpuTimestamp) override;
    D3D12_COMMAND_QUEUE_DESC* STDMETHODCALLTYPE GetDesc(
        D3D12_COMMAND_QUEUE_DESC* pDesc) override;

    // IWineDXGISwapChainFactory methods
    HRESULT STDMETHODCALLTYPE create_swapchain(
        IDXGIFactory* factory,
        HWND window,
        const DXGI_SWAP_CHAIN_DESC1* desc,
        const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen_desc,
        IDXGIOutput* output,
        IDXGISwapChain1** swapchain) override;


   private:
    WrappedD3D12ToD3D11CommandQueue(WrappedD3D12ToD3D11Device* device,
                      const D3D12_COMMAND_QUEUE_DESC* desc);

    WrappedD3D12ToD3D11Device* const m_device;
    D3D12_COMMAND_QUEUE_DESC m_desc;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    LONG m_refCount = 1;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_immediateContext;

    // Queue of pending command lists
    std::queue<Microsoft::WRL::ComPtr<ID3D11CommandList>> m_pendingLists;
};

}  // namespace dxiided
