#include "d3d11_impl/command_list.hpp"

#include "common/debug.hpp"
#include "common/debug_symbols.hpp"
#include "d3d11_impl/device.hpp"

namespace dxiided {

HRESULT D3D11CommandList::Create(D3D11Device* device,
                                 D3D12_COMMAND_LIST_TYPE type,
                                 ID3D12CommandAllocator* allocator,
                                 ID3D12PipelineState* initial_state,
                                 REFIID riid, void** command_list) {
    if (!device || !command_list) {
        return E_INVALIDARG;
    }

    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    device->GetD3D11Device()->CreateDeferredContext(0, &context);
    if (!context) {
        ERR("Failed to create D3D11 deferred context.");
        return E_FAIL;
    }

    Microsoft::WRL::ComPtr<D3D11CommandList> d3d12_command_list =
        new D3D11CommandList(device, type, context);

    return d3d12_command_list.CopyTo(
        reinterpret_cast<ID3D12GraphicsCommandList**>(command_list));
}

D3D11CommandList::D3D11CommandList(
    D3D11Device* device, D3D12_COMMAND_LIST_TYPE type,
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
    : m_device(device), m_type(type), m_context(context) {
    TRACE("Created D3D11CommandList type %d.", type);
}

// IUnknown methods
HRESULT STDMETHODCALLTYPE D3D11CommandList::QueryInterface(REFIID riid,
                                                           void** ppvObject) {
                                                    
    if (!ppvObject) {
        return E_POINTER;
    }

    if (riid == __uuidof(ID3D12GraphicsCommandList) ||
        riid == __uuidof(ID3D12CommandList) || riid == __uuidof(IUnknown)) {
        AddRef();
        *ppvObject = this;
        return S_OK;
    }

    WARN("Unknown interface %s.", debugstr_guid(&riid).c_str());
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE D3D11CommandList::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

ULONG STDMETHODCALLTYPE D3D11CommandList::Release() {
    ULONG ref = InterlockedDecrement(&m_refCount);
    if (ref == 0) {
        delete this;
    }
    return ref;
}

// ID3D12Object methods
HRESULT STDMETHODCALLTYPE D3D11CommandList::GetPrivateData(REFGUID guid,
                                                           UINT* pDataSize,
                                                           void* pData) {
    return m_context->GetPrivateData(guid, pDataSize, pData);
}

HRESULT STDMETHODCALLTYPE D3D11CommandList::SetPrivateData(REFGUID guid,
                                                           UINT DataSize,
                                                           const void* pData) {
    return m_context->SetPrivateData(guid, DataSize, pData);
}

HRESULT STDMETHODCALLTYPE
D3D11CommandList::SetPrivateDataInterface(REFGUID guid, const IUnknown* pData) {
    return m_context->SetPrivateDataInterface(guid, pData);
}

HRESULT STDMETHODCALLTYPE D3D11CommandList::SetName(LPCWSTR Name) {
    return m_context->SetPrivateData(
        WKPDID_D3DDebugObjectName,
        static_cast<UINT>((wcslen(Name) + 1) * sizeof(WCHAR)), Name);
}

// ID3D12DeviceChild methods
HRESULT STDMETHODCALLTYPE D3D11CommandList::GetDevice(REFIID riid,
                                                      void** ppvDevice) {
    return m_device->QueryInterface(riid, ppvDevice);
}

// ID3D12CommandList methods
D3D12_COMMAND_LIST_TYPE STDMETHODCALLTYPE D3D11CommandList::GetType() {
    return m_type;
}

// ID3D12GraphicsCommandList methods
HRESULT STDMETHODCALLTYPE D3D11CommandList::Close() {
  if (!m_isOpen) {
        WARN("Command list is already closed.");
        return E_FAIL;
    }

    // Get the D3D11 command list from the context
    HRESULT hr = m_context->FinishCommandList(FALSE, &m_d3d11CommandList);
    if (FAILED(hr)) {
        ERR("Failed to finish D3D11 command list.");
        return hr;
    }

    m_isOpen = false;
    return S_OK;
}

HRESULT D3D11CommandList::Reset(ID3D12CommandAllocator* pAllocator,
                                ID3D12PipelineState* pInitialState) {
    TRACE("(%p, %p)", pAllocator, pInitialState);

    // Clear any existing command list
    if (m_deferred) {
        m_deferred.Reset();
    }

    // Clear the context state and prepare for new commands
    m_context->ClearState();
    m_isOpen = true;
    return S_OK;
}

void D3D11CommandList::CopyResource(ID3D12Resource* pDstResource,
                                    ID3D12Resource* pSrcResource) {
    TRACE("(%p, %p)", pDstResource, pSrcResource);
    // TODO: Implement resource copying
}

void D3D11CommandList::CopyTiles(
    ID3D12Resource* pTiledResource,
    const D3D12_TILED_RESOURCE_COORDINATE* pTileRegionStartCoordinate,
    const D3D12_TILE_REGION_SIZE* pTileRegionSize, ID3D12Resource* pBuffer,
    UINT64 BufferStartOffsetInBytes, D3D12_TILE_COPY_FLAGS Flags) {
    TRACE("(%p, %p, %p, %p, %llu, %d)", pTiledResource,
          pTileRegionStartCoordinate, pTileRegionSize, pBuffer,
          BufferStartOffsetInBytes, Flags);
    // TODO: Implement tile copying
}

void D3D11CommandList::ResolveSubresource(ID3D12Resource* pDstResource,
                                          UINT DstSubresource,
                                          ID3D12Resource* pSrcResource,
                                          UINT SrcSubresource,
                                          DXGI_FORMAT Format) {
    TRACE("(%p, %u, %p, %u, %d)", pDstResource, DstSubresource, pSrcResource,
          SrcSubresource, Format);
    // TODO: Implement subresource resolution
}

void D3D11CommandList::IASetPrimitiveTopology(
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology) {
    TRACE("(%d)", PrimitiveTopology);
    m_context->IASetPrimitiveTopology(
        static_cast<D3D11_PRIMITIVE_TOPOLOGY>(PrimitiveTopology));
}

void D3D11CommandList::RSSetViewports(UINT NumViewports,
                                      const D3D12_VIEWPORT* pViewports) {
    TRACE("(%u, %p)", NumViewports, pViewports);
    m_context->RSSetViewports(
        NumViewports, reinterpret_cast<const D3D11_VIEWPORT*>(pViewports));
}

void D3D11CommandList::RSSetScissorRects(UINT NumRects,
                                         const D3D12_RECT* pRects) {
    TRACE("(%u, %p)", NumRects, pRects);
    m_context->RSSetScissorRects(NumRects, pRects);
}

void D3D11CommandList::OMSetBlendFactor(const FLOAT BlendFactor[4]) {
    TRACE("(%p)", BlendFactor);
    float currentBlendFactor[4];
    UINT SampleMask;
    ID3D11BlendState* blendState;
    m_context->OMGetBlendState(&blendState, currentBlendFactor, &SampleMask);
    m_context->OMSetBlendState(blendState, BlendFactor, SampleMask);
    if (blendState) blendState->Release();
}

void D3D11CommandList::OMSetStencilRef(UINT StencilRef) {
    TRACE("D3D11CommandList::OMSetStencilRef(%u)", StencilRef);
    ID3D11DepthStencilState* dsState;
    UINT currentRef;
    m_context->OMGetDepthStencilState(&dsState, &currentRef);
    m_context->OMSetDepthStencilState(dsState, StencilRef);
    if (dsState) dsState->Release();
}

void D3D11CommandList::SetPipelineState(ID3D12PipelineState* pPipelineState) {
    TRACE("D3D11CommandList::SetPipelineState(%p)", pPipelineState);
    // TODO: Implement pipeline state setting
}

void D3D11CommandList::ExecuteBundle(ID3D12GraphicsCommandList* pCommandList) {
    TRACE("D3D11CommandList::ExecuteBundle(%p)", pCommandList);
    // TODO: Implement bundle execution
}

void D3D11CommandList::SetDescriptorHeaps(
    UINT NumDescriptorHeaps, ID3D12DescriptorHeap* const* ppDescriptorHeaps) {
    TRACE("D3D11CommandList::SetDescriptorHeaps(%u, %p)", NumDescriptorHeaps, ppDescriptorHeaps);
    // TODO: Implement descriptor heap setting
}

void D3D11CommandList::SetComputeRootSignature(
    ID3D12RootSignature* pRootSignature) {
    TRACE("D3D11CommandList::SetComputeRootSignature(%p)", pRootSignature);
    // TODO: Implement compute root signature setting
}

void D3D11CommandList::SetGraphicsRootSignature(
    ID3D12RootSignature* pRootSignature) {
    TRACE("D3D11CommandList::SetGraphicsRootSignature(%p)", pRootSignature);
    // TODO: Implement graphics root signature setting
}

void D3D11CommandList::SetComputeRootDescriptorTable(
    UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor) {
    TRACE("D3D11CommandList::SetComputeRootDescriptorTable(%u, %llu)", RootParameterIndex, BaseDescriptor.ptr);
    // TODO: Implement compute root descriptor table setting
}

void D3D11CommandList::SetGraphicsRootDescriptorTable(
    UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor) {
    TRACE("D3D11CommandList::SetGraphicsRootDescriptorTable(%u, %llu)", RootParameterIndex, BaseDescriptor.ptr);
    // TODO: Implement graphics root descriptor table setting
}

void D3D11CommandList::SetComputeRoot32BitConstant(
    UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues) {
    TRACE("D3D11CommandList::SetComputeRoot32BitConstant(%u, %u, %u)", RootParameterIndex, SrcData,
          DestOffsetIn32BitValues);
    // TODO: Implement compute root constant setting
}

void D3D11CommandList::SetGraphicsRoot32BitConstant(
    UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues) {
    TRACE("D3D11CommandList::SetGraphicsRoot32BitConstant(%u, %u, %u)", RootParameterIndex, SrcData,
          DestOffsetIn32BitValues);
    // TODO: Implement graphics root constant setting
}

void D3D11CommandList::SetComputeRoot32BitConstants(
    UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData,
    UINT DestOffsetIn32BitValues) {
    TRACE("D3D11CommandList::SetComputeRoot32BitConstants(%u, %u, %p, %u)", RootParameterIndex, Num32BitValuesToSet,
          pSrcData, DestOffsetIn32BitValues);
    // TODO: Implement compute root constants setting
}

void D3D11CommandList::SetGraphicsRoot32BitConstants(
    UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData,
    UINT DestOffsetIn32BitValues) {
    TRACE("D3D11CommandList::SetGraphicsRoot32BitConstants(%u, %u, %p, %u)", RootParameterIndex, Num32BitValuesToSet,
          pSrcData, DestOffsetIn32BitValues);
    // TODO: Implement graphics root constants setting
}

void D3D11CommandList::SetComputeRootConstantBufferView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
    TRACE("D3D11CommandList::SetComputeRootConstantBufferView(%u, %llu)", RootParameterIndex, BufferLocation);
    // TODO: Implement compute root constant buffer view setting
}

void D3D11CommandList::SetGraphicsRootConstantBufferView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
    TRACE("D3D11CommandList::SetGraphicsRootConstantBufferView(%u, %llu)", RootParameterIndex, BufferLocation);
    // TODO: Implement graphics root constant buffer view setting
}

void D3D11CommandList::SetComputeRootShaderResourceView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
    TRACE("D3D11CommandList::SetComputeRootShaderResourceView(%u, %llu)", RootParameterIndex, BufferLocation);
    // TODO: Implement compute root shader resource view setting
}

void D3D11CommandList::SetGraphicsRootShaderResourceView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
    TRACE("D3D11CommandList::SetGraphicsRootShaderResourceView(%u, %llu)", RootParameterIndex, BufferLocation);
    // TODO: Implement graphics root shader resource view setting
}

void D3D11CommandList::SetComputeRootUnorderedAccessView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
    TRACE("D3D11CommandList::SetComputeRootUnorderedAccessView(%u, %llu)", RootParameterIndex, BufferLocation);
    // TODO: Implement compute root unordered access view setting
}

void D3D11CommandList::SetGraphicsRootUnorderedAccessView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
    TRACE("D3D11CommandList::SetGraphicsRootUnorderedAccessView(%u, %llu)", RootParameterIndex, BufferLocation);
    // TODO: Implement graphics root unordered access view setting
}

void D3D11CommandList::IASetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW* pView) {
    TRACE("D3D11CommandList::IASetIndexBuffer(%p)", pView);
    if (pView) {
        D3D11_BUFFER_DESC desc = {};
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        desc.ByteWidth = pView->SizeInBytes;
        desc.Usage = D3D11_USAGE_DEFAULT;

        // TODO: Create and set index buffer
    } else {
        m_context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    }
}

void D3D11CommandList::IASetVertexBuffers(
    UINT StartSlot, UINT NumViews, const D3D12_VERTEX_BUFFER_VIEW* pViews) {
    TRACE("D3D11CommandList::IASetVertexBuffers(%u, %u, %p)", StartSlot, NumViews, pViews);
    // TODO: Implement vertex buffer setting
}

void D3D11CommandList::SOSetTargets(
    UINT StartSlot, UINT NumViews,
    const D3D12_STREAM_OUTPUT_BUFFER_VIEW* pViews) {
    TRACE("D3D11CommandList::SOSetTargets(%u, %u, %p)", StartSlot, NumViews, pViews);
    // TODO: Implement stream output target setting
}

void D3D11CommandList::OMSetRenderTargets(
    UINT NumRenderTargetDescriptors,
    const D3D12_CPU_DESCRIPTOR_HANDLE* pRenderTargetDescriptors,
    BOOL RTsSingleHandleToDescriptorRange,
    const D3D12_CPU_DESCRIPTOR_HANDLE* pDepthStencilDescriptor) {
    TRACE("D3D11CommandList::OMSetRenderTargets(%u, %p, %d, %p)", NumRenderTargetDescriptors,
          pRenderTargetDescriptors, RTsSingleHandleToDescriptorRange,
          pDepthStencilDescriptor);
    // TODO: Implement render target setting
}

void D3D11CommandList::ClearDepthStencilView(
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView, D3D12_CLEAR_FLAGS ClearFlags,
    FLOAT Depth, UINT8 Stencil, UINT NumRects, const D3D12_RECT* pRects) {
    TRACE("D3D11CommandList::ClearDepthStencilView(%llu, %d, %f, %u, %u, %p)", DepthStencilView.ptr, ClearFlags,
          Depth, Stencil, NumRects, pRects);
    // TODO: Implement depth stencil view clearing
}

void D3D11CommandList::ClearRenderTargetView(
    D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const FLOAT ColorRGBA[4],
    UINT NumRects, const D3D12_RECT* pRects) {
    TRACE("D3D11CommandList::ClearRenderTargetView(%llu, %p, %u, %p)", RenderTargetView.ptr, ColorRGBA, NumRects,
          pRects);
    // TODO: Implement render target view clearing
}

void D3D11CommandList::ClearUnorderedAccessViewUint(
    D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap,
    D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource* pResource,
    const UINT Values[4], UINT NumRects, const D3D12_RECT* pRects) {
    TRACE("D3D11CommandList::ClearUnorderedAccessViewUint(%llu, %llu, %p, %p, %u, %p)", ViewGPUHandleInCurrentHeap.ptr,
          ViewCPUHandle.ptr, pResource, Values, NumRects, pRects);
    // TODO: Implement UAV clearing (uint)
}

void D3D11CommandList::ClearUnorderedAccessViewFloat(
    D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap,
    D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource* pResource,
    const FLOAT Values[4], UINT NumRects, const D3D12_RECT* pRects) {
    TRACE("D3D11CommandList::ClearUnorderedAccessViewFloat(%llu, %llu, %p, %p, %u, %p)", ViewGPUHandleInCurrentHeap.ptr,
          ViewCPUHandle.ptr, pResource, Values, NumRects, pRects);
    // TODO: Implement UAV clearing (float)
}

void D3D11CommandList::DiscardResource(ID3D12Resource* pResource,
                                       const D3D12_DISCARD_REGION* pRegion) {
    TRACE("D3D11CommandList::DiscardResource(%p, %p)", pResource, pRegion);
    // TODO: Implement resource discarding
}

void D3D11CommandList::BeginQuery(ID3D12QueryHeap* pQueryHeap,
                                  D3D12_QUERY_TYPE Type, UINT Index) {
    TRACE("D3D11CommandList::BeginQuery(%p, %d, %u)", pQueryHeap, Type, Index);
    // TODO: Implement query beginning
}

void D3D11CommandList::EndQuery(ID3D12QueryHeap* pQueryHeap,
                                D3D12_QUERY_TYPE Type, UINT Index) {
    TRACE("D3D11CommandList::EndQuery(%p, %d, %u)", pQueryHeap, Type, Index);
    // TODO: Implement query ending
}

void D3D11CommandList::ResolveQueryData(ID3D12QueryHeap* pQueryHeap,
                                        D3D12_QUERY_TYPE Type, UINT StartIndex,
                                        UINT NumQueries,
                                        ID3D12Resource* pDestinationBuffer,
                                        UINT64 AlignedDestinationBufferOffset) {
    TRACE("D3D11CommandList::ResolveQueryData(%p, %d, %u, %u, %p, %llu)", pQueryHeap, Type, StartIndex,
          NumQueries, pDestinationBuffer, AlignedDestinationBufferOffset);
    // TODO: Implement query data resolution
}

void D3D11CommandList::SetPredication(ID3D12Resource* pBuffer,
                                      UINT64 AlignedBufferOffset,
                                      D3D12_PREDICATION_OP Operation) {
    TRACE("D3D11CommandList::SetPredication(%p, %llu, %d)", pBuffer, AlignedBufferOffset, Operation);
    // TODO: Implement predication setting
}

void D3D11CommandList::SetMarker(UINT Metadata, const void* pData, UINT Size) {
    TRACE("D3D11CommandList::SetMarker(%u, %p, %u)", Metadata, pData, Size);
    // TODO: Implement marker setting
}

void D3D11CommandList::BeginEvent(UINT Metadata, const void* pData, UINT Size) {
    TRACE("D3D11CommandList(%u, %p, %u)", Metadata, pData, Size);
    // TODO: Implement event beginning
}

void D3D11CommandList::EndEvent() {
    TRACE("D3D11CommandList()");
    // TODO: Implement event ending
}

void D3D11CommandList::ExecuteIndirect(
    ID3D12CommandSignature* pCommandSignature, UINT MaxCommandCount,
    ID3D12Resource* pArgumentBuffer, UINT64 ArgumentBufferOffset,
    ID3D12Resource* pCountBuffer, UINT64 CountBufferOffset) {
    TRACE("(%p, %u, %p, %llu, %p, %llu)", pCommandSignature, MaxCommandCount,
          pArgumentBuffer, ArgumentBufferOffset, pCountBuffer,
          CountBufferOffset);
    // TODO: Implement indirect execution
}

void STDMETHODCALLTYPE
D3D11CommandList::ClearState(ID3D12PipelineState* pPipelineState) {
    TRACE("D3D11CommandList::ClearState(%p)", pPipelineState);

    m_context->ClearState();
}

void STDMETHODCALLTYPE D3D11CommandList::DrawInstanced(
    UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation,
    UINT StartInstanceLocation) {
    TRACE("D3D11CommandList::DrawInstanced: %u, %u, %u, %u", VertexCountPerInstance,
          InstanceCount, StartVertexLocation, StartInstanceLocation);
    m_context->DrawInstanced(VertexCountPerInstance, InstanceCount,
                             StartVertexLocation, StartInstanceLocation);
}

void STDMETHODCALLTYPE D3D11CommandList::DrawIndexedInstanced(
    UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation,
    INT BaseVertexLocation, UINT StartInstanceLocation) {
    TRACE("DrawIndexedInstanced: %u, %u, %u, %d, %u", IndexCountPerInstance,
          InstanceCount, StartIndexLocation, BaseVertexLocation,
          StartInstanceLocation);
    m_context->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount,
                                    StartIndexLocation, BaseVertexLocation,
                                    StartInstanceLocation);
}

void STDMETHODCALLTYPE D3D11CommandList::Dispatch(UINT ThreadGroupCountX,
                                                  UINT ThreadGroupCountY,
                                                  UINT ThreadGroupCountZ) {
    TRACE("D3D11CommandList::Dispatch: %u, %u, %u", ThreadGroupCountX, ThreadGroupCountY,
          ThreadGroupCountZ);
    m_context->Dispatch(ThreadGroupCountX, ThreadGroupCountY,
                        ThreadGroupCountZ);
}

void STDMETHODCALLTYPE D3D11CommandList::CopyBufferRegion(
    ID3D12Resource* pDstBuffer, UINT64 DstOffset, ID3D12Resource* pSrcBuffer,
    UINT64 SrcOffset, UINT64 NumBytes) {
    TRACE("D3D11CommandList::CopyBufferRegion: %p, %llu, %p, %llu, %llu", pDstBuffer, DstOffset,
          pSrcBuffer, SrcOffset, NumBytes);
    // TODO: Implement buffer copy
    FIXME("Buffer copy not implemented yet.");
}

void STDMETHODCALLTYPE D3D11CommandList::CopyTextureRegion(
    const D3D12_TEXTURE_COPY_LOCATION* pDst, UINT DstX, UINT DstY, UINT DstZ,
    const D3D12_TEXTURE_COPY_LOCATION* pSrc, const D3D12_BOX* pSrcBox) {
    TRACE("D3D11CommandList::CopyTextureRegion: %p, %u, %u, %u, %p, %p", pDst, DstX, DstY, DstZ,
          pSrc, pSrcBox);
    // TODO: Implement texture copy
    FIXME("Texture copy not implemented yet.");
}

void STDMETHODCALLTYPE D3D11CommandList::ResourceBarrier(
    UINT NumBarriers, const D3D12_RESOURCE_BARRIER* pBarriers) {
    TRACE("D3D11CommandList::ResourceBarrier: %u, %p", NumBarriers, pBarriers);
    // D3D11 handles resource states automatically, so we can ignore barriers
    TRACE("Ignoring %u resource barriers.", NumBarriers);
}

HRESULT D3D11CommandList::GetD3D11CommandList(ID3D11CommandList** ppCommandList) {
    TRACE("D3D11CommandList::GetD3D11CommandList(%p)", ppCommandList);
    if (!ppCommandList) {
        return E_POINTER;
    }

    if (!m_d3d11CommandList) {
        // If we haven't finished the command list yet, finish it now
        if (m_isOpen) {
            HRESULT hr = Close();
            if (FAILED(hr)) {
                return hr;
            }
        }

        // Get the D3D11 command list from the context
        HRESULT hr = m_context->FinishCommandList(FALSE, &m_d3d11CommandList);
        if (FAILED(hr)) {
            ERR("Failed to finish D3D11 command list.");
            return hr;
        }
    }

    m_d3d11CommandList.CopyTo(ppCommandList);
    return S_OK;
}

}  // namespace dxiided
