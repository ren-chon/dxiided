#include "d3d11_impl/swap_chain.hpp"

#include "d3d11_impl/device.hpp"
#include "d3d11_impl/resource.hpp"

namespace dxiided {

HRESULT D3D11SwapChain::Create(
    D3D11Device* device, IDXGIFactory* factory, HWND window,
    const DXGI_SWAP_CHAIN_DESC1* desc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen_desc, IDXGIOutput* output,
    IDXGISwapChain1** ppSwapChain) {
    if (!device || !factory || !window || !desc || !ppSwapChain) {
        ERR("Invalid parameters in create_swapchain");
        return E_INVALIDARG;
    }
    // Get window name
    char window_name[256] = {};
    GetWindowTextA(window, window_name, sizeof(window_name));
    TRACE("Creating swapchain:");
    TRACE(" Application name: %s", window_name);
    TRACE(" Width: %u", desc->Width);
    TRACE(" Height: %u", desc->Height);
    TRACE(" Format: %u", desc->Format);
    TRACE(" BufferCount: %u", desc->BufferCount);

    // Validate buffer count
    UINT buffer_count = desc->BufferCount;
    if (buffer_count < 2) {
        TRACE("Buffer count must be at least 2 for flip model, adjusting from %u to 2", buffer_count);
        buffer_count = 2;
    }
    // For flip model, we need to ensure proper resource states
    DXGI_SWAP_CHAIN_DESC1 modified_desc = *desc;

    // Keep original buffer count if valid
    if (buffer_count < 2) {
        TRACE("Buffer count must be at least 2 for flip model, adjusting from %u to 2", buffer_count);
        buffer_count = 2;
    }
    modified_desc.BufferCount = buffer_count;

    // Handle flip model requirements
    if (desc->SwapEffect == DXGI_SWAP_EFFECT_FLIP_DISCARD) {
        // For FLIP_DISCARD, preserve all original settings
        TRACE("Using original FLIP_DISCARD swap effect");
        modified_desc.SwapEffect = desc->SwapEffect;
        modified_desc.Flags = desc->Flags;
    } else {
        // For other modes, convert to FLIP_SEQUENTIAL
        TRACE("Converting swap effect from %d to DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL", desc->SwapEffect);
        modified_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        modified_desc.Flags = desc->Flags | DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    }

    // Always ensure proper buffer usage for flip model
    modified_desc.BufferUsage = desc->BufferUsage;
    if (!(modified_desc.BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT)) {
        TRACE("Adding required DXGI_USAGE_RENDER_TARGET_OUTPUT flag");
        modified_desc.BufferUsage |= DXGI_USAGE_RENDER_TARGET_OUTPUT;
    }

    // Handle depth format conversion
    if (modified_desc.Format == DXGI_FORMAT_D32_FLOAT) {
        TRACE("Converting depth format D32_FLOAT to R32_TYPELESS");
        modified_desc.Format = DXGI_FORMAT_R32_TYPELESS;
    }
    
    // Force single sample for flip model
    modified_desc.SampleDesc.Count = 1;
    modified_desc.SampleDesc.Quality = 0;

    // Log original descriptor
    TRACE("Original swap chain descriptor:");
    TRACE("  Width: %u", desc->Width);
    TRACE("  Height: %u", desc->Height);
    TRACE("  Format: %d", desc->Format);
    TRACE("  Stereo: %d", desc->Stereo);
    TRACE("  SampleDesc.Count: %u", desc->SampleDesc.Count);
    TRACE("  SampleDesc.Quality: %u", desc->SampleDesc.Quality);
    TRACE("  BufferUsage: %#x", desc->BufferUsage);
    TRACE("  BufferCount: %u", desc->BufferCount);
    TRACE("  Scaling: %d", desc->Scaling);
    TRACE("  SwapEffect: %d", desc->SwapEffect);
    TRACE("  AlphaMode: %d", desc->AlphaMode);
    TRACE("  Flags: %#x", desc->Flags);

    // Also log our modified descriptor
    TRACE("Modified swap chain descriptor:");
    TRACE("  Width: %u", modified_desc.Width);
    TRACE("  Height: %u", modified_desc.Height);
    TRACE("  Format: %d", modified_desc.Format);
    TRACE("  Stereo: %d", modified_desc.Stereo);
    TRACE("  SampleDesc.Count: %u", modified_desc.SampleDesc.Count);
    TRACE("  SampleDesc.Quality: %u", modified_desc.SampleDesc.Quality);
    TRACE("  BufferUsage: %#x", modified_desc.BufferUsage);
    TRACE("  BufferCount: %u", modified_desc.BufferCount);
    TRACE("  Scaling: %d", modified_desc.Scaling);
    TRACE("  SwapEffect: %d", modified_desc.SwapEffect);
    TRACE("  AlphaMode: %d", modified_desc.AlphaMode);
    TRACE("  Flags: %#x", modified_desc.Flags);
    // Create the swap chain
    Microsoft::WRL::ComPtr<IDXGISwapChain1> base_swapchain;

    // Get IDXGIFactory2 interface
    Microsoft::WRL::ComPtr<IDXGIFactory2> factory2;
    HRESULT hr = factory->QueryInterface(__uuidof(IDXGIFactory2), &factory2);
    if (FAILED(hr)) {
        ERR("Failed to get IDXGIFactory2 interface, hr %#x", hr);
        return hr;
    }

    hr = factory2->CreateSwapChainForHwnd(
        device->GetD3D11Device(), window, &modified_desc, fullscreen_desc, output,
        base_swapchain.GetAddressOf());

    if (FAILED(hr)) {
        ERR("Failed to create swap chain, hr %#x", hr);
        return hr;
    }
    TRACE("Created base swap chain successfully");

    // Get IDXGISwapChain1 interface
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapchain1;
    hr = base_swapchain.As(&swapchain1);
    if (FAILED(hr)) {
        ERR("Failed to get IDXGISwapChain1 interface, hr %#x", hr);
        return hr;
    }

    TRACE("Create our wrapper that handles D3D12 interfaces");
    auto* swapchain = new D3D11SwapChain(device, std::move(swapchain1));
    if (!swapchain) {
        ERR("Failed to allocate D3D11SwapChain wrapper");
        return E_OUTOFMEMORY;
    }

    *ppSwapChain = swapchain;
    TRACE("Created D3D11SwapChain wrapper successfully");

    return S_OK;
}


HRESULT D3D11SwapChain::InitBackBuffers() {
    TRACE("D3D11SwapChain::InitBackBuffers");

    // Get swap chain description
    DXGI_SWAP_CHAIN_DESC desc;
    HRESULT hr = m_base_swapchain->GetDesc(&desc);
    if (FAILED(hr)) {
        ERR("Failed to get swap chain description, hr %#x.", hr);
        return hr;
    }

    TRACE(
        "Swap chain desc - BufferCount: %u, Width: %u, Height: %u, Format: %d",
        desc.BufferCount, desc.BufferDesc.Width, desc.BufferDesc.Height,
        desc.BufferDesc.Format);

    // Store swap chain properties
    m_buffer_count = desc.BufferCount;
    m_format = desc.BufferDesc.Format;
    m_width = desc.BufferDesc.Width;
    m_height = desc.BufferDesc.Height;

    // Clear existing back buffers
    ReleaseBackBuffers();

    // Reserve space for buffers
    m_backbuffers.reserve(desc.BufferCount);
    m_renderTargetViews.reserve(desc.BufferCount);

    // Create D3D11 textures that we'll use as backing storage for D3D12 resources
    for (UINT i = 0; i < desc.BufferCount; i++) {
        TRACE("Creating D3D11 texture for buffer %u of %u", i, desc.BufferCount);

        // Create D3D11 texture that will back the D3D12 resource
        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = m_width;
        texDesc.Height = m_height;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = m_format;
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        texDesc.CPUAccessFlags = 0;
        texDesc.MiscFlags = 0;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        hr = m_device->CreateTexture2D(&texDesc, nullptr, &texture);
        if (FAILED(hr)) {
            ERR("Failed to create D3D11 texture for buffer %u, hr %#x", i, hr);
            return hr;
        }

        // Create render target view
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = texDesc.Format;
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;

        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
        hr = m_device->CreateRenderTargetView(texture.Get(), &rtvDesc, &rtv);
        if (FAILED(hr)) {
            ERR("Failed to create RTV for buffer %u, hr %#x", i, hr);
            return hr;
        }

        m_backbuffers.push_back(std::move(texture));
        m_renderTargetViews.push_back(std::move(rtv));
        TRACE("Created D3D11 back buffer %u with RTV", i);
    }

    return S_OK;
}

HRESULT D3D11SwapChain::GetBuffer(
    UINT Buffer,
    REFIID riid,
    void** ppSurface) {
    
    TRACE("D3D11SwapChain::GetBuffer %u, %s, %p", Buffer,
          debugstr_guid(&riid).c_str(), ppSurface);

    if (Buffer >= m_buffer_count || !ppSurface) {
        ERR("Invalid buffer index %u (buffer_count=%u) or null surface pointer",
            Buffer, m_buffer_count);
        return DXGI_ERROR_INVALID_CALL;
    }

    *ppSurface = nullptr;

    // The game will request a D3D12 resource, so we need to create a D3D11Resource
    // wrapper that presents as a D3D12 resource but translates operations to D3D11
    if (riid == __uuidof(ID3D12Resource)) {
        TRACE("Creating D3D11Resource wrapper for back buffer %u", Buffer);
        
        // Create D3D12 resource description for the swapchain buffer
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = m_width;
        desc.Height = m_height;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = m_format;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        return D3D11Resource::Create(
            m_device,
            m_backbuffers[Buffer].Get(),
            &desc,
            D3D12_RESOURCE_STATE_PRESENT,  // Initial state for swapchain
            riid,
            ppSurface);
    }

    // For D3D11 interfaces, return our D3D11 texture
    return m_backbuffers[Buffer]->QueryInterface(riid, ppSurface);
}

void D3D11SwapChain::ReleaseBackBuffers() {
    TRACE("D3D11SwapChain::ReleaseBackBuffers");
    m_backbuffers.clear();
    m_renderTargetViews.clear();
}

D3D11SwapChain::D3D11SwapChain(
    D3D11Device* device, Microsoft::WRL::ComPtr<IDXGISwapChain1> base_swapchain)
    : m_device(device), m_base_swapchain(std::move(base_swapchain)) {
    TRACE("D3D11SwapChain::D3D11SwapChain");

    // Initialize back buffers
    HRESULT hr = InitBackBuffers();
    if (FAILED(hr)) {
        ERR("Failed to initialize back buffers, hr %#x.", hr);
    }
}

D3D11SwapChain::~D3D11SwapChain() {
    TRACE("D3D11SwapChain::~D3D11SwapChain");
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
    static int frame_count = 0;
    frame_count++;

    // Log every 10th frame to avoid spam
    if (frame_count % 10 == 0) {
        TRACE("Present called - frame %d (SyncInterval=%u, Flags=%#x)",
              frame_count, SyncInterval, Flags);
    }

    HRESULT hr = m_base_swapchain->Present(SyncInterval, Flags);
    if (FAILED(hr)) {
        ERR("Present failed, hr %#x", hr);
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE
D3D11SwapChain::SetFullscreenState(BOOL Fullscreen, IDXGIOutput* pTarget) {
    TRACE("D3D11SwapChain::SetFullscreenState called: Fullscreen=%d, Target=%p",
          Fullscreen, pTarget);
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
        ERR("Failed to resize base swap chain buffers, hr %#x", hr);
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
    return m_base_swapchain->QueryInterface(__uuidof(IDXGISwapChain3),
                                            (void**)&swapchain3) == S_OK
               ? swapchain3->GetLastPresentCount(pLastPresentCount)
               : E_NOINTERFACE;
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
    TRACE("D3D11SwapChain::QueryInterface called: %s",
          debugstr_guid(&riid).c_str());

    if (!ppvObject) return E_INVALIDARG;
    *ppvObject = nullptr;

    // Handle base interfaces ourselves
    if (riid == __uuidof(IUnknown) || riid == __uuidof(IDXGIObject) ||
        riid == __uuidof(IDXGIDeviceSubObject) ||
        riid == __uuidof(IDXGISwapChain) || riid == __uuidof(IDXGISwapChain1) ||
        riid == __uuidof(IDXGISwapChain2) ||
        riid == __uuidof(IDXGISwapChain3) ||
        riid == __uuidof(IDXGISwapChain4)) {
        TRACE("Returning IDXGISwapChain interface");
        *ppvObject = this;
        AddRef();
        return S_OK;
    }

    // Forward unknown interfaces to base swapchain
    HRESULT hr = m_base_swapchain->QueryInterface(riid, ppvObject);
    if (SUCCEEDED(hr)) {
        TRACE("Interface %s handled by base swapchain",
              debugstr_guid(&riid).c_str());
        return S_OK;
    }

    WARN("Unknown interface query %s", debugstr_guid(&riid).c_str());
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