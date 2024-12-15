#pragma once

#include <atomic>
#include <d3d11.h>
#include <d3d12.h>
#include <memory>
#include <unordered_map>
#include <vector>

#include "common/debug.hpp"
#include "d3d11_impl/command_queue.hpp"
#include "d3d11_impl/device_features.hpp"

namespace dxiided {

class D3D11CommandList;
class D3D11CommandQueue;

// We only need to inherit from ID3D12Device2 since it already inherits from ID3D12Device1 and ID3D12Device
class D3D11Device final : public ID3D12Device2, public ID3D12DebugDevice {
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
    HRESULT STDMETHODCALLTYPE CheckFeatureSupport(
        D3D12_FEATURE Feature, void* pFeatureSupportData,
        UINT FeatureSupportDataSize) override;
    HRESULT STDMETHODCALLTYPE CreateRootSignature(
        UINT nodeMask, const void* pBlobWithRootSignature,
        SIZE_T blobLengthInBytes, REFIID riid, void** ppvRootSignature) override;
    void STDMETHODCALLTYPE CreateConstantBufferView(
        const D3D12_CONSTANT_BUFFER_VIEW_DESC* pDesc,
        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) override;
    void STDMETHODCALLTYPE CreateShaderResourceView(
        ID3D12Resource* pResource,
        const D3D12_SHADER_RESOURCE_VIEW_DESC* pDesc,
        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) override;
    void STDMETHODCALLTYPE CreateUnorderedAccessView(
        ID3D12Resource* pResource, ID3D12Resource* pCounterResource,
        const D3D12_UNORDERED_ACCESS_VIEW_DESC* pDesc,
        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) override;
    void STDMETHODCALLTYPE CreateRenderTargetView(
        ID3D12Resource* pResource,
        const D3D12_RENDER_TARGET_VIEW_DESC* pDesc,
        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) override;
    void STDMETHODCALLTYPE CreateDepthStencilView(
        ID3D12Resource* pResource,
        const D3D12_DEPTH_STENCIL_VIEW_DESC* pDesc,
        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) override;
    void STDMETHODCALLTYPE CreateSampler(
        const D3D12_SAMPLER_DESC* pDesc,
        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor) override;
    void STDMETHODCALLTYPE CopyDescriptors(
        UINT NumDestDescriptorRanges,
        const D3D12_CPU_DESCRIPTOR_HANDLE* pDestDescriptorRangeStarts,
        const UINT* pDestDescriptorRangeSizes,
        UINT NumSrcDescriptorRanges,
        const D3D12_CPU_DESCRIPTOR_HANDLE* pSrcDescriptorRangeStarts,
        const UINT* pSrcDescriptorRangeSizes,
        D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType) override;
    void STDMETHODCALLTYPE CopyDescriptorsSimple(
        UINT NumDescriptors,
        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptorRangeStart,
        D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptorRangeStart,
        D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType) override;

    D3D12_RESOURCE_ALLOCATION_INFO* STDMETHODCALLTYPE GetResourceAllocationInfo(
        D3D12_RESOURCE_ALLOCATION_INFO* info,
        UINT visibleMask,
        UINT numResourceDescs,
        const D3D12_RESOURCE_DESC* pResourceDescs) override;
    D3D12_HEAP_PROPERTIES* STDMETHODCALLTYPE GetCustomHeapProperties(
        D3D12_HEAP_PROPERTIES* props,
        UINT nodeMask,
        D3D12_HEAP_TYPE heapType) override;
    HRESULT STDMETHODCALLTYPE CreateCommittedResource(
        const D3D12_HEAP_PROPERTIES* pHeapProperties,
        D3D12_HEAP_FLAGS HeapFlags,
        const D3D12_RESOURCE_DESC* pDesc,
        D3D12_RESOURCE_STATES InitialResourceState,
        const D3D12_CLEAR_VALUE* pOptimizedClearValue,
        REFIID riidResource,
        void** ppvResource) override;
    HRESULT STDMETHODCALLTYPE CreateHeap(
        const D3D12_HEAP_DESC* pDesc,
        REFIID riid,
        void** ppvHeap) override;
    HRESULT STDMETHODCALLTYPE CreatePlacedResource(
        ID3D12Heap* pHeap,
        UINT64 HeapOffset,
        const D3D12_RESOURCE_DESC* pDesc,
        D3D12_RESOURCE_STATES InitialState,
        const D3D12_CLEAR_VALUE* pOptimizedClearValue,
        REFIID riid,
        void** ppvResource) override;
    HRESULT STDMETHODCALLTYPE CreateReservedResource(
        const D3D12_RESOURCE_DESC* pDesc,
        D3D12_RESOURCE_STATES InitialState,
        const D3D12_CLEAR_VALUE* pOptimizedClearValue,
        REFIID riid,
        void** ppvResource) override;
    HRESULT STDMETHODCALLTYPE CreateSharedHandle(
        ID3D12DeviceChild* pObject,
        const SECURITY_ATTRIBUTES* pAttributes,
        DWORD Access,
        LPCWSTR Name,
        HANDLE* pHandle) override;
    HRESULT STDMETHODCALLTYPE OpenSharedHandle(
        HANDLE NTHandle,
        REFIID riid,
        void** ppvObj) override;
    HRESULT STDMETHODCALLTYPE OpenSharedHandleByName(
        LPCWSTR Name,
        DWORD Access,
        HANDLE* pNTHandle) override;
    HRESULT STDMETHODCALLTYPE MakeResident(
        UINT NumObjects,
        ID3D12Pageable* const* ppObjects) override;
    HRESULT STDMETHODCALLTYPE Evict(
        UINT NumObjects,
        ID3D12Pageable* const* ppObjects) override;
    HRESULT STDMETHODCALLTYPE CreateFence(
        UINT64 InitialValue,
        D3D12_FENCE_FLAGS Flags,
        REFIID riid,
        void** ppFence) override;
    HRESULT STDMETHODCALLTYPE GetDeviceRemovedReason() override;
    void STDMETHODCALLTYPE GetCopyableFootprints(
        const D3D12_RESOURCE_DESC* pResourceDesc,
        UINT FirstSubresource,
        UINT NumSubresources,
        UINT64 BaseOffset,
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pLayouts,
        UINT* pNumRows,
        UINT64* pRowSizeInBytes,
        UINT64* pTotalBytes) override;
    HRESULT STDMETHODCALLTYPE CreateQueryHeap(
        const D3D12_QUERY_HEAP_DESC* pDesc,
        REFIID riid,
        void** ppvHeap) override;
    HRESULT STDMETHODCALLTYPE SetStablePowerState(
        BOOL Enable) override;
    HRESULT STDMETHODCALLTYPE CreateCommandSignature(
        const D3D12_COMMAND_SIGNATURE_DESC* pDesc,
        ID3D12RootSignature* pRootSignature,
        REFIID riid,
        void** ppvCommandSignature) override;
    void STDMETHODCALLTYPE GetResourceTiling(
        ID3D12Resource* pTiledResource,
        UINT* pNumTilesForEntireResource,
        D3D12_PACKED_MIP_INFO* pPackedMipDesc,
        D3D12_TILE_SHAPE* pStandardTileShapeForNonPackedMips,
        UINT* pNumSubresourceTilings,
        UINT FirstSubresourceTilingToGet,
        D3D12_SUBRESOURCE_TILING* pSubresourceTilingsForNonPackedMips) override;
    LUID* STDMETHODCALLTYPE GetAdapterLuid(LUID* pLuid) override;

    // ID3D12Device1 methods
    HRESULT STDMETHODCALLTYPE SetEventOnMultipleFenceCompletion(
        ID3D12Fence* const* ppFences,
        const UINT64* pFenceValues,
        UINT NumFences,
        D3D12_MULTIPLE_FENCE_WAIT_FLAGS Flags,
        HANDLE hEvent) override;

    // ID3D12Device2 methods
    HRESULT STDMETHODCALLTYPE CreatePipelineState(
        const D3D12_PIPELINE_STATE_STREAM_DESC* pDesc,
        REFIID riid,
        void** ppPipelineState) override;

    HRESULT STDMETHODCALLTYPE CreatePipelineLibrary(
        const void* pLibraryBlob,
        SIZE_T BlobLengthInBytes,
        REFIID riid,
        void** ppPipelineLibrary) override;

    HRESULT STDMETHODCALLTYPE SetResidencyPriority(
        UINT NumObjects,
        ID3D12Pageable* const* ppObjects,
        const D3D12_RESIDENCY_PRIORITY* pPriorities) override;

    // ID3D12DebugDevice methods
    HRESULT STDMETHODCALLTYPE SetFeatureMask(
        D3D12_DEBUG_FEATURE Mask) override;

    D3D12_DEBUG_FEATURE STDMETHODCALLTYPE GetFeatureMask() override;

    HRESULT STDMETHODCALLTYPE ReportLiveDeviceObjects(
        D3D12_RLDO_FLAGS Flags) override;

    // Helper methods
    ID3D11Device* GetD3D11Device() { return m_d3d11Device.Get(); }
    ID3D11DeviceContext* GetD3D11Context() { return m_d3d11Context.Get(); }

   private:
    D3D11Device(Microsoft::WRL::ComPtr<ID3D11Device> device,
                Microsoft::WRL::ComPtr<ID3D11DeviceContext> context,
                D3D_FEATURE_LEVEL feature_level);

    // Internal state
    Microsoft::WRL::ComPtr<ID3D11Device> m_d3d11Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_d3d11Context;
    D3D_FEATURE_LEVEL m_featureLevel;
    LONG m_refCount{1};

    // Resource tracking
    std::unordered_map<ID3D12Resource*, Microsoft::WRL::ComPtr<ID3D11Resource>>
        m_resourceMap;
    std::vector<std::unique_ptr<D3D11CommandQueue>> m_commandQueues;
};

}  // namespace dxiided
