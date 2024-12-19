#include "d3d11_impl/pipeline_state.hpp"

#include "d3d11_impl/device.hpp"

namespace dxiided {

// Static member initialization
std::unordered_map<D3D11PipelineState::PipelineStateKey,
                   Microsoft::WRL::ComPtr<D3D11PipelineState>,
                   D3D11PipelineState::PipelineStateKeyHasher>
    D3D11PipelineState::s_pipelineStateCache;
std::mutex D3D11PipelineState::s_cacheMutex;

HRESULT D3D11PipelineState::CreateGraphics(
    D3D11Device* device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc,
    REFIID riid, void** ppPipelineState) {
    TRACE("%p, %p, %s, %p", device, pDesc, debugstr_guid(&riid).c_str(),
          ppPipelineState);

    if (!device || !pDesc || !ppPipelineState) {
        return E_INVALIDARG;
    }

    // Try to find cached state
    PipelineStateKey key = ComputeHash(pDesc);
    Microsoft::WRL::ComPtr<D3D11PipelineState> cachedState =
        GetCachedState(key);
    if (cachedState) {
        return cachedState.CopyTo(
            reinterpret_cast<ID3D12PipelineState**>(ppPipelineState));
    }

    // Create new state
    Microsoft::WRL::ComPtr<D3D11PipelineState> state =
        new D3D11PipelineState(device);
    HRESULT hr = state->InitializeGraphics(pDesc);
    if (FAILED(hr)) {
        return hr;
    }

    // Cache the new state
    CacheState(key, state);

    return state.CopyTo(
        reinterpret_cast<ID3D12PipelineState**>(ppPipelineState));
}

HRESULT D3D11PipelineState::CreateCompute(
    D3D11Device* device, const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc,
    REFIID riid, void** ppPipelineState) {
    TRACE("%p, %p, %s, %p", device, pDesc, debugstr_guid(&riid).c_str(),
          ppPipelineState);

    if (!device || !pDesc || !ppPipelineState) {
        return E_INVALIDARG;
    }

    // Try to find cached state
    PipelineStateKey key = ComputeHash(pDesc);
    Microsoft::WRL::ComPtr<D3D11PipelineState> cachedState =
        GetCachedState(key);
    if (cachedState) {
        return cachedState.CopyTo(
            reinterpret_cast<ID3D12PipelineState**>(ppPipelineState));
    }

    // Create new state
    Microsoft::WRL::ComPtr<D3D11PipelineState> state =
        new D3D11PipelineState(device);
    HRESULT hr = state->InitializeCompute(pDesc);
    if (FAILED(hr)) {
        return hr;
    }

    // Cache the new state
    CacheState(key, state);

    return state.CopyTo(
        reinterpret_cast<ID3D12PipelineState**>(ppPipelineState));
}

Microsoft::WRL::ComPtr<D3D11PipelineState> D3D11PipelineState::GetCachedState(
    const PipelineStateKey& key) {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    auto it = s_pipelineStateCache.find(key);
    if (it != s_pipelineStateCache.end()) {
        return it->second;
    }
    return nullptr;
}

void D3D11PipelineState::CacheState(
    const PipelineStateKey& key,
    Microsoft::WRL::ComPtr<D3D11PipelineState> state) {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    s_pipelineStateCache[key] = state;
}

D3D11PipelineState::PipelineStateKey D3D11PipelineState::ComputeHash(
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc) {
    PipelineStateKey key;

    // Add all relevant fields to the hash
    key.hash.insert(
        key.hash.end(), reinterpret_cast<const uint8_t*>(&pDesc->VS),
        reinterpret_cast<const uint8_t*>(&pDesc->VS) + sizeof(pDesc->VS));
    key.hash.insert(
        key.hash.end(), reinterpret_cast<const uint8_t*>(&pDesc->PS),
        reinterpret_cast<const uint8_t*>(&pDesc->PS) + sizeof(pDesc->PS));
    key.hash.insert(
        key.hash.end(), reinterpret_cast<const uint8_t*>(&pDesc->DS),
        reinterpret_cast<const uint8_t*>(&pDesc->DS) + sizeof(pDesc->DS));
    key.hash.insert(
        key.hash.end(), reinterpret_cast<const uint8_t*>(&pDesc->HS),
        reinterpret_cast<const uint8_t*>(&pDesc->HS) + sizeof(pDesc->HS));
    key.hash.insert(
        key.hash.end(), reinterpret_cast<const uint8_t*>(&pDesc->GS),
        reinterpret_cast<const uint8_t*>(&pDesc->GS) + sizeof(pDesc->GS));

    // Add input layout
    for (UINT i = 0; i < pDesc->InputLayout.NumElements; i++) {
        const auto& element = pDesc->InputLayout.pInputElementDescs[i];
        key.hash.insert(
            key.hash.end(), reinterpret_cast<const uint8_t*>(&element),
            reinterpret_cast<const uint8_t*>(&element) + sizeof(element));
    }

    // Add other state objects
    key.hash.insert(key.hash.end(),
                    reinterpret_cast<const uint8_t*>(&pDesc->BlendState),
                    reinterpret_cast<const uint8_t*>(&pDesc->BlendState) +
                        sizeof(pDesc->BlendState));
    key.hash.insert(key.hash.end(),
                    reinterpret_cast<const uint8_t*>(&pDesc->RasterizerState),
                    reinterpret_cast<const uint8_t*>(&pDesc->RasterizerState) +
                        sizeof(pDesc->RasterizerState));
    key.hash.insert(
        key.hash.end(),
        reinterpret_cast<const uint8_t*>(&pDesc->DepthStencilState),
        reinterpret_cast<const uint8_t*>(&pDesc->DepthStencilState) +
            sizeof(pDesc->DepthStencilState));

    return key;
}

D3D11PipelineState::PipelineStateKey D3D11PipelineState::ComputeHash(
    const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc) {
    PipelineStateKey key;

    // For compute pipeline, we only need to hash the compute shader
    key.hash.insert(
        key.hash.end(), reinterpret_cast<const uint8_t*>(&pDesc->CS),
        reinterpret_cast<const uint8_t*>(&pDesc->CS) + sizeof(pDesc->CS));

    return key;
}

D3D11PipelineState::D3D11PipelineState(D3D11Device* device) : m_device(device) {
    TRACE("%p", device);
}

HRESULT D3D11PipelineState::InitializeGraphics(
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc) {
    TRACE("Initializing graphics pipeline state");

    // Create vertex shader
    if (pDesc->VS.pShaderBytecode && pDesc->VS.BytecodeLength) {
        HRESULT hr = m_device->GetD3D11Device()->CreateVertexShader(
            pDesc->VS.pShaderBytecode, pDesc->VS.BytecodeLength, nullptr,
            &m_vertexShader);
        if (FAILED(hr)) {
            ERR("Failed to create vertex shader, hr %#x.", hr);
            return hr;
        }
    }

    // Create stream output if requested
    if (pDesc->StreamOutput.NumEntries > 0) {
        HRESULT hr;
        if (pDesc->GS.pShaderBytecode && pDesc->GS.BytecodeLength) {
            hr = CreateStreamOutputShader(&pDesc->StreamOutput,
                                          pDesc->GS.pShaderBytecode,
                                          pDesc->GS.BytecodeLength);
        } else {
            hr = CreateStreamOutputShader(&pDesc->StreamOutput,
                                          pDesc->VS.pShaderBytecode,
                                          pDesc->VS.BytecodeLength);
        }
        if (FAILED(hr)) {
            ERR("Failed to create stream output shader, hr %#x.", hr);
            return hr;
        }
    }

    // Create pixel shader
    if (pDesc->PS.pShaderBytecode && pDesc->PS.BytecodeLength) {
        HRESULT hr = m_device->GetD3D11Device()->CreatePixelShader(
            pDesc->PS.pShaderBytecode, pDesc->PS.BytecodeLength, nullptr,
            &m_pixelShader);
        if (FAILED(hr)) {
            ERR("Failed to create pixel shader, hr %#x.", hr);
            return hr;
        }
    }

    // Create geometry shader
    if (pDesc->GS.pShaderBytecode && pDesc->GS.BytecodeLength) {
        HRESULT hr = m_device->GetD3D11Device()->CreateGeometryShader(
            pDesc->GS.pShaderBytecode, pDesc->GS.BytecodeLength, nullptr,
            &m_geometryShader);
        if (FAILED(hr)) {
            ERR("Failed to create geometry shader, hr %#x.", hr);
            return hr;
        }
    }

    // Create hull shader
    if (pDesc->HS.pShaderBytecode && pDesc->HS.BytecodeLength) {
        HRESULT hr = m_device->GetD3D11Device()->CreateHullShader(
            pDesc->HS.pShaderBytecode, pDesc->HS.BytecodeLength, nullptr,
            &m_hullShader);
        if (FAILED(hr)) {
            ERR("Failed to create hull shader, hr %#x.", hr);
            return hr;
        }
    }

    // Create domain shader
    if (pDesc->DS.pShaderBytecode && pDesc->DS.BytecodeLength) {
        HRESULT hr = m_device->GetD3D11Device()->CreateDomainShader(
            pDesc->DS.pShaderBytecode, pDesc->DS.BytecodeLength, nullptr,
            &m_domainShader);
        if (FAILED(hr)) {
            ERR("Failed to create domain shader, hr %#x.", hr);
            return hr;
        }
    }

    // Create input layout if vertex shader is present
    if (m_vertexShader && pDesc->InputLayout.NumElements) {
        std::vector<D3D11_INPUT_ELEMENT_DESC> inputElements(
            pDesc->InputLayout.NumElements);
        for (UINT i = 0; i < pDesc->InputLayout.NumElements; i++) {
            const auto& d3d12Elem = pDesc->InputLayout.pInputElementDescs[i];
            auto& d3d11Elem = inputElements[i];

            d3d11Elem.SemanticName = d3d12Elem.SemanticName;
            d3d11Elem.SemanticIndex = d3d12Elem.SemanticIndex;
            d3d11Elem.Format = d3d12Elem.Format;
            d3d11Elem.InputSlot = d3d12Elem.InputSlot;
            d3d11Elem.AlignedByteOffset = d3d12Elem.AlignedByteOffset;
            d3d11Elem.InputSlotClass = static_cast<D3D11_INPUT_CLASSIFICATION>(
                d3d12Elem.InputSlotClass);
            d3d11Elem.InstanceDataStepRate = d3d12Elem.InstanceDataStepRate;
        }

        HRESULT hr = m_device->GetD3D11Device()->CreateInputLayout(
            inputElements.data(), pDesc->InputLayout.NumElements,
            pDesc->VS.pShaderBytecode, pDesc->VS.BytecodeLength,
            &m_inputLayout);
        if (FAILED(hr)) {
            ERR("Failed to create input layout, hr %#x.", hr);
            return hr;
        }
    }

    // Create blend state
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable = pDesc->BlendState.AlphaToCoverageEnable;
    blendDesc.IndependentBlendEnable = pDesc->BlendState.IndependentBlendEnable;
    for (UINT i = 0; i < 8; i++) {
        const auto& d3d12RT = pDesc->BlendState.RenderTarget[i];
        auto& d3d11RT = blendDesc.RenderTarget[i];

        d3d11RT.BlendEnable = d3d12RT.BlendEnable;
        d3d11RT.SrcBlend = static_cast<D3D11_BLEND>(d3d12RT.SrcBlend);
        d3d11RT.DestBlend = static_cast<D3D11_BLEND>(d3d12RT.DestBlend);
        d3d11RT.BlendOp = static_cast<D3D11_BLEND_OP>(d3d12RT.BlendOp);
        d3d11RT.SrcBlendAlpha = static_cast<D3D11_BLEND>(d3d12RT.SrcBlendAlpha);
        d3d11RT.DestBlendAlpha =
            static_cast<D3D11_BLEND>(d3d12RT.DestBlendAlpha);
        d3d11RT.BlendOpAlpha =
            static_cast<D3D11_BLEND_OP>(d3d12RT.BlendOpAlpha);
        d3d11RT.RenderTargetWriteMask = d3d12RT.RenderTargetWriteMask;
    }

    HRESULT hr =
        m_device->GetD3D11Device()->CreateBlendState(&blendDesc, &m_blendState);
    if (FAILED(hr)) {
        ERR("Failed to create blend state, hr %#x.", hr);
        return hr;
    }

    // Create rasterizer state
    D3D11_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode =
        static_cast<D3D11_FILL_MODE>(pDesc->RasterizerState.FillMode);
    rasterizerDesc.CullMode =
        static_cast<D3D11_CULL_MODE>(pDesc->RasterizerState.CullMode);
    rasterizerDesc.FrontCounterClockwise =
        pDesc->RasterizerState.FrontCounterClockwise;
    rasterizerDesc.DepthBias = pDesc->RasterizerState.DepthBias;
    rasterizerDesc.DepthBiasClamp = pDesc->RasterizerState.DepthBiasClamp;
    rasterizerDesc.SlopeScaledDepthBias =
        pDesc->RasterizerState.SlopeScaledDepthBias;
    rasterizerDesc.DepthClipEnable = pDesc->RasterizerState.DepthClipEnable;
    rasterizerDesc.ScissorEnable = TRUE;  // D3D12 always enables scissor
    rasterizerDesc.MultisampleEnable = pDesc->RasterizerState.MultisampleEnable;
    rasterizerDesc.AntialiasedLineEnable =
        pDesc->RasterizerState.AntialiasedLineEnable;

    hr = m_device->GetD3D11Device()->CreateRasterizerState(&rasterizerDesc,
                                                           &m_rasterizerState);
    if (FAILED(hr)) {
        ERR("Failed to create rasterizer state, hr %#x.", hr);
        return hr;
    }

    // Create depth-stencil state
    D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable = pDesc->DepthStencilState.DepthEnable;
    depthStencilDesc.DepthWriteMask = static_cast<D3D11_DEPTH_WRITE_MASK>(
        pDesc->DepthStencilState.DepthWriteMask);
    depthStencilDesc.DepthFunc =
        static_cast<D3D11_COMPARISON_FUNC>(pDesc->DepthStencilState.DepthFunc);
    depthStencilDesc.StencilEnable = pDesc->DepthStencilState.StencilEnable;
    depthStencilDesc.StencilReadMask = pDesc->DepthStencilState.StencilReadMask;
    depthStencilDesc.StencilWriteMask =
        pDesc->DepthStencilState.StencilWriteMask;

    const auto& frontFace = pDesc->DepthStencilState.FrontFace;
    depthStencilDesc.FrontFace.StencilFailOp =
        static_cast<D3D11_STENCIL_OP>(frontFace.StencilFailOp);
    depthStencilDesc.FrontFace.StencilDepthFailOp =
        static_cast<D3D11_STENCIL_OP>(frontFace.StencilDepthFailOp);
    depthStencilDesc.FrontFace.StencilPassOp =
        static_cast<D3D11_STENCIL_OP>(frontFace.StencilPassOp);
    depthStencilDesc.FrontFace.StencilFunc =
        static_cast<D3D11_COMPARISON_FUNC>(frontFace.StencilFunc);

    const auto& backFace = pDesc->DepthStencilState.BackFace;
    depthStencilDesc.BackFace.StencilFailOp =
        static_cast<D3D11_STENCIL_OP>(backFace.StencilFailOp);
    depthStencilDesc.BackFace.StencilDepthFailOp =
        static_cast<D3D11_STENCIL_OP>(backFace.StencilDepthFailOp);
    depthStencilDesc.BackFace.StencilPassOp =
        static_cast<D3D11_STENCIL_OP>(backFace.StencilPassOp);
    depthStencilDesc.BackFace.StencilFunc =
        static_cast<D3D11_COMPARISON_FUNC>(backFace.StencilFunc);

    hr = m_device->GetD3D11Device()->CreateDepthStencilState(
        &depthStencilDesc, &m_depthStencilState);
    if (FAILED(hr)) {
        ERR("Failed to create depth-stencil state, hr %#x.", hr);
        return hr;
    }

    return S_OK;
}

HRESULT D3D11PipelineState::CreateStreamOutputShader(
    const D3D12_STREAM_OUTPUT_DESC* pSODesc, const void* pShaderBytecode,
    SIZE_T BytecodeLength) {
    // Convert D3D12 stream output declarations to D3D11
    std::vector<D3D11_SO_DECLARATION_ENTRY> soDeclarations(pSODesc->NumEntries);
    for (UINT i = 0; i < pSODesc->NumEntries; i++) {
        const auto& d3d12Entry = pSODesc->pSODeclaration[i];
        auto& d3d11Entry = soDeclarations[i];

        d3d11Entry.Stream = d3d12Entry.Stream;
        d3d11Entry.SemanticName = d3d12Entry.SemanticName;
        d3d11Entry.SemanticIndex = d3d12Entry.SemanticIndex;
        d3d11Entry.StartComponent = d3d12Entry.StartComponent;
        d3d11Entry.ComponentCount = d3d12Entry.ComponentCount;
        d3d11Entry.OutputSlot = d3d12Entry.OutputSlot;
    }

    // Store the stream output configuration
    m_soDeclaration = new D3D11_SO_DECLARATION_ENTRY[pSODesc->NumEntries];
    memcpy(m_soDeclaration, soDeclarations.data(),
           sizeof(D3D11_SO_DECLARATION_ENTRY) * pSODesc->NumEntries);
    m_numSODeclarations = pSODesc->NumEntries;

    m_soStrides = new UINT[pSODesc->NumStrides];
    memcpy(m_soStrides, pSODesc->pBufferStrides,
           sizeof(UINT) * pSODesc->NumStrides);
    m_numSOStrides = pSODesc->NumStrides;

    m_rasterizedStream = pSODesc->RasterizedStream;

    // Create the geometry shader with stream output
    return m_device->GetD3D11Device()->CreateGeometryShaderWithStreamOutput(
        pShaderBytecode, BytecodeLength, soDeclarations.data(),
        pSODesc->NumEntries, pSODesc->pBufferStrides, pSODesc->NumStrides,
        pSODesc->RasterizedStream, nullptr, &m_streamOutShader);
}

HRESULT D3D11PipelineState::InitializeCompute(
    const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc) {
    TRACE("Initializing compute pipeline state");

    if (!pDesc->CS.pShaderBytecode || !pDesc->CS.BytecodeLength) {
        ERR("No compute shader provided.");
        return E_INVALIDARG;
    }

    HRESULT hr = m_device->GetD3D11Device()->CreateComputeShader(
        pDesc->CS.pShaderBytecode, pDesc->CS.BytecodeLength, nullptr,
        &m_computeShader);
    if (FAILED(hr)) {
        ERR("Failed to create compute shader, hr %#x.", hr);
        return hr;
    }

    return S_OK;
}

// IUnknown methods
HRESULT STDMETHODCALLTYPE D3D11PipelineState::QueryInterface(REFIID riid,
                                                             void** ppvObject) {
    TRACE("D3D11PipelineState::QueryInterface %s, %p", debugstr_guid(&riid).c_str(), ppvObject);

    if (!ppvObject) {
        return E_POINTER;
    }

    if (riid == __uuidof(ID3D12PipelineState) || riid == __uuidof(IUnknown)) {
        AddRef();
        *ppvObject = this;
        return S_OK;
    }

    WARN("Unknown interface %s.", debugstr_guid(&riid).c_str());
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE D3D11PipelineState::AddRef() {
    ULONG ref = InterlockedIncrement(&m_refCount);
    TRACE("%p increasing refcount to %u.", this, ref);
    return ref;
}

ULONG STDMETHODCALLTYPE D3D11PipelineState::Release() {
    ULONG ref = InterlockedDecrement(&m_refCount);
    TRACE("%p decreasing refcount to %u.", this, ref);
    if (ref == 0) {
        delete this;
    }
    return ref;
}

// ID3D12Object methods
HRESULT STDMETHODCALLTYPE D3D11PipelineState::GetPrivateData(REFGUID guid,
                                                             UINT* pDataSize,
                                                             void* pData) {
    TRACE("D3D11PipelineState::GetPrivateData %s, %p, %p", debugstr_guid(&guid).c_str(), pDataSize, pData);
    return m_device->GetPrivateData(guid, pDataSize, pData);
}

HRESULT STDMETHODCALLTYPE D3D11PipelineState::SetPrivateData(
    REFGUID guid, UINT DataSize, const void* pData) {
    TRACE("D3D11PipelineState::SetPrivateData %s, %u, %p", debugstr_guid(&guid).c_str(), DataSize, pData);
    return m_device->SetPrivateData(guid, DataSize, pData);
}

HRESULT STDMETHODCALLTYPE D3D11PipelineState::SetPrivateDataInterface(
    REFGUID guid, const IUnknown* pData) {
    TRACE("D3D11PipelineState::SetPrivateDataInterface %s, %p", debugstr_guid(&guid).c_str(), pData);
    return m_device->SetPrivateDataInterface(guid, pData);
}

HRESULT STDMETHODCALLTYPE D3D11PipelineState::SetName(LPCWSTR Name) {
    TRACE("D3D11PipelineState::SetName %s", debugstr_w(Name).c_str());
    return m_device->SetName(Name);
}

// ID3D12DeviceChild methods
HRESULT STDMETHODCALLTYPE D3D11PipelineState::GetDevice(REFIID riid,
                                                        void** ppvDevice) {
    TRACE("D3D11PipelineState::GetDevice %s, %p", debugstr_guid(&riid).c_str(), ppvDevice);
    return m_device->QueryInterface(riid, ppvDevice);
}

// ID3D12PipelineState methods
HRESULT STDMETHODCALLTYPE D3D11PipelineState::GetCachedBlob(ID3DBlob** ppBlob) {
    TRACE("D3D11PipelineState::GetCachedBlob %p", ppBlob);
    FIXME("We don't implement pipeline state caching yet");
    return E_NOTIMPL;
}

void D3D11PipelineState::Apply(ID3D11DeviceContext* context) {
    TRACE("D3D11PipelineState::Apply");

    if (m_vertexShader) {
        context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    }
    if (m_pixelShader) {
        context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    }
    if (m_streamOutShader) {
        context->GSSetShader(m_streamOutShader.Get(), nullptr, 0);
    } else if (m_geometryShader) {
        context->GSSetShader(m_geometryShader.Get(), nullptr, 0);
    }
    if (m_hullShader) {
        context->HSSetShader(m_hullShader.Get(), nullptr, 0);
    }
    if (m_domainShader) {
        context->DSSetShader(m_domainShader.Get(), nullptr, 0);
    }
    if (m_computeShader) {
        context->CSSetShader(m_computeShader.Get(), nullptr, 0);
    }
    if (m_inputLayout) {
        context->IASetInputLayout(m_inputLayout.Get());
    }
    if (m_blendState) {
        context->OMSetBlendState(m_blendState.Get(), nullptr, 0xffffffff);
    }
    if (m_rasterizerState) {
        context->RSSetState(m_rasterizerState.Get());
    }
    if (m_depthStencilState) {
        context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
    }
}

}  // namespace dxiided
