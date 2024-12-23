#include "d3d11_impl/swap_chain.hpp"

#include "d3d11_impl/device.hpp"
#include "d3d11_impl/resource.hpp"

namespace dxiided {

HRESULT WrappedD3D12ToD3D11SwapChain::Create(
    WrappedD3D12ToD3D11Device* device, IDXGIFactory* factory, HWND window,
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

    // Create a modified desc that's compatible with D3D11
    DXGI_SWAP_CHAIN_DESC1 d3d11_desc = *desc;
    
    // Clear all flags and only set what we need
    d3d11_desc.Flags = 0;
    
    // Force windowed mode for DXVK compatibility
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fs_desc = {};
    if (fullscreen_desc) {
        fs_desc = *fullscreen_desc;
        fs_desc.Windowed = TRUE;  // Force windowed mode
    } else {
        fs_desc.Windowed = TRUE;
        fs_desc.RefreshRate.Numerator = 60;
        fs_desc.RefreshRate.Denominator = 1;
        fs_desc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        fs_desc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    }

    // Use different swap effects based on backend
    bool using_dxvk = IsDXVKBackend(device);
    if (using_dxvk) {
        // DXVK: Use DISCARD with single buffer
        d3d11_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        d3d11_desc.BufferCount = 1;  // Force single buffer for DXVK internally
        TRACE("Using DXVK mode: Single buffer with DXGI_SWAP_EFFECT_DISCARD");
    } else {
        // WineD3D: Use FLIP_SEQUENTIAL with requested buffer count
        d3d11_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        d3d11_desc.BufferCount = desc->BufferCount;
        TRACE("Using WineD3D mode: %u buffers with DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL", desc->BufferCount);
    }
    
    // Set buffer usage explicitly
    d3d11_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    
    // Keep original format and dimensions
    d3d11_desc.Format = desc->Format;
    d3d11_desc.Width = desc->Width;
    d3d11_desc.Height = desc->Height;
    d3d11_desc.SampleDesc = desc->SampleDesc;

    // Get D3D11 device
    ID3D11Device* d3d11_device = device->GetD3D11Device();
    if (!d3d11_device) {
        ERR("Failed to get D3D11 device");
        return E_FAIL;
    }

    // Get IDXGIFactory2 interface for CreateSwapChainForHwnd
    Microsoft::WRL::ComPtr<IDXGIFactory2> factory2;
    HRESULT hr = factory->QueryInterface(__uuidof(IDXGIFactory2), &factory2);
    if (FAILED(hr)) {
        ERR("Failed to get IDXGIFactory2 interface, hr %#x", hr);
        return hr;
    }

    // Create the swap chain
    Microsoft::WRL::ComPtr<IDXGISwapChain1> base_swapchain;
    hr = factory2->CreateSwapChainForHwnd(
        d3d11_device, window, &d3d11_desc,
        &fs_desc, output, &base_swapchain);
    
    if (FAILED(hr)) {
        ERR("Failed to create DXGI swap chain, hr %#x", hr);
        return hr;
    }

    // Create our wrapper
    *ppSwapChain = new WrappedD3D12ToD3D11SwapChain(device, base_swapchain);
    if (!*ppSwapChain) {
        ERR("Failed to allocate swap chain wrapper");
        return E_OUTOFMEMORY;
    }

    return S_OK;
}

HRESULT WrappedD3D12ToD3D11SwapChain::InitBackBuffers() {
    TRACE("WrappedD3D12ToD3D11SwapChain::InitBackBuffers");

    // Get swap chain description
    DXGI_SWAP_CHAIN_DESC desc;
    HRESULT hr = m_base_swapchain->GetDesc(&desc);
    if (FAILED(hr)) {
        ERR("Failed to get swap chain description, hr %#x.", hr);
        return hr;
    }

    TRACE(
        "Swap chain desc - BufferCount: %u, Width: %u, Height: %u, Format: "
        "%d",
        desc.BufferCount, desc.BufferDesc.Width, desc.BufferDesc.Height,
        desc.BufferDesc.Format);

    // Validate buffer count matches what we requested
    if (desc.BufferCount < 1 || desc.BufferCount > 3) {
        ERR("Unexpected buffer count %u from swap chain", desc.BufferCount);
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
        TRACE("Attempting to get buffer %u of %u", i, desc.BufferCount);

        Microsoft::WRL::ComPtr<ID3D11Texture2D> buffer;
        hr = m_base_swapchain->GetBuffer(
            i, __uuidof(ID3D11Texture2D),
            reinterpret_cast<void**>(buffer.GetAddressOf()));

        if (FAILED(hr)) {
            ERR("Failed to get back buffer %u, hr %#x - this is required for proper operation",
                i, hr);
            // Try to get more information about the failure
            D3D11_TEXTURE2D_DESC desc_attempt = {};
            desc_attempt.Width = m_width;
            desc_attempt.Height = m_height;
            desc_attempt.MipLevels = 1;
            desc_attempt.ArraySize = 1;
            desc_attempt.Format = m_format;
            desc_attempt.SampleDesc.Count = 1;
            desc_attempt.SampleDesc.Quality = 0;
            desc_attempt.Usage = D3D11_USAGE_DEFAULT;
            desc_attempt.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            desc_attempt.CPUAccessFlags = 0;
            desc_attempt.MiscFlags = 0;

            Microsoft::WRL::ComPtr<ID3D11Texture2D> test_buffer;
            HRESULT test_hr = m_device->GetD3D11Device()->CreateTexture2D(&desc_attempt, nullptr, &test_buffer);
            ERR("Test texture creation %s (hr: %#x)", SUCCEEDED(test_hr) ? "succeeded" : "failed", test_hr);
            
            ReleaseBackBuffers();  // Clean up any buffers we did get
            return hr;  // Fail if we can't get all buffers
        }

        // Get original buffer description
        D3D11_TEXTURE2D_DESC tex_desc;
        buffer->GetDesc(&tex_desc);

        // Create RTV description
        D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
        rtv_desc.Format = tex_desc.Format;
        rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtv_desc.Texture2D.MipSlice = 0;

        // Create render target view
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
        hr = m_device->GetD3D11Device()->CreateRenderTargetView(buffer.Get(), &rtv_desc, &rtv);
        if (FAILED(hr)) {
            ERR("Failed to create RTV for back buffer %u, hr %#x.", i, hr);
            ReleaseBackBuffers();
            return hr;
        }

        TRACE(
            "Back buffer %u validated - Width: %u, Height: %u, Format: %d, "
            "ArraySize: %u, BindFlags: %#x",
            i, tex_desc.Width, tex_desc.Height, tex_desc.Format,
            tex_desc.ArraySize, tex_desc.BindFlags);

        m_backbuffers.push_back(std::move(buffer));
        m_renderTargetViews.push_back(std::move(rtv));
        TRACE("Created back buffer %u with RTV", i);
    }

    // Update actual buffer count
    m_buffer_count = static_cast<UINT>(m_backbuffers.size());

    TRACE("Successfully initialized %u back buffers ", m_buffer_count);
    return S_OK;
}

HRESULT WrappedD3D12ToD3D11SwapChain::GetBuffer(UINT Buffer, REFIID riid, void** ppSurface) {
    TRACE("WrappedD3D12ToD3D11SwapChain::GetBuffer %u, %s, %p", Buffer,
          debugstr_guid(&riid).c_str(), ppSurface);

    // When using DXVK with single buffer mode, map all buffer indices to buffer 0
    UINT actual_buffer = (m_buffer_count == 1) ? 0 : Buffer;

    if (actual_buffer >= m_buffer_count || !ppSurface) {
        ERR("Invalid buffer index %u (buffer_count=%u) or null surface pointer",
            Buffer, m_buffer_count);
        return DXGI_ERROR_INVALID_CALL;
    }

    *ppSurface = nullptr;

    if (riid == __uuidof(ID3D11Texture2D)) {
        TRACE("Returning D3D11 texture for buffer %u (mapped from request for buffer %u)", 
              actual_buffer, Buffer);
        m_backbuffers[actual_buffer]->AddRef();
        *ppSurface = m_backbuffers[actual_buffer].Get();
        return S_OK;
    }

    // Try to get the underlying D3D12 resource from the base swapchain
    if (riid == __uuidof(ID3D12Resource)) {
        TRACE("Game requesting D3D12 resource for backbuffer");

        // Create resource description for the back buffer
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = m_width;
        desc.Height = m_height;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = m_format;
        desc.SampleDesc = {1, 0};
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        TRACE("Back buffer description: Width: %u, Height: %u, Format: %d",
              desc.Width, desc.Height, desc.Format);

        // Set up heap properties for the back buffer
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;

        // Create D3D11Resource wrapper around the back buffer
        return WrappedD3D12ToD3D11Resource::Create(
            m_device, &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,  // No optimized clear value for swap chain
            riid, ppSurface);
    }
    TRACE("other interface");
    // For any other interface, try querying our backbuffer
    return m_backbuffers[actual_buffer]->QueryInterface(riid, ppSurface);
}


void WrappedD3D12ToD3D11SwapChain::ReleaseBackBuffers() {
    TRACE("WrappedD3D12ToD3D11SwapChain::ReleaseBackBuffers");
    m_backbuffers.clear();
    m_renderTargetViews.clear();
}

WrappedD3D12ToD3D11SwapChain::WrappedD3D12ToD3D11SwapChain(
    WrappedD3D12ToD3D11Device* device, Microsoft::WRL::ComPtr<IDXGISwapChain1> base_swapchain)
    : m_device(device), m_base_swapchain(std::move(base_swapchain)) {
    TRACE("WrappedD3D12ToD3D11SwapChain::WrappedD3D12ToD3D11SwapChain");

    // Initialize back buffers
    HRESULT hr = InitBackBuffers();
    if (FAILED(hr)) {
        ERR("Failed to initialize back buffers, hr %#x.", hr);
    }
}

WrappedD3D12ToD3D11SwapChain::~WrappedD3D12ToD3D11SwapChain() {
    TRACE("WrappedD3D12ToD3D11SwapChain::~WrappedD3D12ToD3D11SwapChain");
    ReleaseBackBuffers();
}

// IDXGIObject methods
HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11SwapChain::SetPrivateData(REFGUID Name,
                                                         UINT DataSize,
                                                         const void* pData) {
    TRACE("WrappedD3D12ToD3D11SwapChain::SetPrivateData called");
    return m_base_swapchain->SetPrivateData(Name, DataSize, pData);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11SwapChain::SetPrivateDataInterface(
    REFGUID Name, const IUnknown* pUnknown) {
    TRACE("WrappedD3D12ToD3D11SwapChain::SetPrivateDataInterface called");
    return m_base_swapchain->SetPrivateDataInterface(Name, pUnknown);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11SwapChain::GetPrivateData(REFGUID Name,
                                                         UINT* pDataSize,
                                                         void* pData) {
    TRACE("WrappedD3D12ToD3D11SwapChain::GetPrivateData called");
    return m_base_swapchain->GetPrivateData(Name, pDataSize, pData);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11SwapChain::GetParent(REFIID riid,
                                                    void** ppParent) {
    TRACE("WrappedD3D12ToD3D11SwapChain::GetParent called");
    return m_base_swapchain->GetParent(riid, ppParent);
}

// IDXGIDeviceSubObject methods
HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11SwapChain::GetDevice(REFIID riid,
                                                    void** ppDevice) {
    TRACE("WrappedD3D12ToD3D11SwapChain::GetDevice called");
    return m_base_swapchain->GetDevice(riid, ppDevice);
}

// IDXGISwapChain methods
HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11SwapChain::Present(UINT SyncInterval,
                                                  UINT Flags) {
    TRACE("WrappedD3D12ToD3D11SwapChain::Present called");
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
WrappedD3D12ToD3D11SwapChain::SetFullscreenState(BOOL Fullscreen, IDXGIOutput* pTarget) {
    TRACE("WrappedD3D12ToD3D11SwapChain::SetFullscreenState called: Fullscreen=%d, Target=%p",
          Fullscreen, pTarget);
    return m_base_swapchain->SetFullscreenState(Fullscreen, pTarget);
}

HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11SwapChain::GetFullscreenState(BOOL* pFullscreen, IDXGIOutput** ppTarget) {
    TRACE("WrappedD3D12ToD3D11SwapChain::GetFullscreenState called");
    return m_base_swapchain->GetFullscreenState(pFullscreen, ppTarget);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11SwapChain::GetDesc(DXGI_SWAP_CHAIN_DESC* pDesc) {
    TRACE("WrappedD3D12ToD3D11SwapChain::GetDesc called");
    return m_base_swapchain->GetDesc(pDesc);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11SwapChain::ResizeBuffers(UINT BufferCount,
                                                        UINT Width, UINT Height,
                                                        DXGI_FORMAT NewFormat,
                                                        UINT SwapChainFlags) {
    TRACE("WrappedD3D12ToD3D11SwapChain::ResizeBuffers called");
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
WrappedD3D12ToD3D11SwapChain::ResizeTarget(const DXGI_MODE_DESC* pNewTargetParameters) {
    TRACE("WrappedD3D12ToD3D11SwapChain::ResizeTarget called");
    return m_base_swapchain->ResizeTarget(pNewTargetParameters);
}

HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11SwapChain::GetContainingOutput(IDXGIOutput** ppOutput) {
    TRACE("WrappedD3D12ToD3D11SwapChain::GetContainingOutput called");
    return m_base_swapchain->GetContainingOutput(ppOutput);
}

HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11SwapChain::GetFrameStatistics(DXGI_FRAME_STATISTICS* pStats) {
    TRACE("WrappedD3D12ToD3D11SwapChain::GetFrameStatistics called");
    return m_base_swapchain->GetFrameStatistics(pStats);
}

HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11SwapChain::GetLastPresentCount(UINT* pLastPresentCount) {
    TRACE("WrappedD3D12ToD3D11SwapChain::GetLastPresentCount called");
    return m_base_swapchain->QueryInterface(__uuidof(IDXGISwapChain3),
                                            (void**)&swapchain3) == S_OK
               ? swapchain3->GetLastPresentCount(pLastPresentCount)
               : E_NOINTERFACE;
}

// IDXGISwapChain1 methods
HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11SwapChain::GetDesc1(DXGI_SWAP_CHAIN_DESC1* pDesc) {
    TRACE("WrappedD3D12ToD3D11SwapChain::GetDesc1 called");
    return m_base_swapchain->GetDesc1(pDesc);
}

HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11SwapChain::GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pDesc) {
    TRACE("WrappedD3D12ToD3D11SwapChain::GetFullscreenDesc called");
    return m_base_swapchain->GetFullscreenDesc(pDesc);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11SwapChain::GetHwnd(HWND* pHwnd) {
    TRACE("WrappedD3D12ToD3D11SwapChain::GetHwnd called");
    return m_base_swapchain->GetHwnd(pHwnd);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11SwapChain::GetCoreWindow(REFIID refiid,
                                                        void** ppUnk) {
    TRACE("WrappedD3D12ToD3D11SwapChain::GetCoreWindow called");
    return m_base_swapchain->GetCoreWindow(refiid, ppUnk);
}

HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11SwapChain::Present1(UINT SyncInterval, UINT PresentFlags,
                         const DXGI_PRESENT_PARAMETERS* pPresentParameters) {
    TRACE("WrappedD3D12ToD3D11SwapChain::Present1 called");
    return m_base_swapchain->Present1(SyncInterval, PresentFlags,
                                      pPresentParameters);
}

BOOL STDMETHODCALLTYPE WrappedD3D12ToD3D11SwapChain::IsTemporaryMonoSupported() {
    TRACE("WrappedD3D12ToD3D11SwapChain::IsTemporaryMonoSupported called");
    return m_base_swapchain->IsTemporaryMonoSupported();
}

HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11SwapChain::GetRestrictToOutput(IDXGIOutput** ppRestrictToOutput) {
    TRACE("WrappedD3D12ToD3D11SwapChain::GetRestrictToOutput called");
    return m_base_swapchain->GetRestrictToOutput(ppRestrictToOutput);
}

HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11SwapChain::SetBackgroundColor(const DXGI_RGBA* pColor) {
    TRACE("WrappedD3D12ToD3D11SwapChain::SetBackgroundColor called");
    return m_base_swapchain->SetBackgroundColor(pColor);
}

HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11SwapChain::GetBackgroundColor(DXGI_RGBA* pColor) {
    TRACE("WrappedD3D12ToD3D11SwapChain::GetBackgroundColor called");
    return m_base_swapchain->GetBackgroundColor(pColor);
}

HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11SwapChain::SetRotation(DXGI_MODE_ROTATION Rotation) {
    TRACE("WrappedD3D12ToD3D11SwapChain::SetRotation called");
    return m_base_swapchain->SetRotation(Rotation);
}

HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11SwapChain::GetRotation(DXGI_MODE_ROTATION* pRotation) {
    TRACE("WrappedD3D12ToD3D11SwapChain::GetRotation called");
    return m_base_swapchain->GetRotation(pRotation);
}

// IUnknown methods
HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11SwapChain::QueryInterface(REFIID riid,
                                                         void** ppvObject) {
    TRACE("WrappedD3D12ToD3D11SwapChain::QueryInterface called: %s",
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

ULONG STDMETHODCALLTYPE WrappedD3D12ToD3D11SwapChain::AddRef() {
    TRACE("WrappedD3D12ToD3D11SwapChain::AddRef called");
    return InterlockedIncrement(&m_refcount);
}

ULONG STDMETHODCALLTYPE WrappedD3D12ToD3D11SwapChain::Release() {
    TRACE("WrappedD3D12ToD3D11SwapChain::Release called");
    ULONG refcount = InterlockedDecrement(&m_refcount);
    if (refcount == 0) {
        delete this;
    }
    return refcount;
}
}  // namespace dxiided