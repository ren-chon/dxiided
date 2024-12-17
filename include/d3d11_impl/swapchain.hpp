#pragma once

#include <d3d11.h>
#include <d3d11_2.h>
#include <dxgi1_2.h>

#include <atomic>
#include <memory>

#include "common/debug.hpp"
#include "d3d11_impl/device.hpp"

namespace dxiided {

class D3D11Device;

class D3D11SwapChain final : public IDXGISwapChain1 {
   public:
    static HRESULT Create(D3D11Device* device,
                         IDXGIFactory2* factory,
                         HWND hwnd,
                         const DXGI_SWAP_CHAIN_DESC1* desc,
                         const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen_desc,
                         IDXGIOutput* restrict_to_output,
                         IDXGISwapChain1** swapchain);

    D3D11SwapChain(D3D11Device* device,
                   Microsoft::WRL::ComPtr<IDXGISwapChain1> swapchain);

    // IUnknown methods
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void** ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    // IDXGIObject methods
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID Name,
                                           UINT DataSize,
                                           const void* pData) override;
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID Name,
                                                    const IUnknown* pUnknown) override;
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID Name,
                                           UINT* pDataSize,
                                           void* pData) override;
    HRESULT STDMETHODCALLTYPE GetParent(REFIID riid,
                                      void** ppParent) override;

    // IDXGIDeviceSubObject methods
    HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid,
                                      void** ppDevice) override;

    // IDXGISwapChain methods
    HRESULT STDMETHODCALLTYPE Present(UINT SyncInterval,
                                    UINT Flags) override;
    HRESULT STDMETHODCALLTYPE GetBuffer(UINT Buffer,
                                      REFIID riid,
                                      void** ppSurface) override;
    HRESULT STDMETHODCALLTYPE SetFullscreenState(BOOL Fullscreen,
                                               IDXGIOutput* pTarget) override;
    HRESULT STDMETHODCALLTYPE GetFullscreenState(BOOL* pFullscreen,
                                               IDXGIOutput** ppTarget) override;
    HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SWAP_CHAIN_DESC* pDesc) override;
    HRESULT STDMETHODCALLTYPE ResizeBuffers(UINT BufferCount,
                                          UINT Width,
                                          UINT Height,
                                          DXGI_FORMAT NewFormat,
                                          UINT SwapChainFlags) override;
    HRESULT STDMETHODCALLTYPE ResizeTarget(const DXGI_MODE_DESC* pNewTargetParameters) override;
    HRESULT STDMETHODCALLTYPE GetContainingOutput(IDXGIOutput** ppOutput) override;
    HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS* pStats) override;
    HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT* pLastPresentCount) override;

    // IDXGISwapChain1 methods
    HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_SWAP_CHAIN_DESC1* pDesc) override;
    HRESULT STDMETHODCALLTYPE GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pDesc) override;
    HRESULT STDMETHODCALLTYPE GetHwnd(HWND* pHwnd) override;
    HRESULT STDMETHODCALLTYPE GetCoreWindow(REFIID refiid,
                                          void** ppUnk) override;
    HRESULT STDMETHODCALLTYPE Present1(UINT SyncInterval,
                                     UINT PresentFlags,
                                     const DXGI_PRESENT_PARAMETERS* pPresentParameters) override;
    BOOL STDMETHODCALLTYPE IsTemporaryMonoSupported() override;
    HRESULT STDMETHODCALLTYPE GetRestrictToOutput(IDXGIOutput** ppRestrictToOutput) override;
    HRESULT STDMETHODCALLTYPE SetBackgroundColor(const DXGI_RGBA* pColor) override;
    HRESULT STDMETHODCALLTYPE GetBackgroundColor(DXGI_RGBA* pColor) override;
    HRESULT STDMETHODCALLTYPE SetRotation(DXGI_MODE_ROTATION Rotation) override;
    HRESULT STDMETHODCALLTYPE GetRotation(DXGI_MODE_ROTATION* pRotation) override;

   private:
    D3D11Device* m_device;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> m_swapchain;
    LONG m_refCount{1};
};

}  // namespace dxiided