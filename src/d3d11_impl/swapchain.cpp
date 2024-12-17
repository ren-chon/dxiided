#include "d3d11_impl/swapchain.hpp"

#include <d3d11.h>
#include <d3d11_2.h>
#include <dxgi1_2.h>

#include "d3d11_impl/device.hpp"

// Interface GUIDs
const GUID IID_IDXGISwapChain = {
    0x310d36a0,
    0xd2e7,
    0x4c0a,
    {0xaa, 0x04, 0x6a, 0x9d, 0x23, 0xb8, 0x88, 0x6a}};
const GUID IID_IDXGISwapChain1 = {
    0x790a45f7,
    0x0d42,
    0x4876,
    {0x98, 0x3a, 0x0a, 0x55, 0xcf, 0xe6, 0xf4, 0xaa}};

namespace dxiided {

HRESULT D3D11SwapChain::Create(
    D3D11Device* device, IDXGIFactory2* factory, HWND hwnd,
    const DXGI_SWAP_CHAIN_DESC1* desc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen_desc,
    IDXGIOutput* restrict_to_output, IDXGISwapChain1** swapchain) {
    TRACE("D3D11SwapChain::Create called");
    if (!device || !factory || !hwnd || !desc || !swapchain) {
        ERR("D3D11SwapChain::Create Invalid arguments.\n");
        return E_INVALIDARG;
    }

    Microsoft::WRL::ComPtr<IDXGISwapChain1> dxgi_swapchain;
    HRESULT hr = factory->CreateSwapChainForHwnd(
        device->GetD3D11Device(), hwnd, desc, fullscreen_desc,
        restrict_to_output, &dxgi_swapchain);

    if (FAILED(hr)) {
        ERR("Failed to create DXGI swapchain, hr %#x\n", hr);
        return hr;
    }
    TRACE("wrapping swapchain");
    auto d3d11_swapchain = new D3D11SwapChain(device, dxgi_swapchain);
    *swapchain = d3d11_swapchain;
    TRACE("Created D3D11SwapChain %p\n", d3d11_swapchain);
    return S_OK;
}

D3D11SwapChain::D3D11SwapChain(
    D3D11Device* device, Microsoft::WRL::ComPtr<IDXGISwapChain1> swapchain)
    : m_device(device), m_swapchain(swapchain), m_refCount(1) {}

// IUnknown methods
HRESULT STDMETHODCALLTYPE D3D11SwapChain::QueryInterface(REFIID riid,
                                                         void** ppvObject) {
    TRACE("D3D11SwapChain::QueryInterface(%s, %p)\n",
          debugstr_guid(&riid).c_str(), ppvObject);

    if (!ppvObject) {
        return E_POINTER;
    }

    *ppvObject = nullptr;

    // Direct interfaces we implement
    if (IsEqualGUID(riid, __uuidof(IUnknown)) ||
        IsEqualGUID(riid, __uuidof(IDXGIObject)) ||
        IsEqualGUID(riid, __uuidof(IDXGIDeviceSubObject)) ||
        IsEqualGUID(riid, __uuidof(IDXGISwapChain)) ||
        IsEqualGUID(riid, __uuidof(IDXGISwapChain1))) {
        *ppvObject = this;
        AddRef();
        TRACE("Returning interface %s\n", debugstr_guid(&riid).c_str());
        return S_OK;
    }

    // Try the underlying swapchain for other interfaces
    HRESULT hr = m_swapchain->QueryInterface(riid, ppvObject);
    if (SUCCEEDED(hr)) {
        TRACE("Forwarded interface %s to underlying swapchain\n",
              debugstr_guid(&riid).c_str());
    } else {
        WARN("Unknown interface %s.\n", debugstr_guid(&riid).c_str());
    }
    return hr;
}

ULONG STDMETHODCALLTYPE D3D11SwapChain::AddRef() {
    ULONG ref = InterlockedIncrement(&m_refCount);
    TRACE("D3D11SwapChain::AddRef %p increasing refcount to %u.\n", this, ref);
    return ref;
}

ULONG STDMETHODCALLTYPE D3D11SwapChain::Release() {
    ULONG ref = InterlockedDecrement(&m_refCount);
    TRACE("D3D11SwapChain::Release %p decreasing refcount to %u.\n", this, ref);

    if (ref == 0) {
        delete this;
    }

    return ref;
}

// IDXGIObject methods
HRESULT STDMETHODCALLTYPE D3D11SwapChain::SetPrivateData(REFGUID Name,
                                                         UINT DataSize,
                                                         const void* pData) {
    return m_swapchain->SetPrivateData(Name, DataSize, pData);
}

HRESULT STDMETHODCALLTYPE D3D11SwapChain::SetPrivateDataInterface(
    REFGUID Name, const IUnknown* pUnknown) {
    return m_swapchain->SetPrivateDataInterface(Name, pUnknown);
}

HRESULT STDMETHODCALLTYPE D3D11SwapChain::GetPrivateData(REFGUID Name,
                                                         UINT* pDataSize,
                                                         void* pData) {
    return m_swapchain->GetPrivateData(Name, pDataSize, pData);
}

HRESULT STDMETHODCALLTYPE D3D11SwapChain::GetParent(REFIID riid,
                                                    void** ppParent) {
    return m_swapchain->GetParent(riid, ppParent);
}

// IDXGIDeviceSubObject methods
HRESULT STDMETHODCALLTYPE D3D11SwapChain::GetDevice(REFIID riid,
                                                    void** ppDevice) {
    return m_device->QueryInterface(riid, ppDevice);
}

// IDXGISwapChain methods
HRESULT STDMETHODCALLTYPE D3D11SwapChain::Present(UINT SyncInterval,
                                                  UINT Flags) {
    TRACE("D3D11SwapChain::Present(%u, %#x)\n", SyncInterval, Flags);

    // Validate sync interval
    if (SyncInterval > 4) {
        ERR("Invalid sync interval %u\n", SyncInterval);
        return DXGI_ERROR_INVALID_CALL;
    }

    // Present using the underlying DXGI swapchain
    HRESULT hr = m_swapchain->Present(SyncInterval, Flags);
    if (FAILED(hr)) {
        ERR("Present failed with hr %#x\n", hr);
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE D3D11SwapChain::GetBuffer(UINT Buffer, REFIID riid,
                                                    void** ppSurface) {
    TRACE("D3D11SwapChain::GetBuffer(%u, %s, %p)\n", Buffer,
          debugstr_guid(&riid).c_str(), ppSurface);

    // Validate parameters
    if (!ppSurface) {
        ERR("Invalid pointer provided for ppSurface\n");
        return E_POINTER;
    }

    *ppSurface = nullptr;

    // Validate buffer index
    DXGI_SWAP_CHAIN_DESC1 desc;
    HRESULT hr = m_swapchain->GetDesc1(&desc);
    if (FAILED(hr)) {
        ERR("Failed to get swapchain description, hr %#x\n", hr);
        return hr;
    }

    if (Buffer >= desc.BufferCount) {
        ERR("Invalid buffer index %u, swapchain has %u buffers\n", Buffer, desc.BufferCount);
        return E_INVALIDARG;
    }

    // Get the underlying buffer from DXGI swapchain
    Microsoft::WRL::ComPtr<IUnknown> buffer;
    hr = m_swapchain->GetBuffer(Buffer, __uuidof(IUnknown), &buffer);
    if (FAILED(hr)) {
        ERR("Failed to get buffer %u from swapchain, hr %#x\n", Buffer, hr);
        return hr;
    }

    // Query for the requested interface
    hr = buffer->QueryInterface(riid, ppSurface);
    if (FAILED(hr)) {
        ERR("Failed to query interface %s for buffer %u, hr %#x\n", 
            debugstr_guid(&riid).c_str(), Buffer, hr);
        return hr;
    }

    TRACE("Successfully retrieved buffer %u with interface %s\n", 
          Buffer, debugstr_guid(&riid).c_str());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
D3D11SwapChain::SetFullscreenState(BOOL Fullscreen, IDXGIOutput* pTarget) {
    TRACE("D3D11SwapChain::SetFullscreenState(%d, %p)\n", Fullscreen, pTarget);
    return m_swapchain->SetFullscreenState(Fullscreen, pTarget);
}

HRESULT STDMETHODCALLTYPE
D3D11SwapChain::GetFullscreenState(BOOL* pFullscreen, IDXGIOutput** ppTarget) {
    TRACE("D3D11SwapChain::GetFullscreenState called");
    return m_swapchain->GetFullscreenState(pFullscreen, ppTarget);
}

HRESULT STDMETHODCALLTYPE D3D11SwapChain::GetDesc(DXGI_SWAP_CHAIN_DESC* pDesc) {
    TRACE("D3D11SwapChain::GetDesc called");
    return m_swapchain->GetDesc(pDesc);
}

HRESULT STDMETHODCALLTYPE D3D11SwapChain::ResizeBuffers(UINT BufferCount,
                                                        UINT Width, UINT Height,
                                                        DXGI_FORMAT NewFormat,
                                                        UINT SwapChainFlags) {
    TRACE("D3D11SwapChain::ResizeBuffers called");
    return m_swapchain->ResizeBuffers(BufferCount, Width, Height, NewFormat,
                                      SwapChainFlags);
}

HRESULT STDMETHODCALLTYPE
D3D11SwapChain::ResizeTarget(const DXGI_MODE_DESC* pNewTargetParameters) {
    TRACE("D3D11SwapChain::ResizeTarget called");
    return m_swapchain->ResizeTarget(pNewTargetParameters);
}

HRESULT STDMETHODCALLTYPE
D3D11SwapChain::GetContainingOutput(IDXGIOutput** ppOutput) {
    TRACE("D3D11SwapChain::GetContainingOutput called");
    return m_swapchain->GetContainingOutput(ppOutput);
}

HRESULT STDMETHODCALLTYPE
D3D11SwapChain::GetFrameStatistics(DXGI_FRAME_STATISTICS* pStats) {
    TRACE("D3D11SwapChain::GetFrameStatistics called");
    return m_swapchain->GetFrameStatistics(pStats);
}

HRESULT STDMETHODCALLTYPE
D3D11SwapChain::GetLastPresentCount(UINT* pLastPresentCount) {
    TRACE("D3D11SwapChain::GetLastPresentCount called");
    return m_swapchain->GetLastPresentCount(pLastPresentCount);
}

// IDXGISwapChain1 methods
HRESULT STDMETHODCALLTYPE
D3D11SwapChain::GetDesc1(DXGI_SWAP_CHAIN_DESC1* pDesc) {
    TRACE("D3D11SwapChain::GetDesc1 called");
    return m_swapchain->GetDesc1(pDesc);
}

HRESULT STDMETHODCALLTYPE
D3D11SwapChain::GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pDesc) {
    TRACE("D3D11SwapChain::GetFullscreenDesc called");
    return m_swapchain->GetFullscreenDesc(pDesc);
}

HRESULT STDMETHODCALLTYPE D3D11SwapChain::GetHwnd(HWND* pHwnd) {
    TRACE("D3D11SwapChain::GetHwnd called");
    if (pHwnd) {
        TCHAR windowTitle[256];
        if (GetWindowText(*pHwnd, windowTitle,
                          sizeof(windowTitle) / sizeof(TCHAR))) {
            TRACE("Window title: %s\n", windowTitle);
        }
    }
    return m_swapchain->GetHwnd(pHwnd);
}

HRESULT STDMETHODCALLTYPE D3D11SwapChain::GetCoreWindow(REFIID refiid,
                                                        void** ppUnk) {
    TRACE("D3D11SwapChain::GetCoreWindow called");
    return m_swapchain->GetCoreWindow(refiid, ppUnk);
}

HRESULT STDMETHODCALLTYPE
D3D11SwapChain::Present1(UINT SyncInterval, UINT PresentFlags,
                         const DXGI_PRESENT_PARAMETERS* pPresentParameters) {
    TRACE("D3D11SwapChain::Present1(%u, %#x, %p)\n", SyncInterval, PresentFlags,
          pPresentParameters);

    // Validate sync interval
    if (SyncInterval > 4) {
        ERR("Invalid sync interval %u\n", SyncInterval);
        return DXGI_ERROR_INVALID_CALL;
    }

    // Validate present parameters
    if (pPresentParameters == nullptr) {
        ERR("Present parameters cannot be null\n");
        return E_INVALIDARG;
    }

    // Present using the underlying DXGI swapchain
    HRESULT hr =
        m_swapchain->Present1(SyncInterval, PresentFlags, pPresentParameters);
    if (FAILED(hr)) {
        ERR("Present1 failed with hr %#x\n", hr);
    }
    return hr;
}

BOOL STDMETHODCALLTYPE D3D11SwapChain::IsTemporaryMonoSupported() {
    TRACE("D3D11SwapChain::IsTemporaryMonoSupported called");
    return m_swapchain->IsTemporaryMonoSupported();
}

HRESULT STDMETHODCALLTYPE
D3D11SwapChain::GetRestrictToOutput(IDXGIOutput** ppRestrictToOutput) {
    return m_swapchain->GetRestrictToOutput(ppRestrictToOutput);
}

HRESULT STDMETHODCALLTYPE
D3D11SwapChain::SetBackgroundColor(const DXGI_RGBA* pColor) {
    TRACE("D3D11SwapChain::SetBackgroundColor called");
    return m_swapchain->SetBackgroundColor(pColor);
}

HRESULT STDMETHODCALLTYPE
D3D11SwapChain::GetBackgroundColor(DXGI_RGBA* pColor) {
    TRACE("D3D11SwapChain::GetBackgroundColor called");
    return m_swapchain->GetBackgroundColor(pColor);
}

HRESULT STDMETHODCALLTYPE
D3D11SwapChain::SetRotation(DXGI_MODE_ROTATION Rotation) {
    TRACE("D3D11SwapChain::SetRotation called");
    return m_swapchain->SetRotation(Rotation);
}

HRESULT STDMETHODCALLTYPE
D3D11SwapChain::GetRotation(DXGI_MODE_ROTATION* pRotation) {
    TRACE("D3D11SwapChain::GetRotation called");
    return m_swapchain->GetRotation(pRotation);
}

}  // namespace dxiided