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
        d3d11_desc.BufferCount = 2;  // Force single buffer for DXVK internally
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
        "Swap chain desc - BufferCount: %u, Width: %u, Height: %u, Format: %d",
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

    // Create D3D11 textures and RTVs for all buffers
    for (UINT i = 0; i < desc.BufferCount; i++) {
        TRACE("Creating buffer %u of %u", i, desc.BufferCount);

        // Create D3D11 texture for the back buffer
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

        Microsoft::WRL::ComPtr<ID3D11Texture2D> buffer;
        hr = m_device->GetD3D11Device()->CreateTexture2D(&texDesc, nullptr, &buffer);
        if (FAILED(hr)) {
            ERR("Failed to create back buffer %u texture, hr %#x", i, hr);
            ReleaseBackBuffers();
            return hr;
        }

        // Create RTV description
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = texDesc.Format;
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;

        // Create render target view
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
        hr = m_device->GetD3D11Device()->CreateRenderTargetView(buffer.Get(), &rtvDesc, &rtv);
        if (FAILED(hr)) {
            ERR("Failed to create RTV for back buffer %u, hr %#x.", i, hr);
            ReleaseBackBuffers();
            return hr;
        }

        TRACE(
            "Back buffer %u created - Width: %u, Height: %u, Format: %d",
            i, texDesc.Width, texDesc.Height, texDesc.Format);

        m_backbuffers.push_back(std::move(buffer));
        m_renderTargetViews.push_back(std::move(rtv));
        TRACE("Created back buffer %u with RTV", i);
    }

    return S_OK;
}

void WrappedD3D12ToD3D11SwapChain::ReleaseBackBuffers() {
    TRACE("WrappedD3D12ToD3D11SwapChain::ReleaseBackBuffers");
    
    // Release render target views first since they reference the back buffers
    for (auto& rtv : m_renderTargetViews) {
        if (rtv) {
            rtv->Release();
        }
    }
    m_renderTargetViews.clear();

    // Then release back buffers
    for (auto& buffer : m_backbuffers) {
        if (buffer) {
            buffer->Release();
        }
    }
    m_backbuffers.clear();
}

HRESULT WrappedD3D12ToD3D11SwapChain::GetBuffer(UINT Buffer, REFIID riid, void** ppSurface) {
    TRACE("WrappedD3D12ToD3D11SwapChain::GetBuffer called with Buffer=%u, riid=%s", 
          Buffer, debugstr_guid(&riid).c_str());

    if (!ppSurface) {
        return E_INVALIDARG;
    }

    *ppSurface = nullptr;

    if (Buffer >= m_backbuffers.size()) {
        ERR("Buffer index %u is out of range (max: %zu)", Buffer, m_backbuffers.size());
        return DXGI_ERROR_INVALID_CALL;
    }

    // Get the D3D11 texture
    ID3D11Texture2D* d3d11Texture = m_backbuffers[Buffer].Get();
    if (!d3d11Texture) {
        ERR("Back buffer %u is null", Buffer);
        return E_FAIL;
    }

    // Get texture description for D3D12 resource creation
    D3D11_TEXTURE2D_DESC d3d11Desc;
    d3d11Texture->GetDesc(&d3d11Desc);

    // Create D3D12 resource description
    D3D12_RESOURCE_DESC d3d12Desc = {};
    d3d12Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    d3d12Desc.Alignment = 0;
    d3d12Desc.Width = d3d11Desc.Width;
    d3d12Desc.Height = d3d11Desc.Height;
    d3d12Desc.DepthOrArraySize = d3d11Desc.ArraySize;
    d3d12Desc.MipLevels = d3d11Desc.MipLevels;
    d3d12Desc.Format = d3d11Desc.Format;
    d3d12Desc.SampleDesc = d3d11Desc.SampleDesc;
    d3d12Desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    d3d12Desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    // Default heap properties for swap chain resources
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    // Create wrapped D3D12 resource
    void* resource = nullptr;
    HRESULT hr = WrappedD3D12ToD3D11Resource::Create(
        m_device,
        d3d11Texture,
        &d3d12Desc,
        D3D12_RESOURCE_STATE_PRESENT,
        riid,
        &resource);

    if (FAILED(hr)) {
        ERR("Failed to create wrapped D3D12 resource for back buffer %u, hr %#x", Buffer, hr);
        return hr;
    }

    *ppSurface = resource;
    return S_OK;
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

    // Get current back buffer index
    UINT currentIndex = 0;
    if (Microsoft::WRL::ComPtr<IDXGISwapChain3> swapchain3; 
        SUCCEEDED(m_base_swapchain->QueryInterface(__uuidof(IDXGISwapChain3), 
                                                 (void**)&swapchain3))) {
        currentIndex = swapchain3->GetCurrentBackBufferIndex();
    }

    // Ensure we have valid back buffers
    if (currentIndex >= m_backbuffers.size() || !m_backbuffers[currentIndex]) {
        ERR("Invalid back buffer index %u (total buffers: %zu)", 
            currentIndex, m_backbuffers.size());
        return E_FAIL;
    }

    // Get D3D11 immediate context
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    m_device->GetD3D11Device()->GetImmediateContext(&context);

    // Transition from D3D12_RESOURCE_STATE_RENDER_TARGET to D3D12_RESOURCE_STATE_PRESENT
    // In D3D11 this means:
    // 1. Flush any pending rendering
    context->Flush();

    // 2. Ensure back buffer is ready for presentation
    if (Flags & DXGI_PRESENT_DO_NOT_WAIT) {
        // If DO_NOT_WAIT is set, we don't wait for GPU
        // This maps to D3D12's DXGI_PRESENT_DO_NOT_WAIT
    } else {
        // Wait for GPU to complete all work
        // This ensures the transition to PRESENT state is complete
        context->Flush();
    }

    // Present the back buffer
    HRESULT hr = m_base_swapchain->Present(SyncInterval, Flags);
    if (FAILED(hr)) {
        ERR("Present failed, hr %#x", hr);
        return hr;
    }

    // After presentation, the back buffer is automatically transitioned to 
    // D3D12_RESOURCE_STATE_COMMON in D3D12
    // In D3D11, we don't need to do anything as the runtime handles this

    return S_OK;
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