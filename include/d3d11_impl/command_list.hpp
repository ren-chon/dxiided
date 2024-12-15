#pragma once

#include <d3d11.h>
#include <d3d12.h>
#include <wrl/client.h>

#include <memory>
#include <vector>

#include "common/debug.hpp"

namespace dxiided {

class D3D11Device;

class D3D11CommandList final : public ID3D12GraphicsCommandList {
   public:
    static HRESULT Create(D3D11Device* device, D3D12_COMMAND_LIST_TYPE type,
                          ID3D12CommandAllocator* allocator,
                          ID3D12PipelineState* initial_state, REFIID riid,
                          void** command_list);

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

    // ID3D12CommandList methods
    D3D12_COMMAND_LIST_TYPE STDMETHODCALLTYPE GetType() override;

    // ID3D12GraphicsCommandList methods
    HRESULT STDMETHODCALLTYPE Close() override;
    HRESULT STDMETHODCALLTYPE
    Reset(ID3D12CommandAllocator* pAllocator,
          ID3D12PipelineState* pInitialState) override;
    void STDMETHODCALLTYPE
    ClearState(ID3D12PipelineState* pPipelineState) override;
    void STDMETHODCALLTYPE DrawInstanced(UINT VertexCountPerInstance,
                                         UINT InstanceCount,
                                         UINT StartVertexLocation,
                                         UINT StartInstanceLocation) override;
    void STDMETHODCALLTYPE DrawIndexedInstanced(
        UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation,
        INT BaseVertexLocation, UINT StartInstanceLocation) override;
    void STDMETHODCALLTYPE Dispatch(UINT ThreadGroupCountX,
                                    UINT ThreadGroupCountY,
                                    UINT ThreadGroupCountZ) override;
    void STDMETHODCALLTYPE CopyBufferRegion(ID3D12Resource* pDstBuffer,
                                            UINT64 DstOffset,
                                            ID3D12Resource* pSrcBuffer,
                                            UINT64 SrcOffset,
                                            UINT64 NumBytes) override;
    void STDMETHODCALLTYPE CopyTextureRegion(
        const D3D12_TEXTURE_COPY_LOCATION* pDst, UINT DstX, UINT DstY,
        UINT DstZ, const D3D12_TEXTURE_COPY_LOCATION* pSrc,
        const D3D12_BOX* pSrcBox) override;
    void STDMETHODCALLTYPE ResourceBarrier(
        UINT NumBarriers, const D3D12_RESOURCE_BARRIER* pBarriers) override;

    // Required D3D12 interface methods
    void STDMETHODCALLTYPE CopyResource(ID3D12Resource* pDstResource,
                                        ID3D12Resource* pSrcResource) override;

    void STDMETHODCALLTYPE CopyTiles(
        ID3D12Resource* pTiledResource,
        const D3D12_TILED_RESOURCE_COORDINATE* pTileRegionStartCoordinate,
        const D3D12_TILE_REGION_SIZE* pTileRegionSize, ID3D12Resource* pBuffer,
        UINT64 BufferStartOffsetInBytes, D3D12_TILE_COPY_FLAGS Flags) override;

    void STDMETHODCALLTYPE ResolveSubresource(ID3D12Resource* pDstResource,
                                              UINT DstSubresource,
                                              ID3D12Resource* pSrcResource,
                                              UINT SrcSubresource,
                                              DXGI_FORMAT Format) override;

    void STDMETHODCALLTYPE
    IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology) override;

    void STDMETHODCALLTYPE RSSetViewports(
        UINT NumViewports, const D3D12_VIEWPORT* pViewports) override;

    void STDMETHODCALLTYPE RSSetScissorRects(UINT NumRects,
                                             const D3D12_RECT* pRects) override;

    void STDMETHODCALLTYPE
    OMSetBlendFactor(const FLOAT BlendFactor[4]) override;

    void STDMETHODCALLTYPE OMSetStencilRef(UINT StencilRef) override;

    void STDMETHODCALLTYPE
    SetPipelineState(ID3D12PipelineState* pPipelineState) override;

    void STDMETHODCALLTYPE
    ExecuteBundle(ID3D12GraphicsCommandList* pCommandList) override;

    void STDMETHODCALLTYPE
    SetDescriptorHeaps(UINT NumDescriptorHeaps,
                       ID3D12DescriptorHeap* const* ppDescriptorHeaps) override;

    void STDMETHODCALLTYPE
    SetComputeRootSignature(ID3D12RootSignature* pRootSignature) override;

    void STDMETHODCALLTYPE
    SetGraphicsRootSignature(ID3D12RootSignature* pRootSignature) override;

    void STDMETHODCALLTYPE SetComputeRootDescriptorTable(
        UINT RootParameterIndex,
        D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor) override;

    void STDMETHODCALLTYPE SetGraphicsRootDescriptorTable(
        UINT RootParameterIndex,
        D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor) override;

    void STDMETHODCALLTYPE
    SetComputeRoot32BitConstant(UINT RootParameterIndex, UINT SrcData,
                                UINT DestOffsetIn32BitValues) override;

    void STDMETHODCALLTYPE
    SetGraphicsRoot32BitConstant(UINT RootParameterIndex, UINT SrcData,
                                 UINT DestOffsetIn32BitValues) override;

    void STDMETHODCALLTYPE SetComputeRoot32BitConstants(
        UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData,
        UINT DestOffsetIn32BitValues) override;

    void STDMETHODCALLTYPE SetGraphicsRoot32BitConstants(
        UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData,
        UINT DestOffsetIn32BitValues) override;

    void STDMETHODCALLTYPE SetComputeRootConstantBufferView(
        UINT RootParameterIndex,
        D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) override;

    void STDMETHODCALLTYPE SetGraphicsRootConstantBufferView(
        UINT RootParameterIndex,
        D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) override;

    void STDMETHODCALLTYPE SetComputeRootShaderResourceView(
        UINT RootParameterIndex,
        D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) override;

    void STDMETHODCALLTYPE SetGraphicsRootShaderResourceView(
        UINT RootParameterIndex,
        D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) override;

    void STDMETHODCALLTYPE SetComputeRootUnorderedAccessView(
        UINT RootParameterIndex,
        D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) override;

    void STDMETHODCALLTYPE SetGraphicsRootUnorderedAccessView(
        UINT RootParameterIndex,
        D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) override;

    void STDMETHODCALLTYPE
    IASetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW* pView) override;

    void STDMETHODCALLTYPE
    IASetVertexBuffers(UINT StartSlot, UINT NumViews,
                       const D3D12_VERTEX_BUFFER_VIEW* pViews) override;

    void STDMETHODCALLTYPE
    SOSetTargets(UINT StartSlot, UINT NumViews,
                 const D3D12_STREAM_OUTPUT_BUFFER_VIEW* pViews) override;

    void STDMETHODCALLTYPE OMSetRenderTargets(
        UINT NumRenderTargetDescriptors,
        const D3D12_CPU_DESCRIPTOR_HANDLE* pRenderTargetDescriptors,
        BOOL RTsSingleHandleToDescriptorRange,
        const D3D12_CPU_DESCRIPTOR_HANDLE* pDepthStencilDescriptor) override;

    void STDMETHODCALLTYPE ClearDepthStencilView(
        D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView,
        D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth, UINT8 Stencil, UINT NumRects,
        const D3D12_RECT* pRects) override;

    void STDMETHODCALLTYPE ClearRenderTargetView(
        D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const FLOAT ColorRGBA[4],
        UINT NumRects, const D3D12_RECT* pRects) override;

    void STDMETHODCALLTYPE ClearUnorderedAccessViewUint(
        D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap,
        D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource* pResource,
        const UINT Values[4], UINT NumRects, const D3D12_RECT* pRects) override;

    void STDMETHODCALLTYPE ClearUnorderedAccessViewFloat(
        D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap,
        D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource* pResource,
        const FLOAT Values[4], UINT NumRects,
        const D3D12_RECT* pRects) override;

    void STDMETHODCALLTYPE
    DiscardResource(ID3D12Resource* pResource,
                    const D3D12_DISCARD_REGION* pRegion) override;

    void STDMETHODCALLTYPE BeginQuery(ID3D12QueryHeap* pQueryHeap,
                                      D3D12_QUERY_TYPE Type,
                                      UINT Index) override;

    void STDMETHODCALLTYPE EndQuery(ID3D12QueryHeap* pQueryHeap,
                                    D3D12_QUERY_TYPE Type, UINT Index) override;

    void STDMETHODCALLTYPE ResolveQueryData(
        ID3D12QueryHeap* pQueryHeap, D3D12_QUERY_TYPE Type, UINT StartIndex,
        UINT NumQueries, ID3D12Resource* pDestinationBuffer,
        UINT64 AlignedDestinationBufferOffset) override;

    void STDMETHODCALLTYPE
    SetPredication(ID3D12Resource* pBuffer, UINT64 AlignedBufferOffset,
                   D3D12_PREDICATION_OP Operation) override;

    void STDMETHODCALLTYPE SetMarker(UINT Metadata, const void* pData,
                                     UINT Size) override;

    void STDMETHODCALLTYPE BeginEvent(UINT Metadata, const void* pData,
                                      UINT Size) override;

    void STDMETHODCALLTYPE EndEvent() override;

    void STDMETHODCALLTYPE ExecuteIndirect(
        ID3D12CommandSignature* pCommandSignature, UINT MaxCommandCount,
        ID3D12Resource* pArgumentBuffer, UINT64 ArgumentBufferOffset,
        ID3D12Resource* pCountBuffer, UINT64 CountBufferOffset) override;

   private:
    D3D11CommandList(D3D11Device* device, D3D12_COMMAND_LIST_TYPE type,
                     Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);

    D3D11Device* m_device;
    D3D12_COMMAND_LIST_TYPE m_type;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    LONG m_refCount{1};
    bool m_isOpen{true};
    Microsoft::WRL::ComPtr<ID3D11CommandList> m_deferred;
};

}  // namespace dxiided
