#pragma once

#include <d3d11.h>
#include <d3d12.h>
#include <wrl/client.h>

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "common/debug.hpp"

namespace dxiided {

class WrappedD3D12ToD3D11Device;

class WrappedD3D12ToD3D11PipelineState final : public ID3D12PipelineState {
   public:
    static HRESULT CreateGraphics(
        WrappedD3D12ToD3D11Device* device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc,
        REFIID riid, void** ppPipelineState);

    static HRESULT CreateCompute(WrappedD3D12ToD3D11Device* device,
                                 const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc,
                                 REFIID riid, void** ppPipelineState);

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

    // ID3D12PipelineState methods
    HRESULT STDMETHODCALLTYPE GetCachedBlob(ID3DBlob** ppBlob) override;

    // Helper methods
    void Apply(ID3D11DeviceContext* context);

    // Pipeline state caching
    struct PipelineStateKey {
        std::vector<uint8_t> hash;
        bool operator==(const PipelineStateKey& other) const {
            return hash == other.hash;
        }
    };

    struct PipelineStateKeyHasher {
        size_t operator()(const PipelineStateKey& key) const {
            return std::hash<std::string>()(
                std::string(reinterpret_cast<const char*>(key.hash.data()),
                            key.hash.size()));
        }
    };

    static Microsoft::WRL::ComPtr<WrappedD3D12ToD3D11PipelineState> GetCachedState(
        const PipelineStateKey& key);
    static void CacheState(const PipelineStateKey& key,
                           Microsoft::WRL::ComPtr<WrappedD3D12ToD3D11PipelineState> state);

   private:
    WrappedD3D12ToD3D11PipelineState(WrappedD3D12ToD3D11Device* device);

    HRESULT InitializeGraphics(const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc);
    HRESULT InitializeCompute(const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc);

    WrappedD3D12ToD3D11Device* const m_device;
    LONG m_refCount{1};

    // Graphics pipeline state
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
    Microsoft::WRL::ComPtr<ID3D11GeometryShader> m_geometryShader;
    Microsoft::WRL::ComPtr<ID3D11HullShader> m_hullShader;
    Microsoft::WRL::ComPtr<ID3D11DomainShader> m_domainShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_blendState;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_rasterizerState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthStencilState;

    // Compute pipeline state
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_computeShader;

    // Stream output
    Microsoft::WRL::ComPtr<ID3D11GeometryShader> m_streamOutShader;
    D3D11_SO_DECLARATION_ENTRY* m_soDeclaration;
    UINT m_numSODeclarations;
    UINT* m_soStrides;
    UINT m_numSOStrides;
    UINT m_rasterizedStream;

    HRESULT CreateStreamOutputShader(const D3D12_STREAM_OUTPUT_DESC* pSODesc,
                                     const void* pShaderBytecode,
                                     SIZE_T BytecodeLength);

    static PipelineStateKey ComputeHash(
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc);
    static PipelineStateKey ComputeHash(
        const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc);

    static std::unordered_map<PipelineStateKey,
                              Microsoft::WRL::ComPtr<WrappedD3D12ToD3D11PipelineState>,
                              PipelineStateKeyHasher>
        s_pipelineStateCache;
    static std::mutex s_cacheMutex;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC m_desc;
};

}  // namespace dxiided
