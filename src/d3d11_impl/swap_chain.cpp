#include "d3d11_impl/swap_chain.hpp"

#include "d3d11_impl/device.hpp"

namespace dxiided {

HRESULT D3D11SwapChain::Create(
    D3D11Device* device, IDXGIFactory* factory, HWND window,
    const DXGI_SWAP_CHAIN_DESC1* desc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen_desc, IDXGIOutput* output,
    IDXGISwapChain1** ppSwapChain) {
    if (!device || !factory || !window || !desc || !ppSwapChain) {
        ERR("Invalid parameters in create_swapchain\n");
        return E_INVALIDARG;
    }
    // Get window name
    char window_name[256] = {};
    GetWindowTextA(window, window_name, sizeof(window_name));
    TRACE(
        "Creating swap "
        "chain:\nWidth=%u\nHeight=%u\nFormat=%d\nBufferCount=%u\nWindow "
        "name:%s\n",
        desc->Width, desc->Height, desc->Format, desc->BufferCount,
        window_name);

    // Validate buffer count
    UINT buffer_count = desc->BufferCount < 1 ? 1 
                     : desc->BufferCount > 2 ? 2 
                     : desc->BufferCount;

    if (buffer_count != desc->BufferCount) {
        ERR("Adjusted buffer count from %u to %u\n", desc->BufferCount,
            buffer_count);
    }

    DXGI_SWAP_CHAIN_DESC swapchain_desc = {};
    swapchain_desc.BufferDesc.Width = desc->Width;
    swapchain_desc.BufferDesc.Height = desc->Height;
    swapchain_desc.BufferDesc.Format = desc->Format;
    swapchain_desc.BufferDesc.RefreshRate.Numerator = 60;
    swapchain_desc.BufferDesc.RefreshRate.Denominator = 1;
    swapchain_desc.SampleDesc.Count = 1;
    swapchain_desc.SampleDesc.Quality = 0;
    swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchain_desc.BufferCount = buffer_count;
    swapchain_desc.OutputWindow = window;
    swapchain_desc.Windowed = TRUE;  // We'll handle fullscreen transitions separately
    swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;
    swapchain_desc.Flags = desc->Flags;

    // Get the D3D11 device
    ID3D11Device* d3d11Device = device->GetD3D11Device();
    if (!d3d11Device) {
        ERR("Failed to get D3D11 device.\n");
        return E_FAIL;
    }

    TRACE("D3D11 device pointer: %p\n", d3d11Device);
    // Verify device is valid by trying to query interface
    Microsoft::WRL::ComPtr<IUnknown> unk;
    HRESULT hr = d3d11Device->QueryInterface(__uuidof(IUnknown), &unk);
    if (FAILED(hr)) {
        ERR("D3D11 device appears invalid - QueryInterface failed, hr %#x.\n",
            hr);
        return hr;
    }
    TRACE("D3D11 device validated successfully\n");

    // Log device details
    D3D_FEATURE_LEVEL featureLevel = d3d11Device->GetFeatureLevel();
    TRACE("D3D11 device feature level: %#x\n", featureLevel);

    // Verify factory
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    hr = d3d11Device->QueryInterface(__uuidof(IDXGIDevice), &dxgiDevice);
    if (FAILED(hr)) {
        ERR("Failed to get DXGI device, hr %#x.\n", hr);
        return hr;
    }
    TRACE("Got DXGI device interface\n");

    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(&adapter);
    if (FAILED(hr)) {
        ERR("Failed to get DXGI adapter, hr %#x.\n", hr);
        return hr;
    }
    TRACE("Got DXGI adapter\n");

    Microsoft::WRL::ComPtr<IDXGIFactory> deviceFactory;
    hr = adapter->GetParent(IID_PPV_ARGS(&deviceFactory));
    if (FAILED(hr)) {
        ERR("Failed to get DXGI factory from adapter, hr %#x.\n", hr);
        return hr;
    }
    TRACE("Got DXGI factory from device\n");

    // Create the swap chain
    TRACE("Create the swap chain using D3D11 device\n");
    TRACE("  Using factory: %p\n", factory);
    TRACE("  Using D3D11 device: %p\n", d3d11Device);

    TRACE("Creating swap chain with %u buffers\n", buffer_count);

    Microsoft::WRL::ComPtr<IDXGISwapChain> base_swapchain;
    hr = factory->CreateSwapChain(d3d11Device, &swapchain_desc, &base_swapchain);

    if (FAILED(hr)) {
        ERR("Failed to create swap chain, hr %#x\n", hr);
        return hr;
    }
    TRACE("Created base swap chain successfully\n");

    // Get IDXGISwapChain1 interface
    hr = base_swapchain.CopyTo(ppSwapChain);
    if (FAILED(hr)) {
        ERR("Failed to return swap chain interface, hr %#x\n", hr);
        return hr;
    }
    TRACE("Got IDXGISwapChain1 interface successfully\n");

    return S_OK;
}

HRESULT D3D11SwapChain::InitBackBuffers() {
    TRACE("D3D11SwapChain::InitBackBuffers\n");

    // Get swap chain description
    DXGI_SWAP_CHAIN_DESC desc;
    HRESULT hr = m_base_swapchain->GetDesc(&desc);
    if (FAILED(hr)) {
        ERR("Failed to get swap chain description, hr %#x.\n", hr);
        return hr;
    }

    TRACE(
        "Swap chain desc - BufferCount: %u, Width: %u, Height: %u, Format: "
        "%d\n",
        desc.BufferCount, desc.BufferDesc.Width, desc.BufferDesc.Height, desc.BufferDesc.Format);

    // Validate buffer count matches what we requested
    if (desc.BufferCount < 1 || desc.BufferCount > 2) {
        ERR("Unexpected buffer count %u from swap chain\n", desc.BufferCount);
        return E_UNEXPECTED;
    }

    m_buffer_count = desc.BufferCount;
    m_format = desc.BufferDesc.Format;
    m_width = desc.BufferDesc.Width;
    m_height = desc.BufferDesc.Height;

    // Clear existing back buffers
    ReleaseBackBuffers();

    // Reserve space to avoid reallocations
    m_backbuffers.reserve(desc.BufferCount);
    m_renderTargetViews.reserve(desc.BufferCount);

    // Create RTVs for all buffers
    for (UINT i = 0; i < desc.BufferCount; i++) {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> buffer;
        hr = m_base_swapchain->GetBuffer(
            i, __uuidof(ID3D11Texture2D),
            reinterpret_cast<void**>(buffer.GetAddressOf()));

        if (FAILED(hr)) {
            WARN(
                "Could not get back buffer %u, hr %#x - continuing with %u "
                "buffers\n",
                i, hr, static_cast<UINT>(m_backbuffers.size()));
            break;  // Stop trying to get more buffers but don't fail
        }

        D3D11_TEXTURE2D_DESC tex_desc;
        buffer->GetDesc(&tex_desc);
        TRACE(
            "Back buffer %u validated - Width: %u, Height: %u, Format: %d, "
            "ArraySize: %u\n",
            i, tex_desc.Width, tex_desc.Height, tex_desc.Format,
            tex_desc.ArraySize);

        // Create RTV description
        D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
        rtv_desc.Format = tex_desc.Format;
        rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtv_desc.Texture2D.MipSlice = 0;

        // Create render target view
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
        hr = m_device->CreateRenderTargetView(buffer.Get(), &rtv_desc, &rtv);
        if (FAILED(hr)) {
            ERR("Failed to create RTV for back buffer %u, hr %#x.\n", i, hr);
            return hr;  // Still fail if we can't create RTV
        }

        m_backbuffers.push_back(std::move(buffer));
        m_renderTargetViews.push_back(std::move(rtv));
        TRACE("Created back buffer %u with RTV\n", i);
    }

    // Update actual buffer count
    m_buffer_count = static_cast<UINT>(m_backbuffers.size());

    TRACE("Successfully initialized %u back buffers\n", m_buffer_count);
    return S_OK;
}

HRESULT D3D11SwapChain::GetBuffer(UINT Buffer, REFIID riid, void** ppSurface) {
    TRACE("D3D11SwapChain::GetBuffer %u, %s, %p\n", Buffer,
          debugstr_guid(&riid).c_str(), ppSurface);

    if (Buffer >= m_buffer_count) {
        ERR("Invalid buffer index %u (count: %u)\n", Buffer, m_buffer_count);
        return DXGI_ERROR_NOT_FOUND;
    }

    if (riid != __uuidof(ID3D11Texture2D) &&
        riid != __uuidof(ID3D11RenderTargetView)) {
        ERR("Unsupported interface %s\n", debugstr_guid(&riid).c_str());
        return E_NOINTERFACE;
    }

    if (!ppSurface) {
        return E_INVALIDARG;
    }

    if (riid == __uuidof(ID3D11Texture2D)) {
        ID3D11Texture2D* buffer = m_backbuffers[Buffer].Get();
        if (!buffer) {
            ERR("Back buffer %u not initialized\n", Buffer);
            return DXGI_ERROR_NOT_FOUND;
        }

        *ppSurface = buffer;
        buffer->AddRef();
    } else if (riid == __uuidof(ID3D11RenderTargetView)) {
        ID3D11RenderTargetView* rtv = m_renderTargetViews[Buffer].Get();
        if (!rtv) {
            ERR("RTV for back buffer %u not initialized\n", Buffer);
            return DXGI_ERROR_NOT_FOUND;
        }

        *ppSurface = rtv;
        rtv->AddRef();
    }

    return S_OK;
}

void D3D11SwapChain::ReleaseBackBuffers() {
    TRACE("D3D11SwapChain::ReleaseBackBuffers\n");
    m_backbuffers.clear();
    m_renderTargetViews.clear();
}

D3D11SwapChain::D3D11SwapChain(
    D3D11Device* device, Microsoft::WRL::ComPtr<IDXGISwapChain1> base_swapchain)
    : m_device(device), m_base_swapchain(std::move(base_swapchain)) {
    TRACE("D3D11SwapChain::D3D11SwapChain\n");

    // Initialize back buffers
    HRESULT hr = InitBackBuffers();
    if (FAILED(hr)) {
        ERR("Failed to initialize back buffers, hr %#x.\n", hr);
    }
}

D3D11SwapChain::~D3D11SwapChain() {
    TRACE("D3D11SwapChain::~D3D11SwapChain\n");
    ReleaseBackBuffers();
}

// IDXGIObject methods
HRESULT STDMETHODCALLTYPE D3D11SwapChain::SetPrivateData(REFGUID Name,
                                                         UINT DataSize,
                                                         const void* pData) {
    TRACE("D3D11SwapChain::SetPrivateData called");
    return m_base_swapchain->SetPrivateData(Name, DataSize, pData);
}

HRESULT STDMETHODCALLTYPE D3D11SwapChain::SetPrivateDataInterface(
    REFGUID Name, const IUnknown* pUnknown) {
    TRACE("D3D11SwapChain::SetPrivateDataInterface called");
    return m_base_swapchain->SetPrivateDataInterface(Name, pUnknown);
}

HRESULT STDMETHODCALLTYPE D3D11SwapChain::GetPrivateData(REFGUID Name,
                                                         UINT* pDataSize,
                                                         void* pData) {
    TRACE("D3D11SwapChain::GetPrivateData called");
    return m_base_swapchain->GetPrivateData(Name, pDataSize, pData);
}

HRESULT STDMETHODCALLTYPE D3D11SwapChain::GetParent(REFIID riid,
                                                    void** ppParent) {
    TRACE("D3D11SwapChain::GetParent called");
    return m_base_swapchain->GetParent(riid, ppParent);
}

// IDXGIDeviceSubObject methods
HRESULT STDMETHODCALLTYPE D3D11SwapChain::GetDevice(REFIID riid,
                                                    void** ppDevice) {
    TRACE("D3D11SwapChain::GetDevice called");
    return m_base_swapchain->GetDevice(riid, ppDevice);
}

// IDXGISwapChain methods
HRESULT STDMETHODCALLTYPE D3D11SwapChain::Present(UINT SyncInterval,
                                                  UINT Flags) {
    TRACE("D3D11SwapChain::Present called");
    return m_base_swapchain->Present(SyncInterval, Flags);
}

HRESULT STDMETHODCALLTYPE
D3D11SwapChain::SetFullscreenState(BOOL Fullscreen, IDXGIOutput* pTarget) {
    TRACE("D3D11SwapChain::SetFullscreenState called");
    return m_base_swapchain->SetFullscreenState(Fullscreen, pTarget);
}

HRESULT STDMETHODCALLTYPE
D3D11SwapChain::GetFullscreenState(BOOL* pFullscreen, IDXGIOutput** ppTarget) {
    TRACE("D3D11SwapChain::GetFullscreenState called");
    return m_base_swapchain->GetFullscreenState(pFullscreen, ppTarget);
}

HRESULT STDMETHODCALLTYPE D3D11SwapChain::GetDesc(DXGI_SWAP_CHAIN_DESC* pDesc) {
    TRACE("D3D11SwapChain::GetDesc called");
    return m_base_swapchain->GetDesc(pDesc);
}

HRESULT STDMETHODCALLTYPE D3D11SwapChain::ResizeBuffers(UINT BufferCount,
                                                        UINT Width, UINT Height,
                                                        DXGI_FORMAT NewFormat,
                                                        UINT SwapChainFlags) {
    TRACE("D3D11SwapChain::ResizeBuffers called");
    // Release existing back buffers before resize
    ReleaseBackBuffers();

    // Resize the underlying swap chain
    HRESULT hr = m_base_swapchain->ResizeBuffers(BufferCount, Width, Height,
                                                 NewFormat, SwapChainFlags);
    if (FAILED(hr)) {
        ERR("Failed to resize base swap chain buffers, hr %#x\n", hr);
        return hr;
    }

    // Re-initialize our back buffers
    return InitBackBuffers();
}

HRESULT STDMETHODCALLTYPE
D3D11SwapChain::ResizeTarget(const DXGI_MODE_DESC* pNewTargetParameters) {
    TRACE("D3D11SwapChain::ResizeTarget called");
    return m_base_swapchain->ResizeTarget(pNewTargetParameters);
}

HRESULT STDMETHODCALLTYPE
D3D11SwapChain::GetContainingOutput(IDXGIOutput** ppOutput) {
    TRACE("D3D11SwapChain::GetContainingOutput called");
    return m_base_swapchain->GetContainingOutput(ppOutput);
}

HRESULT STDMETHODCALLTYPE
D3D11SwapChain::GetFrameStatistics(DXGI_FRAME_STATISTICS* pStats) {
    TRACE("D3D11SwapChain::GetFrameStatistics called");
    return m_base_swapchain->GetFrameStatistics(pStats);
}

HRESULT STDMETHODCALLTYPE
D3D11SwapChain::GetLastPresentCount(UINT* pLastPresentCount) {
    TRACE("D3D11SwapChain::GetLastPresentCount called");
    return m_base_swapchain->GetLastPresentCount(pLastPresentCount);
}

// IDXGISwapChain1 methods
HRESULT STDMETHODCALLTYPE
D3D11SwapChain::GetDesc1(DXGI_SWAP_CHAIN_DESC1* pDesc) {
    TRACE("D3D11SwapChain::GetDesc1 called");
    return m_base_swapchain->GetDesc1(pDesc);
}

HRESULT STDMETHODCALLTYPE
D3D11SwapChain::GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pDesc) {
    TRACE("D3D11SwapChain::GetFullscreenDesc called");
    return m_base_swapchain->GetFullscreenDesc(pDesc);
}

HRESULT STDMETHODCALLTYPE D3D11SwapChain::GetHwnd(HWND* pHwnd) {
    TRACE("D3D11SwapChain::GetHwnd called");
    return m_base_swapchain->GetHwnd(pHwnd);
}

HRESULT STDMETHODCALLTYPE D3D11SwapChain::GetCoreWindow(REFIID refiid,
                                                        void** ppUnk) {
    TRACE("D3D11SwapChain::GetCoreWindow called");
    return m_base_swapchain->GetCoreWindow(refiid, ppUnk);
}

HRESULT STDMETHODCALLTYPE
D3D11SwapChain::Present1(UINT SyncInterval, UINT PresentFlags,
                         const DXGI_PRESENT_PARAMETERS* pPresentParameters) {
    TRACE("D3D11SwapChain::Present1 called");
    return m_base_swapchain->Present1(SyncInterval, PresentFlags,
                                      pPresentParameters);
}

BOOL STDMETHODCALLTYPE D3D11SwapChain::IsTemporaryMonoSupported() {
    TRACE("D3D11SwapChain::IsTemporaryMonoSupported called");
    return m_base_swapchain->IsTemporaryMonoSupported();
}

HRESULT STDMETHODCALLTYPE
D3D11SwapChain::GetRestrictToOutput(IDXGIOutput** ppRestrictToOutput) {
    TRACE("D3D11SwapChain::GetRestrictToOutput called");
    return m_base_swapchain->GetRestrictToOutput(ppRestrictToOutput);
}

HRESULT STDMETHODCALLTYPE
D3D11SwapChain::SetBackgroundColor(const DXGI_RGBA* pColor) {
    TRACE("D3D11SwapChain::SetBackgroundColor called");
    return m_base_swapchain->SetBackgroundColor(pColor);
}

HRESULT STDMETHODCALLTYPE
D3D11SwapChain::GetBackgroundColor(DXGI_RGBA* pColor) {
    TRACE("D3D11SwapChain::GetBackgroundColor called");
    return m_base_swapchain->GetBackgroundColor(pColor);
}

HRESULT STDMETHODCALLTYPE
D3D11SwapChain::SetRotation(DXGI_MODE_ROTATION Rotation) {
    TRACE("D3D11SwapChain::SetRotation called");
    return m_base_swapchain->SetRotation(Rotation);
}

HRESULT STDMETHODCALLTYPE
D3D11SwapChain::GetRotation(DXGI_MODE_ROTATION* pRotation) {
    TRACE("D3D11SwapChain::GetRotation called");
    return m_base_swapchain->GetRotation(pRotation);
}

// IUnknown methods
HRESULT STDMETHODCALLTYPE D3D11SwapChain::QueryInterface(REFIID riid,
                                                         void** ppvObject) {
    TRACE("D3D11SwapChain::QueryInterface called: %s, %p\n",
          debugstr_guid(&riid).c_str(), ppvObject);
    if (!ppvObject) return E_INVALIDARG;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(IDXGIObject) ||
        riid == __uuidof(IDXGIDeviceSubObject) ||
        riid == __uuidof(IDXGISwapChain) || riid == __uuidof(IDXGISwapChain1)) {
        *ppvObject = this;
        AddRef();
        return S_OK;
    }

    WARN("D3D11SwapChain::QueryInterface: Unknown interface query %s\n",
         debugstr_guid(&riid).c_str());
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE D3D11SwapChain::AddRef() {
    TRACE("D3D11SwapChain::AddRef called");
    return InterlockedIncrement(&m_refcount);
}

ULONG STDMETHODCALLTYPE D3D11SwapChain::Release() {
    TRACE("D3D11SwapChain::Release called");
    ULONG refcount = InterlockedDecrement(&m_refcount);
    if (refcount == 0) {
        delete this;
    }
    return refcount;
}
}  // namespace dxiided