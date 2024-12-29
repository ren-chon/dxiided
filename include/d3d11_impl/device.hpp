#pragma once

#include <d3d11.h>
#include <d3d11_2.h>
#include <d3d12.h>

#include <atomic>
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include "common/debug.hpp"
#include "d3d11_impl/command_queue.hpp"
#include "d3d11_impl/device_features.hpp"

namespace dxiided {

class WrappedD3D12ToD3D11CommandList;
class WrappedD3D12ToD3D11CommandQueue;

class WrappedD3D12ToD3D11Device final : public ID3D12Device2,
                         public ID3D12DebugDevice,
                         public ID3D11Device2 {
   public:
    static HRESULT Create(IUnknown* adapter,
                          D3D_FEATURE_LEVEL minimum_feature_level, REFIID riid,
                          void** device);

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

    // ID3D12Device methods
    UINT STDMETHODCALLTYPE GetNodeCount() override;
    HRESULT STDMETHODCALLTYPE
    CreateCommandQueue(const D3D12_COMMAND_QUEUE_DESC* pDesc, REFIID riid,
                       void** ppCommandQueue) override;
    HRESULT STDMETHODCALLTYPE
    CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE type, REFIID riid,
                           void** ppCommandAllocator) override;
    HRESULT STDMETHODCALLTYPE
    CreateGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc,
                                REFIID riid, void** ppPipelineState) override;
    HRESULT STDMETHODCALLTYPE
    CreateComputePipelineState(const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc,
                               REFIID riid, void** ppPipelineState) override;
    HRESULT STDMETHODCALLTYPE
    CreateCommandList(UINT nodeMask, D3D12_COMMAND_LIST_TYPE type,
                      ID3D12CommandAllocator* pCommandAllocator,
                      ID3D12PipelineState* pInitialState, REFIID riid,
                      void** ppCommandList) override;
    HRESULT STDMETHODCALLTYPE
    CreateDescriptorHeap(const D3D12_DESCRIPTOR_HEAP_DESC* pDescriptorHeapDesc,
                         REFIID riid, void** ppvHeap) override;
    UINT STDMETHODCALLTYPE GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapType) override;
    HRESULT STDMETHODCALLTYPE
    CheckFeatureSupport(D3D12_FEATURE Feature, void* pFeatureSupportData,
                        UINT FeatureSupportDataSize) override;
    HRESULT STDMETHODCALLTYPE
    CreateRootSignature(UINT nodeMask, const void* pBlobWithRootSignature,
                        SIZE_T blobLengthInBytes, REFIID riid,
                        void** ppvRootSignature) override;
    void STDMETHODCALLTYPE CreateConstantBufferView(
        const D3D12_CONSTANT_BUFFER_VIEW_DESC* pDesc,
        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) override;
    void STDMETHODCALLTYPE CreateShaderResourceView(
        ID3D12Resource* pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC* pDesc,
        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) override;
    void STDMETHODCALLTYPE CreateUnorderedAccessView(
        ID3D12Resource* pResource, ID3D12Resource* pCounterResource,
        const D3D12_UNORDERED_ACCESS_VIEW_DESC* pDesc,
        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) override;
    void STDMETHODCALLTYPE CreateRenderTargetView(
        ID3D12Resource* pResource, const D3D12_RENDER_TARGET_VIEW_DESC* pDesc,
        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) override;
    void STDMETHODCALLTYPE CreateDepthStencilView(
        ID3D12Resource* pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC* pDesc,
        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) override;
    void STDMETHODCALLTYPE
    CreateSampler(const D3D12_SAMPLER_DESC* pDesc,
                  D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) override;
    void STDMETHODCALLTYPE CopyDescriptors(
        UINT NumDestDescriptorRanges,
        const D3D12_CPU_DESCRIPTOR_HANDLE* pDestDescriptorRangeStarts,
        const UINT* pDestDescriptorRangeSizes, UINT NumSrcDescriptorRanges,
        const D3D12_CPU_DESCRIPTOR_HANDLE* pSrcDescriptorRangeStarts,
        const UINT* pSrcDescriptorRangeSizes,
        D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType) override;
    void STDMETHODCALLTYPE CopyDescriptorsSimple(
        UINT NumDescriptors,
        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptorRangeStart,
        D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptorRangeStart,
        D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType) override;

    D3D12_RESOURCE_ALLOCATION_INFO* STDMETHODCALLTYPE GetResourceAllocationInfo(
        D3D12_RESOURCE_ALLOCATION_INFO* info, UINT visibleMask,
        UINT numResourceDescs,
        const D3D12_RESOURCE_DESC* pResourceDescs) override;
    D3D12_HEAP_PROPERTIES* STDMETHODCALLTYPE
    GetCustomHeapProperties(D3D12_HEAP_PROPERTIES* props, UINT nodeMask,
                            D3D12_HEAP_TYPE heapType) override;
    HRESULT STDMETHODCALLTYPE CreateCommittedResource(
        const D3D12_HEAP_PROPERTIES* pHeapProperties,
        D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC* pDesc,
        D3D12_RESOURCE_STATES InitialResourceState,
        const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riidResource,
        void** ppvResource) override;
    HRESULT STDMETHODCALLTYPE CreateHeap(const D3D12_HEAP_DESC* pDesc,
                                         REFIID riid, void** ppvHeap) override;
    HRESULT STDMETHODCALLTYPE CreatePlacedResource(
        ID3D12Heap* pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC* pDesc,
        D3D12_RESOURCE_STATES InitialState,
        const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riid,
        void** ppvResource) override;
    HRESULT STDMETHODCALLTYPE CreateReservedResource(
        const D3D12_RESOURCE_DESC* pDesc, D3D12_RESOURCE_STATES InitialState,
        const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riid,
        void** ppvResource) override;
    HRESULT STDMETHODCALLTYPE CreateSharedHandle(
        ID3D12DeviceChild* pObject, const SECURITY_ATTRIBUTES* pAttributes,
        DWORD Access, LPCWSTR Name, HANDLE* pHandle) override;
    HRESULT STDMETHODCALLTYPE OpenSharedHandle(HANDLE NTHandle, REFIID riid,
                                               void** ppvObj) override;
    HRESULT STDMETHODCALLTYPE OpenSharedHandleByName(
        LPCWSTR Name, DWORD Access, HANDLE* pNTHandle) override;
    HRESULT STDMETHODCALLTYPE
    MakeResident(UINT NumObjects, ID3D12Pageable* const* ppObjects) override;
    HRESULT STDMETHODCALLTYPE Evict(UINT NumObjects,
                                    ID3D12Pageable* const* ppObjects) override;
    HRESULT STDMETHODCALLTYPE CreateFence(UINT64 InitialValue,
                                          D3D12_FENCE_FLAGS Flags, REFIID riid,
                                          void** ppFence) override;
    HRESULT STDMETHODCALLTYPE GetDeviceRemovedReason() override;
    void STDMETHODCALLTYPE GetCopyableFootprints(
        const D3D12_RESOURCE_DESC* pResourceDesc, UINT FirstSubresource,
        UINT NumSubresources, UINT64 BaseOffset,
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pLayouts, UINT* pNumRows,
        UINT64* pRowSizeInBytes, UINT64* pTotalBytes) override;
    HRESULT STDMETHODCALLTYPE
    CreateQueryHeap(const D3D12_QUERY_HEAP_DESC* pDesc, REFIID riid,
                    void** ppvHeap) override;
    HRESULT STDMETHODCALLTYPE SetStablePowerState(BOOL Enable) override;
    HRESULT STDMETHODCALLTYPE
    CreateCommandSignature(const D3D12_COMMAND_SIGNATURE_DESC* pDesc,
                           ID3D12RootSignature* pRootSignature, REFIID riid,
                           void** ppvCommandSignature) override;
    void STDMETHODCALLTYPE GetResourceTiling(
        ID3D12Resource* pTiledResource, UINT* pNumTilesForEntireResource,
        D3D12_PACKED_MIP_INFO* pPackedMipDesc,
        D3D12_TILE_SHAPE* pStandardTileShapeForNonPackedMips,
        UINT* pNumSubresourceTilings, UINT FirstSubresourceTilingToGet,
        D3D12_SUBRESOURCE_TILING* pSubresourceTilingsForNonPackedMips) override;
    LUID* STDMETHODCALLTYPE GetAdapterLuid(LUID* pLuid) override;

    // ID3D12Device1 methods
    HRESULT STDMETHODCALLTYPE SetEventOnMultipleFenceCompletion(
        ID3D12Fence* const* ppFences, const UINT64* pFenceValues,
        UINT NumFences, D3D12_MULTIPLE_FENCE_WAIT_FLAGS Flags,
        HANDLE hEvent) override;

    // ID3D12Device2 methods
    HRESULT STDMETHODCALLTYPE
    CreatePipelineState(const D3D12_PIPELINE_STATE_STREAM_DESC* pDesc,
                        REFIID riid, void** ppPipelineState) override;

    HRESULT STDMETHODCALLTYPE
    CreatePipelineLibrary(const void* pLibraryBlob, SIZE_T BlobLengthInBytes,
                          REFIID riid, void** ppPipelineLibrary) override;

    HRESULT STDMETHODCALLTYPE
    SetResidencyPriority(UINT NumObjects, ID3D12Pageable* const* ppObjects,
                         const D3D12_RESIDENCY_PRIORITY* pPriorities) override;

    // ID3D12DebugDevice methods
    HRESULT STDMETHODCALLTYPE SetFeatureMask(D3D12_DEBUG_FEATURE Mask) override;

    D3D12_DEBUG_FEATURE STDMETHODCALLTYPE GetFeatureMask() override;

    HRESULT STDMETHODCALLTYPE
    ReportLiveDeviceObjects(D3D12_RLDO_FLAGS Flags) override;

    // ID3D11Device methods
    HRESULT STDMETHODCALLTYPE CreateBuffer(const D3D11_BUFFER_DESC* pDesc,
        const D3D11_SUBRESOURCE_DATA* pInitialData,
        ID3D11Buffer** ppBuffer) override;
    HRESULT STDMETHODCALLTYPE CreateTexture1D(const D3D11_TEXTURE1D_DESC* pDesc,
        const D3D11_SUBRESOURCE_DATA* pInitialData,
        ID3D11Texture1D** ppTexture1D) override;
    HRESULT STDMETHODCALLTYPE CreateTexture2D(const D3D11_TEXTURE2D_DESC* pDesc,
        const D3D11_SUBRESOURCE_DATA* pInitialData,
        ID3D11Texture2D** ppTexture2D) override;
    HRESULT STDMETHODCALLTYPE CreateTexture3D(const D3D11_TEXTURE3D_DESC* pDesc,
        const D3D11_SUBRESOURCE_DATA* pInitialData,
        ID3D11Texture3D** ppTexture3D) override;
    HRESULT STDMETHODCALLTYPE CreateShaderResourceView(ID3D11Resource* pResource,
        const D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc,
        ID3D11ShaderResourceView** ppSRView) override;
    HRESULT STDMETHODCALLTYPE CreateUnorderedAccessView(ID3D11Resource* pResource,
        const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc,
        ID3D11UnorderedAccessView** ppUAView) override;
    HRESULT STDMETHODCALLTYPE CreateRenderTargetView(ID3D11Resource* pResource,
        const D3D11_RENDER_TARGET_VIEW_DESC* pDesc,
        ID3D11RenderTargetView** ppRTView) override;
    HRESULT STDMETHODCALLTYPE CreateDepthStencilView(ID3D11Resource* pResource,
        const D3D11_DEPTH_STENCIL_VIEW_DESC* pDesc,
        ID3D11DepthStencilView** ppDepthStencilView) override;
    HRESULT STDMETHODCALLTYPE CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC* pInputElementDescs,
        UINT NumElements, const void* pShaderBytecodeWithInputSignature,
        SIZE_T BytecodeLength, ID3D11InputLayout** ppInputLayout) override;
    HRESULT STDMETHODCALLTYPE CreateVertexShader(const void* pShaderBytecode,
        SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage,
        ID3D11VertexShader** ppVertexShader) override;
    HRESULT STDMETHODCALLTYPE CreateGeometryShader(const void* pShaderBytecode,
        SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage,
        ID3D11GeometryShader** ppGeometryShader) override;
    HRESULT STDMETHODCALLTYPE CreateGeometryShaderWithStreamOutput(const void* pShaderBytecode,
        SIZE_T BytecodeLength, const D3D11_SO_DECLARATION_ENTRY* pSODeclaration,
        UINT NumEntries, const UINT* pBufferStrides, UINT NumStrides,
        UINT RasterizedStream, ID3D11ClassLinkage* pClassLinkage,
        ID3D11GeometryShader** ppGeometryShader) override;
    HRESULT STDMETHODCALLTYPE CreatePixelShader(const void* pShaderBytecode,
        SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage,
        ID3D11PixelShader** ppPixelShader) override;
    HRESULT STDMETHODCALLTYPE CreateHullShader(const void* pShaderBytecode,
        SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage,
        ID3D11HullShader** ppHullShader) override;
    HRESULT STDMETHODCALLTYPE CreateDomainShader(const void* pShaderBytecode,
        SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage,
        ID3D11DomainShader** ppDomainShader) override;
    HRESULT STDMETHODCALLTYPE CreateComputeShader(const void* pShaderBytecode,
        SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage,
        ID3D11ComputeShader** ppComputeShader) override;
    HRESULT STDMETHODCALLTYPE CreateClassLinkage(ID3D11ClassLinkage** ppLinkage) override;
    HRESULT STDMETHODCALLTYPE CreateBlendState(const D3D11_BLEND_DESC* pBlendStateDesc,
        ID3D11BlendState** ppBlendState) override;
    HRESULT STDMETHODCALLTYPE CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC* pDepthStencilDesc,
        ID3D11DepthStencilState** ppDepthStencilState) override;
    HRESULT STDMETHODCALLTYPE CreateRasterizerState(const D3D11_RASTERIZER_DESC* pRasterizerDesc,
        ID3D11RasterizerState** ppRasterizerState) override;
    HRESULT STDMETHODCALLTYPE CreateSamplerState(const D3D11_SAMPLER_DESC* pSamplerDesc,
        ID3D11SamplerState** ppSamplerState) override;
    HRESULT STDMETHODCALLTYPE CreateQuery(const D3D11_QUERY_DESC* pQueryDesc,
        ID3D11Query** ppQuery) override;
    HRESULT STDMETHODCALLTYPE CreatePredicate(const D3D11_QUERY_DESC* pPredicateDesc,
        ID3D11Predicate** ppPredicate) override;
    HRESULT STDMETHODCALLTYPE CreateCounter(const D3D11_COUNTER_DESC* pCounterDesc,
        ID3D11Counter** ppCounter) override;
    HRESULT STDMETHODCALLTYPE CreateDeferredContext(UINT ContextFlags,
        ID3D11DeviceContext** ppDeferredContext) override;
    HRESULT STDMETHODCALLTYPE OpenSharedResource(HANDLE hResource, REFIID ReturnedInterface,
        void** ppResource) override;
    HRESULT STDMETHODCALLTYPE CheckFormatSupport(DXGI_FORMAT Format,
        UINT* pFormatSupport) override;
    HRESULT STDMETHODCALLTYPE CheckMultisampleQualityLevels(DXGI_FORMAT Format,
        UINT SampleCount, UINT* pNumQualityLevels) override;
    void STDMETHODCALLTYPE CheckCounterInfo(D3D11_COUNTER_INFO* pCounterInfo) override;
    HRESULT STDMETHODCALLTYPE CheckCounter(const D3D11_COUNTER_DESC* pDesc,
        D3D11_COUNTER_TYPE* pType, UINT* pActiveCounters, LPSTR szName,
        UINT* pNameLength, LPSTR szUnits, UINT* pUnitsLength,
        LPSTR szDescription, UINT* pDescriptionLength) override;
    HRESULT STDMETHODCALLTYPE CheckFeatureSupport(D3D11_FEATURE Feature,
        void* pFeatureSupportData, UINT FeatureSupportDataSize) override;
    D3D_FEATURE_LEVEL STDMETHODCALLTYPE GetFeatureLevel() override;
    UINT STDMETHODCALLTYPE GetCreationFlags() override;
    void STDMETHODCALLTYPE GetImmediateContext(ID3D11DeviceContext** ppImmediateContext) override;
    HRESULT STDMETHODCALLTYPE SetExceptionMode(UINT RaiseFlags) override;
    UINT STDMETHODCALLTYPE GetExceptionMode() override;

    // ID3D11Device1 methods
    void STDMETHODCALLTYPE GetImmediateContext1(ID3D11DeviceContext1** ppImmediateContext) override;
    HRESULT STDMETHODCALLTYPE CreateDeferredContext1(UINT ContextFlags,
        ID3D11DeviceContext1** ppDeferredContext) override;
    HRESULT STDMETHODCALLTYPE CreateBlendState1(const D3D11_BLEND_DESC1* pBlendStateDesc,
        ID3D11BlendState1** ppBlendState) override;
    HRESULT STDMETHODCALLTYPE CreateRasterizerState1(const D3D11_RASTERIZER_DESC1* pRasterizerDesc,
        ID3D11RasterizerState1** ppRasterizerState) override;
    HRESULT STDMETHODCALLTYPE CreateDeviceContextState(UINT Flags,
        const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels,
        UINT SDKVersion, REFIID EmulatedInterface,
        D3D_FEATURE_LEVEL* pChosenFeatureLevel,
        ID3DDeviceContextState** ppContextState) override;
    HRESULT STDMETHODCALLTYPE OpenSharedResource1(HANDLE hResource,
        REFIID returnedInterface, void** ppResource) override;
    HRESULT STDMETHODCALLTYPE OpenSharedResourceByName(LPCWSTR lpName,
        DWORD dwDesiredAccess, REFIID returnedInterface,
        void** ppResource) override;

    // ID3D11Device2 methods
    void STDMETHODCALLTYPE GetImmediateContext2(ID3D11DeviceContext2** ppImmediateContext) override;
    HRESULT STDMETHODCALLTYPE CreateDeferredContext2(UINT ContextFlags,
        ID3D11DeviceContext2** ppDeferredContext) override;
    void STDMETHODCALLTYPE GetResourceTiling(ID3D11Resource* pTiledResource,
        UINT* pNumTilesForEntireResource,
        D3D11_PACKED_MIP_DESC* pPackedMipDesc,
        D3D11_TILE_SHAPE* pStandardTileShapeForNonPackedMips,
        UINT* pNumSubresourceTilings,
        UINT FirstSubresourceTilingToGet,
        D3D11_SUBRESOURCE_TILING* pSubresourceTilingsForNonPackedMips) override;
    HRESULT STDMETHODCALLTYPE CheckMultisampleQualityLevels1(DXGI_FORMAT Format,
        UINT SampleCount, UINT Flags, UINT* pNumQualityLevels) override;

    // Helper methods
    ID3D11Device* GetD3D11Device() { return m_d3d11Device.Get(); }
    ID3D11DeviceContext* GetD3D11Context() { return m_d3d11Context.Get(); }
    ID3D11Resource* GetD3D11Resource(ID3D12Resource* d3d12Resource);
    ID3D12Resource* GetD3D12Resource(ID3D11Resource* d3d11Resource);
    void StoreD3D11ResourceMapping(ID3D12Resource* d3d12Resource, ID3D11Resource* d3d11Resource);
   private:
    WrappedD3D12ToD3D11Device(Microsoft::WRL::ComPtr<ID3D11Device> device,
                Microsoft::WRL::ComPtr<ID3D11DeviceContext> context,
                D3D_FEATURE_LEVEL feature_level);

    // Internal state
    Microsoft::WRL::ComPtr<ID3D11Device> m_d3d11Device;
    Microsoft::WRL::ComPtr<ID3D11Device1> m_d3d11Device1;
    Microsoft::WRL::ComPtr<ID3D11Device2> m_d3d11Device2;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_d3d11Context;
    D3D_FEATURE_LEVEL m_featureLevel;
    LONG m_refCount{1};

    // Resource tracking
    std::unordered_map<ID3D12Resource*, Microsoft::WRL::ComPtr<ID3D11Resource>>
        m_resourceMap;
    std::vector<std::unique_ptr<WrappedD3D12ToD3D11CommandQueue>> m_commandQueues;

    // Resource mapping
    std::mutex m_resourceMappingMutex;
    std::unordered_map<ID3D12Resource*, ID3D11Resource*> m_d3d12ToD3d11Resources;
    std::unordered_map<ID3D11Resource*, ID3D12Resource*> m_d3d11ToD3d12Resources;
};

}  // namespace dxiided
