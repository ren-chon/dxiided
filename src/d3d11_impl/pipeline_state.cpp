#include "d3d11_impl/pipeline_state.hpp"

#include "d3d11_impl/device.hpp"
#include "d3d11_impl/shader_library.hpp"
#include <cmath>
namespace dxiided {

// Static member initialization
std::unordered_map<WrappedD3D12ToD3D11PipelineState::PipelineStateKey,
                   Microsoft::WRL::ComPtr<WrappedD3D12ToD3D11PipelineState>,
                   WrappedD3D12ToD3D11PipelineState::PipelineStateKeyHasher>
    WrappedD3D12ToD3D11PipelineState::s_pipelineStateCache;
std::mutex WrappedD3D12ToD3D11PipelineState::s_cacheMutex;

HRESULT WrappedD3D12ToD3D11PipelineState::CreateGraphics(
    WrappedD3D12ToD3D11Device* device,
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc, REFIID riid,
    void** ppPipelineState) {
    TRACE("WrappedD3D12ToD3D11PipelineState::CreateGraphics %p, %p, %s, %p",
          device, pDesc, debugstr_guid(&riid).c_str(), ppPipelineState);

    if (!device || !pDesc || !ppPipelineState) {
        return E_INVALIDARG;
    }

    // Try to find cached state
    PipelineStateKey key = ComputeHash(pDesc);
    Microsoft::WRL::ComPtr<WrappedD3D12ToD3D11PipelineState> cachedState =
        GetCachedState(key);
    if (cachedState) {
        return cachedState.CopyTo(
            reinterpret_cast<ID3D12PipelineState**>(ppPipelineState));
    }

    // Create new state
    Microsoft::WRL::ComPtr<WrappedD3D12ToD3D11PipelineState> state =
        new WrappedD3D12ToD3D11PipelineState(device);
    HRESULT hr = state->InitializeGraphics(pDesc);
    if (FAILED(hr)) {
        return hr;
    }

    // Cache the new state
    CacheState(key, state);

    return state.CopyTo(
        reinterpret_cast<ID3D12PipelineState**>(ppPipelineState));
}

HRESULT WrappedD3D12ToD3D11PipelineState::CreateCompute(
    WrappedD3D12ToD3D11Device* device,
    const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc, REFIID riid,
    void** ppPipelineState) {
    TRACE("WrappedD3D12ToD3D11PipelineState::CreateCompute %p, %p, %s, %p",
          device, pDesc, debugstr_guid(&riid).c_str(), ppPipelineState);

    if (!device || !pDesc || !ppPipelineState) {
        return E_INVALIDARG;
    }

    // Try to find cached state
    PipelineStateKey key = ComputeHash(pDesc);
    Microsoft::WRL::ComPtr<WrappedD3D12ToD3D11PipelineState> cachedState =
        GetCachedState(key);
    if (cachedState) {
        return cachedState.CopyTo(
            reinterpret_cast<ID3D12PipelineState**>(ppPipelineState));
    }

    // Create new state
    Microsoft::WRL::ComPtr<WrappedD3D12ToD3D11PipelineState> state =
        new WrappedD3D12ToD3D11PipelineState(device);
    HRESULT hr = state->InitializeCompute(pDesc);
    if (FAILED(hr)) {
        return hr;
    }

    // Cache the new state
    CacheState(key, state);

    return state.CopyTo(
        reinterpret_cast<ID3D12PipelineState**>(ppPipelineState));
}

Microsoft::WRL::ComPtr<WrappedD3D12ToD3D11PipelineState>
WrappedD3D12ToD3D11PipelineState::GetCachedState(const PipelineStateKey& key) {
    TRACE("WrappedD3D12ToD3D11PipelineState::GetCachedState");
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    auto it = s_pipelineStateCache.find(key);
    if (it != s_pipelineStateCache.end()) {
        return it->second;
    }
    return nullptr;
}

void WrappedD3D12ToD3D11PipelineState::CacheState(
    const PipelineStateKey& key,
    Microsoft::WRL::ComPtr<WrappedD3D12ToD3D11PipelineState> state) {
    TRACE("WrappedD3D12ToD3D11PipelineState::CacheState");
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    s_pipelineStateCache[key] = state;
}

WrappedD3D12ToD3D11PipelineState::PipelineStateKey
WrappedD3D12ToD3D11PipelineState::ComputeHash(
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc) {
    TRACE("WrappedD3D12ToD3D11PipelineState::ComputeHash");
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

WrappedD3D12ToD3D11PipelineState::PipelineStateKey
WrappedD3D12ToD3D11PipelineState::ComputeHash(
    const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc) {
    TRACE("WrappedD3D12ToD3D11PipelineState::ComputeHash");
    PipelineStateKey key;

    // For compute pipeline, we only need to hash the compute shader
    key.hash.insert(
        key.hash.end(), reinterpret_cast<const uint8_t*>(&pDesc->CS),
        reinterpret_cast<const uint8_t*>(&pDesc->CS) + sizeof(pDesc->CS));

    return key;
}

WrappedD3D12ToD3D11PipelineState::WrappedD3D12ToD3D11PipelineState(
    WrappedD3D12ToD3D11Device* device)
    : m_device(device) {
    TRACE(
        "WrappedD3D12ToD3D11PipelineState::WrappedD3D12ToD3D11PipelineState %p",
        device);
}

HRESULT WrappedD3D12ToD3D11PipelineState::InitializeGraphics(
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc) {
    TRACE("Initializing graphics pipeline state");
    if (!pDesc) {
        ERR("Null pipeline state descriptor");
        return E_INVALIDARG;
    }
    TRACE("Initializing graphics pipeline state");
    TRACE("VS BytecodeLength: %zu", pDesc->VS.BytecodeLength);
    TRACE("PS BytecodeLength: %zu", pDesc->PS.BytecodeLength);
    TRACE("NumRenderTargets: %u", pDesc->NumRenderTargets);
    // Store original desc for debugging
    m_desc = *pDesc;

    // Verify D3D11 device is valid
    ID3D11Device* d3d11Device = m_device->GetD3D11Device();
    if (!d3d11Device) {
        ERR("D3D11 device is null");
        return E_FAIL;
    }
    // Create vertex shader if provided
    if (pDesc->VS.BytecodeLength > 0) {
        if (pDesc->VS.BytecodeLength == 1) {
            // Special built-in shader
            uint64_t specialValue =
                reinterpret_cast<uint64_t>(pDesc->VS.pShaderBytecode);
            m_vertexShader = D3D11ShaderLibrary::GetBuiltinVertexShader(
                m_device->GetD3D11Device(), specialValue);
            if (!m_vertexShader) {
                ERR("Failed to create built-in vertex shader");
                return E_FAIL;
            }
        } else {
            // Regular shader
            HRESULT hrV = m_device->GetD3D11Device()->CreateVertexShader(
                pDesc->VS.pShaderBytecode, pDesc->VS.BytecodeLength, nullptr,
                &m_vertexShader);
            if (FAILED(hrV)) {
                ERR("Failed to create vertex shader, hrV=%08x", hrV);
                return hrV;
            }
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
        TRACE("Creating pixel shader with bytecode length %zu",
              pDesc->PS.BytecodeLength);
        // Log first few bytes of shader bytecode for debugging
        const uint32_t* dwordData =
            static_cast<const uint32_t*>(pDesc->PS.pShaderBytecode);
        if (pDesc->PS.BytecodeLength >= 4) {
            TRACE("Shader bytecode header: %08x", dwordData[0]);
        }
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
        TRACE("Creating geometry shader with bytecode length %zu",
              pDesc->GS.BytecodeLength);
        // Log first few bytes of shader bytecode for debugging
        const uint32_t* dwordData =
            static_cast<const uint32_t*>(pDesc->GS.pShaderBytecode);
        if (pDesc->GS.BytecodeLength >= 4) {
            TRACE("Shader bytecode header: %08x", dwordData[0]);
        }
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
        TRACE("Creating hull shader with bytecode length %zu",
              pDesc->HS.BytecodeLength);
        // Log first few bytes of shader bytecode for debugging
        const uint32_t* dwordData =
            static_cast<const uint32_t*>(pDesc->HS.pShaderBytecode);
        if (pDesc->HS.BytecodeLength >= 4) {
            TRACE("Shader bytecode header: %08x", dwordData[0]);
        }
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
        TRACE("Creating domain shader with bytecode length %zu",
              pDesc->DS.BytecodeLength);
        // Log first few bytes of shader bytecode for debugging
        const uint32_t* dwordData =
            static_cast<const uint32_t*>(pDesc->DS.pShaderBytecode);
        if (pDesc->DS.BytecodeLength >= 4) {
            TRACE("Shader bytecode header: %08x", dwordData[0]);
        }
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
    // First add validation for the blend state desc
    if (pDesc->BlendState.IndependentBlendEnable > 1) {
        WARN("Invalid IndependentBlendEnable value: %d, defaulting to FALSE",
             pDesc->BlendState.IndependentBlendEnable);

        // Create a default blend state desc
        D3D11_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;

        // Set up default blend state for the first render target
        blendDesc.RenderTarget[0].BlendEnable = FALSE;
        blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask =
            D3D11_COLOR_WRITE_ENABLE_ALL;

        HRESULT hr = m_device->GetD3D11Device()->CreateBlendState(
            &blendDesc, &m_blendState);
        if (FAILED(hr)) {
            ERR("Failed to create default blend state, hr %#x", hr);
            return hr;
        }

        TRACE("Created default blend state due to invalid input parameters");
    } else {
        // Convert D3D12 blend desc to D3D11
        D3D11_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable =
            pDesc->BlendState.AlphaToCoverageEnable;
        blendDesc.IndependentBlendEnable =
            pDesc->BlendState.IndependentBlendEnable;

        // Copy render target blend states
        for (UINT i = 0; i < 8; i++) {
            auto& rt = blendDesc.RenderTarget[i];
            auto& d3d12_rt = pDesc->BlendState.RenderTarget[i];

            rt.BlendEnable = d3d12_rt.BlendEnable;
            rt.SrcBlend = static_cast<D3D11_BLEND>(d3d12_rt.SrcBlend);
            rt.DestBlend = static_cast<D3D11_BLEND>(d3d12_rt.DestBlend);
            rt.BlendOp = static_cast<D3D11_BLEND_OP>(d3d12_rt.BlendOp);
            rt.SrcBlendAlpha = static_cast<D3D11_BLEND>(d3d12_rt.SrcBlendAlpha);
            rt.DestBlendAlpha =
                static_cast<D3D11_BLEND>(d3d12_rt.DestBlendAlpha);
            rt.BlendOpAlpha =
                static_cast<D3D11_BLEND_OP>(d3d12_rt.BlendOpAlpha);
            rt.RenderTargetWriteMask = d3d12_rt.RenderTargetWriteMask;
        }

        HRESULT hr = m_device->GetD3D11Device()->CreateBlendState(
            &blendDesc, &m_blendState);
        if (FAILED(hr)) {
            ERR("Failed to create blend state from D3D12 desc, hr %#x", hr);
            return hr;
        }

        TRACE("Successfully created blend state from D3D12 desc");
    }

    // Create rasterizer state
    D3D11_RASTERIZER_DESC rasterizerDesc = {};
    
    // Validate and convert FillMode
    auto fillMode = static_cast<D3D11_FILL_MODE>(pDesc->RasterizerState.FillMode);
    if (fillMode != D3D11_FILL_WIREFRAME && fillMode != D3D11_FILL_SOLID) {
        WARN("Invalid FillMode value: %d, defaulting to SOLID", fillMode);
        rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    } else {
        rasterizerDesc.FillMode = fillMode;
    }

    // Validate and convert CullMode
    auto cullMode = static_cast<D3D11_CULL_MODE>(pDesc->RasterizerState.CullMode);
    if (cullMode != D3D11_CULL_NONE && cullMode != D3D11_CULL_FRONT && cullMode != D3D11_CULL_BACK) {
        WARN("Invalid CullMode value: %d, defaulting to BACK", cullMode);
        rasterizerDesc.CullMode = D3D11_CULL_BACK;
    } else {
        rasterizerDesc.CullMode = cullMode;
    }

    rasterizerDesc.FrontCounterClockwise = pDesc->RasterizerState.FrontCounterClockwise ? TRUE : FALSE;
    rasterizerDesc.DepthBias = pDesc->RasterizerState.DepthBias;
    rasterizerDesc.DepthBiasClamp = pDesc->RasterizerState.DepthBiasClamp;
    rasterizerDesc.SlopeScaledDepthBias = pDesc->RasterizerState.SlopeScaledDepthBias;
    rasterizerDesc.DepthClipEnable = pDesc->RasterizerState.DepthClipEnable ? TRUE : FALSE;
    rasterizerDesc.ScissorEnable = TRUE;  // D3D12 always enables scissor
    rasterizerDesc.MultisampleEnable = pDesc->RasterizerState.MultisampleEnable ? TRUE : FALSE;
    rasterizerDesc.AntialiasedLineEnable = pDesc->RasterizerState.AntialiasedLineEnable ? TRUE : FALSE;

    // Clamp depth bias values to valid ranges
    if (!std::isfinite(rasterizerDesc.DepthBiasClamp)) {
        WARN("Invalid DepthBiasClamp value, defaulting to 0.0f");
        rasterizerDesc.DepthBiasClamp = 0.0f;
    }
    
    if (!std::isfinite(rasterizerDesc.SlopeScaledDepthBias)) {
        WARN("Invalid SlopeScaledDepthBias value, defaulting to 0.0f");
        rasterizerDesc.SlopeScaledDepthBias = 0.0f;
    }

    HRESULT hrRS = m_device->GetD3D11Device()->CreateRasterizerState(&rasterizerDesc, &m_rasterizerState);
    if (FAILED(hrRS)) {
        ERR("Failed to create rasterizer state, hr %#x.", hrRS);
        return hrRS;
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

    HRESULT hrDSS = m_device->GetD3D11Device()->CreateDepthStencilState(
        &depthStencilDesc, &m_depthStencilState);
    if (FAILED(hrDSS)) {
        ERR("Failed to create depth-stencil state, hrDSS %#x.", hrDSS);
        return hrDSS;
    }

    TRACE("Graphics pipeline state initialized successfully");
    return S_OK;
}

HRESULT WrappedD3D12ToD3D11PipelineState::CreateStreamOutputShader(
    const D3D12_STREAM_OUTPUT_DESC* pSODesc, const void* pShaderBytecode,
    SIZE_T BytecodeLength) {
    TRACE(
        "WrappedD3D12ToD3D11PipelineState::CreateStreamOutputShader: Creating "
        "stream output shader");
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

HRESULT WrappedD3D12ToD3D11PipelineState::InitializeCompute(
    const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc) {
    TRACE("Initializing compute pipeline state");

    if (!pDesc->CS.pShaderBytecode || !pDesc->CS.BytecodeLength) {
        ERR("No compute shader provided.");
        return E_INVALIDARG;
    }

    TRACE("Creating compute shader with bytecode length %zu",
          pDesc->CS.BytecodeLength);
    // Log first few bytes of shader bytecode for debugging
    const uint32_t* dwordData =
        static_cast<const uint32_t*>(pDesc->CS.pShaderBytecode);
    if (pDesc->CS.BytecodeLength >= 4) {
        TRACE("Shader bytecode header: %08x", dwordData[0]);
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
HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11PipelineState::QueryInterface(
    REFIID riid, void** ppvObject) {
    TRACE("WrappedD3D12ToD3D11PipelineState::QueryInterface %s, %p",
          debugstr_guid(&riid).c_str(), ppvObject);

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

ULONG STDMETHODCALLTYPE WrappedD3D12ToD3D11PipelineState::AddRef() {
    ULONG ref = InterlockedIncrement(&m_refCount);
    TRACE("%p increasing refcount to %u.", this, ref);
    return ref;
}

ULONG STDMETHODCALLTYPE WrappedD3D12ToD3D11PipelineState::Release() {
    ULONG ref = InterlockedDecrement(&m_refCount);
    TRACE("%p decreasing refcount to %u.", this, ref);
    if (ref == 0) {
        delete this;
    }
    return ref;
}

// ID3D12Object methods
HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11PipelineState::GetPrivateData(
    REFGUID guid, UINT* pDataSize, void* pData) {
    TRACE("WrappedD3D12ToD3D11PipelineState::GetPrivateData %s, %p, %p",
          debugstr_guid(&guid).c_str(), pDataSize, pData);
    return m_device->GetPrivateData(guid, pDataSize, pData);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11PipelineState::SetPrivateData(
    REFGUID guid, UINT DataSize, const void* pData) {
    TRACE("WrappedD3D12ToD3D11PipelineState::SetPrivateData %s, %u, %p",
          debugstr_guid(&guid).c_str(), DataSize, pData);
    return m_device->SetPrivateData(guid, DataSize, pData);
}

HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11PipelineState::SetPrivateDataInterface(
    REFGUID guid, const IUnknown* pData) {
    TRACE("WrappedD3D12ToD3D11PipelineState::SetPrivateDataInterface %s, %p",
          debugstr_guid(&guid).c_str(), pData);
    return m_device->SetPrivateDataInterface(guid, pData);
}

HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11PipelineState::SetName(LPCWSTR Name) {
    TRACE("WrappedD3D12ToD3D11PipelineState::SetName %s",
          debugstr_w(Name).c_str());
    return m_device->SetName(Name);
}

// ID3D12DeviceChild methods
HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11PipelineState::GetDevice(REFIID riid, void** ppvDevice) {
    TRACE("WrappedD3D12ToD3D11PipelineState::GetDevice %s, %p",
          debugstr_guid(&riid).c_str(), ppvDevice);
    return m_device->QueryInterface(riid, ppvDevice);
}

// ID3D12PipelineState methods
HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11PipelineState::GetCachedBlob(ID3DBlob** ppBlob) {
    TRACE("WrappedD3D12ToD3D11PipelineState::GetCachedBlob %p", ppBlob);
    FIXME("We don't implement pipeline state caching yet");
    return E_NOTIMPL;
}

void WrappedD3D12ToD3D11PipelineState::Apply(ID3D11DeviceContext* context) {
    TRACE("WrappedD3D12ToD3D11PipelineState::Apply");
    TRACE("WrappedD3D12ToD3D11PipelineState::Apply");

    if (!context) {
        ERR("Null context passed to Apply()");
        return;
    }

    // Vertex Shader Stage
    TRACE("Setting vertex shader state");
    context->VSSetShader(m_vertexShader.Get(), nullptr, 0);

    // Pixel Shader Stage
    TRACE("Setting pixel shader state");
    context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

    // Geometry/Stream-Out Stage
    TRACE("Setting geometry shader state");
    if (m_streamOutShader) {
        context->GSSetShader(m_streamOutShader.Get(), nullptr, 0);
    } else if (m_geometryShader) {
        context->GSSetShader(m_geometryShader.Get(), nullptr, 0);
    } else {
        // Explicitly clear GS stage if no shader is set
        context->GSSetShader(nullptr, nullptr, 0);
    }

    // Hull Shader Stage
    TRACE("Setting hull shader state");
    context->HSSetShader(m_hullShader.Get(), nullptr, 0);

    // Domain Shader Stage
    TRACE("Setting domain shader state");
    context->DSSetShader(m_domainShader.Get(), nullptr, 0);

    // Compute Shader Stage
    TRACE("Setting compute shader state");
    context->CSSetShader(m_computeShader.Get(), nullptr, 0);

    // Input Assembly Stage
    TRACE("Setting input layout");
    if (m_inputLayout) {
        context->IASetInputLayout(m_inputLayout.Get());
    } else {
        // For fullscreen triangle, we don't need input layout
        context->IASetInputLayout(nullptr);
    }

    // Output Merger Stage
    TRACE("Setting blend state");
    if (m_blendState) {
        float blendFactor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        context->OMSetBlendState(m_blendState.Get(), blendFactor, 0xffffffff);
    } else {
        context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    }

    // Rasterizer Stage
    TRACE("Setting rasterizer state");
    if (m_rasterizerState) {
        context->RSSetState(m_rasterizerState.Get());
    } else {
        context->RSSetState(nullptr);
    }

    // Depth-Stencil Stage
    TRACE("Setting depth-stencil state");
    if (m_depthStencilState) {
        context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
    } else {
        context->OMSetDepthStencilState(nullptr, 0);
    }

    TRACE("Pipeline state applied successfully");
}

}  // namespace dxiided
