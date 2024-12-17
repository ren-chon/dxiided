#pragma once

#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <initguid.h>

#include <memory>
#include <queue>
#include <vector>

#include "common/debug.hpp"
#include "d3d11_impl/swapchain.hpp"

namespace dxiided {

class D3D11Device;
class D3D11CommandList;

// Wine/DXVK swapchain factory interface
DEFINE_GUID(IID_IWineDXGISwapChainFactory,
    0x53cb4ff0, 0xc25a, 0x4164, 0xa8, 0x91, 0x0e, 0x83, 0xdb, 0x0a, 0x7a, 0xac);

class IWineDXGISwapChainFactory : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE create_swapchain(
        IDXGIFactory* pFactory,
        HWND hWnd,
        const DXGI_SWAP_CHAIN_DESC1* pDesc,
        const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc,
        IDXGIOutput* pRestrictToOutput,
        IDXGISwapChain1** ppSwapChain) = 0;
};

class D3D11CommandQueue final : public ID3D12CommandQueue,
                              public IWineDXGISwapChainFactory {
   public:
    static HRESULT Create(D3D11Device* device,
                          const D3D12_COMMAND_QUEUE_DESC* desc, REFIID riid,
                          void** command_queue);

    // Custom methods for D3D11 compatibility
    HRESULT CreateSwapChain(HWND hwnd, DXGI_SWAP_CHAIN_DESC1* desc,
                           IDXGISwapChain1** ppSwapChain);

    // IWineDXGISwapChainFactory methods
    HRESULT STDMETHODCALLTYPE create_swapchain(
        IDXGIFactory* pFactory,
        HWND hWnd,
        const DXGI_SWAP_CHAIN_DESC1* pDesc,
        const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc,
        IDXGIOutput* pRestrictToOutput,
        IDXGISwapChain1** ppSwapChain) override;

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

   private:
    D3D11CommandQueue(D3D11Device* device,
                      const D3D12_COMMAND_QUEUE_DESC* desc);

    D3D11Device* m_device;
    D3D12_COMMAND_QUEUE_DESC m_desc;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> m_swapChain;
    LONG m_refCount = 1;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_immediateContext;

    // Queue of pending command lists
    std::queue<Microsoft::WRL::ComPtr<ID3D11CommandList>> m_pendingLists;
};

}  // namespace dxiided
