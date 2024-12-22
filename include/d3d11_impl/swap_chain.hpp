#pragma once

#include <d3d11.h>
#include <dxgi1_2.h>
#include <dxgi1_5.h>
#include <wrl/client.h>
#include <atomic>
#include <vector>

#include "common/debug.hpp"

namespace dxiided {

class WrappedD3D12ToD3D11Device;

class WrappedD3D12ToD3D11SwapChain final : public IDXGISwapChain4 {
public:
    static HRESULT Create(
        WrappedD3D12ToD3D11Device* device,
        IDXGIFactory* factory,
        HWND window,
        const DXGI_SWAP_CHAIN_DESC1* desc,
        const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen_desc,
        IDXGIOutput* output,
        IDXGISwapChain1** ppSwapChain);

    // IUnknown methods
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void** ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    // IDXGIObject methods
    HRESULT STDMETHODCALLTYPE SetPrivateData(
        REFGUID Name,
        UINT DataSize,
        const void* pData) override;
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
        REFGUID Name,
        const IUnknown* pUnknown) override;
    HRESULT STDMETHODCALLTYPE GetPrivateData(
        REFGUID Name,
        UINT* pDataSize,
        void* pData) override;
    HRESULT STDMETHODCALLTYPE GetParent(
        REFIID riid,
        void** ppParent) override;

    // IDXGIDeviceSubObject methods
    HRESULT STDMETHODCALLTYPE GetDevice(
        REFIID riid,
        void** ppDevice) override;

    // IDXGISwapChain methods
    HRESULT STDMETHODCALLTYPE Present(
        UINT SyncInterval,
        UINT Flags) override;
    HRESULT STDMETHODCALLTYPE GetBuffer(
        UINT Buffer,
        REFIID riid,
        void** ppSurface) override;
    HRESULT STDMETHODCALLTYPE SetFullscreenState(
        BOOL Fullscreen,
        IDXGIOutput* pTarget) override;
    HRESULT STDMETHODCALLTYPE GetFullscreenState(
        BOOL* pFullscreen,
        IDXGIOutput** ppTarget) override;
    HRESULT STDMETHODCALLTYPE GetDesc(
        DXGI_SWAP_CHAIN_DESC* pDesc) override;
    HRESULT STDMETHODCALLTYPE ResizeBuffers(
        UINT BufferCount,
        UINT Width,
        UINT Height,
        DXGI_FORMAT NewFormat,
        UINT SwapChainFlags) override;
    HRESULT STDMETHODCALLTYPE ResizeTarget(
        const DXGI_MODE_DESC* pNewTargetParameters) override;
    HRESULT STDMETHODCALLTYPE GetContainingOutput(
        IDXGIOutput** ppOutput) override;
    HRESULT STDMETHODCALLTYPE GetFrameStatistics(
        DXGI_FRAME_STATISTICS* pStats) override;
    HRESULT STDMETHODCALLTYPE GetLastPresentCount(
        UINT* pLastPresentCount) override;

    // IDXGISwapChain1 methods
    HRESULT STDMETHODCALLTYPE GetDesc1(
        DXGI_SWAP_CHAIN_DESC1* pDesc) override;
    HRESULT STDMETHODCALLTYPE GetFullscreenDesc(
        DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pDesc) override;
    HRESULT STDMETHODCALLTYPE GetHwnd(
        HWND* pHwnd) override;
    HRESULT STDMETHODCALLTYPE GetCoreWindow(
        REFIID refiid,
        void** ppUnk) override;
    HRESULT STDMETHODCALLTYPE Present1(
        UINT SyncInterval,
        UINT PresentFlags,
        const DXGI_PRESENT_PARAMETERS* pPresentParameters) override;
    BOOL STDMETHODCALLTYPE IsTemporaryMonoSupported() override;
    HRESULT STDMETHODCALLTYPE GetRestrictToOutput(
        IDXGIOutput** ppRestrictToOutput) override;
    HRESULT STDMETHODCALLTYPE SetBackgroundColor(
        const DXGI_RGBA* pColor) override;
    HRESULT STDMETHODCALLTYPE GetBackgroundColor(
        DXGI_RGBA* pColor) override;
    HRESULT STDMETHODCALLTYPE SetRotation(
        DXGI_MODE_ROTATION Rotation) override;
    HRESULT STDMETHODCALLTYPE GetRotation(
        DXGI_MODE_ROTATION* pRotation) override;

    // IDXGISwapChain2 methods
    HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT MaxLatency) override {
        TRACE("WrappedD3D12ToD3D11SwapChain::SetMaximumFrameLatency called");
        return m_base_swapchain->QueryInterface(__uuidof(IDXGISwapChain2), (void**)&swapchain2) == S_OK ?
            swapchain2->SetMaximumFrameLatency(MaxLatency) : E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT* pMaxLatency) override {
        TRACE("WrappedD3D12ToD3D11SwapChain::GetMaximumFrameLatency called");
        return m_base_swapchain->QueryInterface(__uuidof(IDXGISwapChain2), (void**)&swapchain2) == S_OK ?
            swapchain2->GetMaximumFrameLatency(pMaxLatency) : E_NOINTERFACE;
    }

    HANDLE STDMETHODCALLTYPE GetFrameLatencyWaitableObject() override {
        TRACE("WrappedD3D12ToD3D11SwapChain::GetFrameLatencyWaitableObject called");
        return m_base_swapchain->QueryInterface(__uuidof(IDXGISwapChain2), (void**)&swapchain2) == S_OK ?
            swapchain2->GetFrameLatencyWaitableObject() : nullptr;
    }

    HRESULT STDMETHODCALLTYPE SetMatrixTransform(const DXGI_MATRIX_3X2_F* pMatrix) override {
        TRACE("WrappedD3D12ToD3D11SwapChain::SetMatrixTransform called");
        return m_base_swapchain->QueryInterface(__uuidof(IDXGISwapChain2), (void**)&swapchain2) == S_OK ?
            swapchain2->SetMatrixTransform(pMatrix) : E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE GetMatrixTransform(DXGI_MATRIX_3X2_F* pMatrix) override {
        TRACE("WrappedD3D12ToD3D11SwapChain::GetMatrixTransform called");
        return m_base_swapchain->QueryInterface(__uuidof(IDXGISwapChain2), (void**)&swapchain2) == S_OK ?
            swapchain2->GetMatrixTransform(pMatrix) : E_NOINTERFACE;
    }

    // IDXGISwapChain3 methods
    HRESULT STDMETHODCALLTYPE GetSourceSize(UINT* pWidth, UINT* pHeight) override {
        TRACE("WrappedD3D12ToD3D11SwapChain::GetSourceSize called");
        return m_base_swapchain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&swapchain3) == S_OK ?
            swapchain3->GetSourceSize(pWidth, pHeight) : E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE SetSourceSize(UINT Width, UINT Height) override {
        TRACE("WrappedD3D12ToD3D11SwapChain::SetSourceSize called");
        return m_base_swapchain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&swapchain3) == S_OK ?
            swapchain3->SetSourceSize(Width, Height) : E_NOINTERFACE;
    }


    HRESULT STDMETHODCALLTYPE CheckColorSpaceSupport(DXGI_COLOR_SPACE_TYPE ColorSpace, UINT* pColorSpaceSupport) override {
        TRACE("WrappedD3D12ToD3D11SwapChain::CheckColorSpaceSupport called");
        return m_base_swapchain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&swapchain3) == S_OK ?
            swapchain3->CheckColorSpaceSupport(ColorSpace, pColorSpaceSupport) : E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE SetColorSpace1(DXGI_COLOR_SPACE_TYPE ColorSpace) override {
        TRACE("WrappedD3D12ToD3D11SwapChain::SetColorSpace1 called");
        TRACE("  ColorSpace: %d", ColorSpace);
        return m_base_swapchain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&swapchain3) == S_OK ?
            swapchain3->SetColorSpace1(ColorSpace) : E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE ResizeBuffers1(UINT BufferCount, UINT Width, UINT Height, 
                                           DXGI_FORMAT Format, UINT SwapChainFlags,
                                           const UINT* pCreationNodeMask,
                                           IUnknown* const* ppPresentQueue) override {
        TRACE("WrappedD3D12ToD3D11SwapChain::ResizeBuffers1 called");
        return m_base_swapchain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&swapchain3) == S_OK ?
            swapchain3->ResizeBuffers1(BufferCount, Width, Height, Format, SwapChainFlags,
                                     pCreationNodeMask, ppPresentQueue) : E_NOINTERFACE;
    }

    UINT STDMETHODCALLTYPE GetCurrentBackBufferIndex() override {
        TRACE("WrappedD3D12ToD3D11SwapChain::GetCurrentBackBufferIndex called");
        return m_base_swapchain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&swapchain3) == S_OK ?
            swapchain3->GetCurrentBackBufferIndex() : 0;
    }

    // IDXGISwapChain4 methods
    HRESULT STDMETHODCALLTYPE SetHDRMetaData(DXGI_HDR_METADATA_TYPE Type, UINT Size, void* pMetaData) override {
        TRACE("WrappedD3D12ToD3D11SwapChain::SetHDRMetaData called");
        TRACE("  Type: %d", Type);
        TRACE("  Size: %d", Size);
        return m_base_swapchain->QueryInterface(__uuidof(IDXGISwapChain4), (void**)&swapchain4) == S_OK ?
            swapchain4->SetHDRMetaData(Type, Size, pMetaData) : E_NOINTERFACE;
    }

private:
    WrappedD3D12ToD3D11SwapChain(
        WrappedD3D12ToD3D11Device* device,
        Microsoft::WRL::ComPtr<IDXGISwapChain1> base_swapchain);

    ~WrappedD3D12ToD3D11SwapChain();

    HRESULT InitBackBuffers();
    void ReleaseBackBuffers();

    WrappedD3D12ToD3D11Device* const m_device;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> m_base_swapchain;
    volatile ULONG m_refcount = 1;
    
    // Back buffer management
    std::vector<Microsoft::WRL::ComPtr<ID3D11Texture2D>> m_backbuffers;
    std::vector<Microsoft::WRL::ComPtr<ID3D11RenderTargetView>> m_renderTargetViews;
    UINT m_buffer_count = 0;
    DXGI_FORMAT m_format = DXGI_FORMAT_UNKNOWN;
    UINT m_width = 0;
    UINT m_height = 0;

    Microsoft::WRL::ComPtr<IDXGISwapChain2> swapchain2;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapchain3;
    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapchain4;
};

} // namespace dxiided
