#include "d3d11_impl/command_list.hpp"
#include "d3d11_impl/pipeline_state.hpp"
#include "d3d11_impl/resource.hpp"

#include "common/debug.hpp"
#include "common/debug_symbols.hpp"
#include "d3d11_impl/device.hpp"

namespace dxiided {

HRESULT WrappedD3D12ToD3D11CommandList::Create(WrappedD3D12ToD3D11Device* device,
                                 D3D12_COMMAND_LIST_TYPE type,
                                 ID3D12CommandAllocator* allocator,
                                 ID3D12PipelineState* initial_state,
                                 REFIID riid, void** command_list) {
        TRACE("WrappedD3D12ToD3D11CommandList::Create called");
    if (!device || !command_list) {
        ERR("WrappedD3D12ToD3D11CommandList::Create: Invalid parameters.");
        return E_INVALIDARG;
    }

    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    device->GetD3D11Device()->CreateDeferredContext(0, &context);
    if (!context) {
        ERR("Failed to create D3D11 deferred context.");
        return E_FAIL;
    }

    Microsoft::WRL::ComPtr<WrappedD3D12ToD3D11CommandList> d3d12_command_list =
        new WrappedD3D12ToD3D11CommandList(device, type, context);

    return d3d12_command_list.CopyTo(
        reinterpret_cast<ID3D12GraphicsCommandList**>(command_list));
}

WrappedD3D12ToD3D11CommandList::WrappedD3D12ToD3D11CommandList(
    WrappedD3D12ToD3D11Device* device, D3D12_COMMAND_LIST_TYPE type,
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
    : m_device(device), m_type(type), m_context(context) {
    TRACE("Created WrappedD3D12ToD3D11CommandList type %d.", type);
}

// IUnknown methods
HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11CommandList::QueryInterface(REFIID riid,
                                                           void** ppvObject) {
    TRACE("WrappedD3D12ToD3D11CommandList::QueryInterface called for %s, %p",
          debugstr_guid(&riid).c_str(), ppvObject);
                                                    
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

ULONG STDMETHODCALLTYPE WrappedD3D12ToD3D11CommandList::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

ULONG STDMETHODCALLTYPE WrappedD3D12ToD3D11CommandList::Release() {
    ULONG ref = InterlockedDecrement(&m_refCount);
    if (ref == 0) {
        delete this;
    }
    return ref;
}

// ID3D12Object methods
HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11CommandList::GetPrivateData(REFGUID guid,
                                                           UINT* pDataSize,
                                                           void* pData) {
    return m_context->GetPrivateData(guid, pDataSize, pData);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11CommandList::SetPrivateData(REFGUID guid,
                                                           UINT DataSize,
                                                           const void* pData) {
    return m_context->SetPrivateData(guid, DataSize, pData);
}

HRESULT STDMETHODCALLTYPE
WrappedD3D12ToD3D11CommandList::SetPrivateDataInterface(REFGUID guid, const IUnknown* pData) {
    return m_context->SetPrivateDataInterface(guid, pData);
}

HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11CommandList::SetName(LPCWSTR Name) {
    TRACE("WrappedD3D12ToD3D11CommandList::SetName");
    return m_context->SetPrivateData(
        WKPDID_D3DDebugObjectName,
        static_cast<UINT>((wcslen(Name) + 1) * sizeof(WCHAR)), Name);
}

// ID3D12DeviceChild methods
HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11CommandList::GetDevice(REFIID riid,
                                                      void** ppvDevice) {
                                                        TRACE("WrappedD3D12ToD3D11CommandList::GetDevice");
    return m_device->QueryInterface(riid, ppvDevice);
}

// ID3D12CommandList methods
D3D12_COMMAND_LIST_TYPE STDMETHODCALLTYPE WrappedD3D12ToD3D11CommandList::GetType() {
    TRACE("WrappedD3D12ToD3D11CommandList::GetType");
    return m_type;
}

// ID3D12GraphicsCommandList methods
HRESULT STDMETHODCALLTYPE WrappedD3D12ToD3D11CommandList::Close() {
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

HRESULT WrappedD3D12ToD3D11CommandList::Reset(ID3D12CommandAllocator* pAllocator,
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

void WrappedD3D12ToD3D11CommandList::CopyResource(ID3D12Resource* pDstResource,
                                    ID3D12Resource* pSrcResource) {
    TRACE("CopyResource: %p -> %p", pSrcResource, pDstResource);
    
    // Get the underlying D3D11 resources
    ID3D11Resource* d3d11SrcResource = nullptr;
    ID3D11Resource* d3d11DstResource = nullptr;
    
    if (!pSrcResource || !pDstResource) {
        ERR("Invalid source or destination resource");
        return;
    }

    HRESULT hr = pSrcResource->QueryInterface(__uuidof(ID3D11Resource), (void**)&d3d11SrcResource);
    if (FAILED(hr)) {
        ERR("Failed to get D3D11 source resource");
        return;
    }

    hr = pDstResource->QueryInterface(__uuidof(ID3D11Resource), (void**)&d3d11DstResource);
    if (FAILED(hr)) {
        d3d11SrcResource->Release();
        ERR("Failed to get D3D11 destination resource");
        return;
    }

    // Perform the copy
    m_context->CopyResource(d3d11DstResource, d3d11SrcResource);

    // Clean up
    d3d11SrcResource->Release();
    d3d11DstResource->Release();
}

void WrappedD3D12ToD3D11CommandList::CopyBufferRegion(
    ID3D12Resource* pDstBuffer, UINT64 DstOffset, ID3D12Resource* pSrcBuffer,
    UINT64 SrcOffset, UINT64 NumBytes) {
    TRACE("CopyBufferRegion: %p[%llu] -> %p[%llu], size=%llu", pSrcBuffer, SrcOffset,
          pDstBuffer, DstOffset, NumBytes);

    // Get D3D11 buffers
    Microsoft::WRL::ComPtr<ID3D11Buffer> d3d11DstBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> d3d11SrcBuffer;
    
    // Try to get the D3D11 buffers, handling both direct buffers and wrapped resources
    if (FAILED(GetD3D11Buffer(pDstBuffer, &d3d11DstBuffer))) {
        ERR("Failed to get D3D11 destination buffer");
        return;
    }

    // For source buffer, try both ID3D11Buffer and ID3D11Resource
    Microsoft::WRL::ComPtr<ID3D11Resource> d3d11SrcResource;
    if (FAILED(GetD3D11Resource(pSrcBuffer, &d3d11SrcResource))) {
        ERR("Failed to get D3D11 source resource");
        return;
    }

    if (FAILED(d3d11SrcResource.As(&d3d11SrcBuffer))) {
        ERR("Source resource is not a buffer");
        return;
    }

    // Get buffer descriptions
    D3D11_BUFFER_DESC srcDesc = {}, dstDesc = {};
    d3d11SrcBuffer->GetDesc(&srcDesc);
    d3d11DstBuffer->GetDesc(&dstDesc);
    
    TRACE("  Source buffer: size=%u, usage=%d, bind=%#x", 
          srcDesc.ByteWidth, srcDesc.Usage, srcDesc.BindFlags);
    TRACE("  Dest buffer: size=%u, usage=%d, bind=%#x", 
          dstDesc.ByteWidth, dstDesc.Usage, dstDesc.BindFlags);

    // Verify offsets and sizes
    if (SrcOffset + NumBytes > srcDesc.ByteWidth || 
        DstOffset + NumBytes > dstDesc.ByteWidth) {
        ERR("Copy region out of bounds");
        return;
    }

    // Create staging buffer if needed for partial copies
    if (SrcOffset > 0 || DstOffset > 0 || NumBytes != srcDesc.ByteWidth) {
        TRACE("Creating staging buffer for partial copy");
        D3D11_BUFFER_DESC stagingDesc = {};
        stagingDesc.ByteWidth = static_cast<UINT>(NumBytes);
        stagingDesc.Usage = D3D11_USAGE_STAGING;
        stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
        
        Microsoft::WRL::ComPtr<ID3D11Buffer> stagingBuffer;
        if (FAILED(m_device->GetD3D11Device()->CreateBuffer(&stagingDesc, nullptr, &stagingBuffer))) {
            ERR("Failed to create staging buffer");
            return;
        }

        // Copy source to staging
        D3D11_BOX srcBox = {};
        srcBox.left = static_cast<UINT>(SrcOffset);
        srcBox.right = static_cast<UINT>(SrcOffset + NumBytes);
        srcBox.top = 0;
        srcBox.bottom = 1;
        srcBox.front = 0;
        srcBox.back = 1;

        m_context->CopySubresourceRegion(stagingBuffer.Get(), 0, 0, 0, 0,
                                       d3d11SrcBuffer.Get(), 0, &srcBox);

        // Copy staging to destination
        D3D11_BOX dstBox = {};
        dstBox.left = 0;
        dstBox.right = static_cast<UINT>(NumBytes);
        dstBox.top = 0;
        dstBox.bottom = 1;
        dstBox.front = 0;
        dstBox.back = 1;

        m_context->CopySubresourceRegion(d3d11DstBuffer.Get(), 0, 
                                       static_cast<UINT>(DstOffset), 0, 0,
                                       stagingBuffer.Get(), 0, &dstBox);
    } else {
        // Direct copy for full buffer copies
        m_context->CopyResource(d3d11DstBuffer.Get(), d3d11SrcBuffer.Get());
    }
}

void WrappedD3D12ToD3D11CommandList::CopyTiles(
    ID3D12Resource* pTiledResource,
    const D3D12_TILED_RESOURCE_COORDINATE* pTileRegionStartCoordinate,
    const D3D12_TILE_REGION_SIZE* pTileRegionSize, ID3D12Resource* pBuffer,
    UINT64 BufferStartOffsetInBytes, D3D12_TILE_COPY_FLAGS Flags) {
    TRACE("(%p, %p, %p, %p, %llu, %d)", pTiledResource,
          pTileRegionStartCoordinate, pTileRegionSize, pBuffer,
          BufferStartOffsetInBytes, Flags);
    // TODO: Implement tile copying
}

void WrappedD3D12ToD3D11CommandList::ResolveSubresource(ID3D12Resource* pDstResource,
                                          UINT DstSubresource,
                                          ID3D12Resource* pSrcResource,
                                          UINT SrcSubresource,
                                          DXGI_FORMAT Format) {
    TRACE("WrappedD3D12ToD3D11CommandList::ResolveSubresource (%p, %u, %p, %u, %d)", pDstResource, DstSubresource, pSrcResource,
          SrcSubresource, Format);
    // TODO: Implement subresource resolution
}

void WrappedD3D12ToD3D11CommandList::IASetPrimitiveTopology(
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology) {
    TRACE("WrappedD3D12ToD3D11CommandList::IASetPrimitiveTopology (%d)", PrimitiveTopology);
    m_context->IASetPrimitiveTopology(
        static_cast<D3D11_PRIMITIVE_TOPOLOGY>(PrimitiveTopology));
}

void WrappedD3D12ToD3D11CommandList::RSSetViewports(UINT NumViewports,
                                      const D3D12_VIEWPORT* pViewports) {
    TRACE("WrappedD3D12ToD3D11CommandList::RSSetViewports(%u, %p)", NumViewports, pViewports);
    m_context->RSSetViewports(
        NumViewports, reinterpret_cast<const D3D11_VIEWPORT*>(pViewports));
}

void WrappedD3D12ToD3D11CommandList::RSSetScissorRects(UINT NumRects,
                                         const D3D12_RECT* pRects) {
    TRACE("WrappedD3D12ToD3D11CommandList::RSSetScissorRects(%u, %p)", NumRects, pRects);
    m_context->RSSetScissorRects(NumRects, pRects);
}

void WrappedD3D12ToD3D11CommandList::OMSetBlendFactor(const FLOAT BlendFactor[4]) {
    TRACE("WrappedD3D12ToD3D11CommandList::OMSetBlendFactor(%p)", BlendFactor);
    float currentBlendFactor[4];
    UINT SampleMask;
    ID3D11BlendState* blendState;
    m_context->OMGetBlendState(&blendState, currentBlendFactor, &SampleMask);
    m_context->OMSetBlendState(blendState, BlendFactor, SampleMask);
    if (blendState) blendState->Release();
}

void WrappedD3D12ToD3D11CommandList::OMSetStencilRef(UINT StencilRef) {
    TRACE("WrappedD3D12ToD3D11CommandList::OMSetStencilRef(%u)", StencilRef);
    ID3D11DepthStencilState* dsState;
    UINT currentRef;
    m_context->OMGetDepthStencilState(&dsState, &currentRef);
    m_context->OMSetDepthStencilState(dsState, StencilRef);
    if (dsState) dsState->Release();
}

void WrappedD3D12ToD3D11CommandList::SetPipelineState(ID3D12PipelineState* pPipelineState) {
    TRACE("WrappedD3D12ToD3D11CommandList::SetPipelineState(%p)", pPipelineState);
    
    if (!pPipelineState) {
        WARN("Null pipeline state passed to SetPipelineState");
        return;
    }

    auto* pipelineState = static_cast<WrappedD3D12ToD3D11PipelineState*>(pPipelineState);
    pipelineState->Apply(m_context.Get());
}

void WrappedD3D12ToD3D11CommandList::ExecuteBundle(ID3D12GraphicsCommandList* pCommandList) {
    TRACE("WrappedD3D12ToD3D11CommandList::ExecuteBundle(%p)", pCommandList);
    // TODO: Implement bundle execution
}

void WrappedD3D12ToD3D11CommandList::SetDescriptorHeaps(
    UINT NumDescriptorHeaps, ID3D12DescriptorHeap* const* ppDescriptorHeaps) {
    TRACE("WrappedD3D12ToD3D11CommandList::SetDescriptorHeaps(%u, %p)", NumDescriptorHeaps, ppDescriptorHeaps);
    // TODO: Implement descriptor heap setting
}

void WrappedD3D12ToD3D11CommandList::SetComputeRootSignature(
    ID3D12RootSignature* pRootSignature) {
    TRACE("WrappedD3D12ToD3D11CommandList::SetComputeRootSignature(%p)", pRootSignature);
    // TODO: Implement compute root signature setting
}

void WrappedD3D12ToD3D11CommandList::SetGraphicsRootSignature(
    ID3D12RootSignature* pRootSignature) {
    TRACE("WrappedD3D12ToD3D11CommandList::SetGraphicsRootSignature(%p)", pRootSignature);
    // TODO: Implement graphics root signature setting
}

void WrappedD3D12ToD3D11CommandList::SetComputeRootDescriptorTable(
    UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor) {
    TRACE("WrappedD3D12ToD3D11CommandList::SetComputeRootDescriptorTable(%u, %llu)", RootParameterIndex, BaseDescriptor.ptr);
    // TODO: Implement compute root descriptor table setting
}

void WrappedD3D12ToD3D11CommandList::SetGraphicsRootDescriptorTable(
    UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor) {
    TRACE("WrappedD3D12ToD3D11CommandList::SetGraphicsRootDescriptorTable(%u, %llu)", RootParameterIndex, BaseDescriptor.ptr);
    // TODO: Implement graphics root descriptor table setting
}

void WrappedD3D12ToD3D11CommandList::SetComputeRoot32BitConstant(
    UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues) {
    TRACE("WrappedD3D12ToD3D11CommandList::SetComputeRoot32BitConstant(%u, %u, %u)", RootParameterIndex, SrcData,
          DestOffsetIn32BitValues);
    // TODO: Implement compute root constant setting
}

void WrappedD3D12ToD3D11CommandList::SetGraphicsRoot32BitConstant(
    UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues) {
    TRACE("WrappedD3D12ToD3D11CommandList::SetGraphicsRoot32BitConstant(%u, %u, %u)", RootParameterIndex, SrcData,
          DestOffsetIn32BitValues);
    // TODO: Implement graphics root constant setting
}

void WrappedD3D12ToD3D11CommandList::SetComputeRoot32BitConstants(
    UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData,
    UINT DestOffsetIn32BitValues) {
    TRACE("WrappedD3D12ToD3D11CommandList::SetComputeRoot32BitConstants(%u, %u, %p, %u)", RootParameterIndex, Num32BitValuesToSet,
          pSrcData, DestOffsetIn32BitValues);
    // TODO: Implement compute root constants setting
}

void WrappedD3D12ToD3D11CommandList::SetGraphicsRoot32BitConstants(
    UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData,
    UINT DestOffsetIn32BitValues) {
    TRACE("WrappedD3D12ToD3D11CommandList::SetGraphicsRoot32BitConstants(%u, %u, %p, %u)", RootParameterIndex, Num32BitValuesToSet,
          pSrcData, DestOffsetIn32BitValues);
    // TODO: Implement graphics root constants setting
}

void WrappedD3D12ToD3D11CommandList::SetComputeRootConstantBufferView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
    TRACE("WrappedD3D12ToD3D11CommandList::SetComputeRootConstantBufferView(%u, %llu)", RootParameterIndex, BufferLocation);
    // TODO: Implement compute root constant buffer view setting
}

void WrappedD3D12ToD3D11CommandList::SetGraphicsRootConstantBufferView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
    TRACE("WrappedD3D12ToD3D11CommandList::SetGraphicsRootConstantBufferView(%u, %llu)", RootParameterIndex, BufferLocation);
    // TODO: Implement graphics root constant buffer view setting
}

void WrappedD3D12ToD3D11CommandList::SetComputeRootShaderResourceView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
    TRACE("WrappedD3D12ToD3D11CommandList::SetComputeRootShaderResourceView(%u, %llu)", RootParameterIndex, BufferLocation);
    // TODO: Implement compute root shader resource view setting
}

void WrappedD3D12ToD3D11CommandList::SetGraphicsRootShaderResourceView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
    TRACE("WrappedD3D12ToD3D11CommandList::SetGraphicsRootShaderResourceView(%u, %llu)", RootParameterIndex, BufferLocation);
    // TODO: Implement graphics root shader resource view setting
}

void WrappedD3D12ToD3D11CommandList::SetComputeRootUnorderedAccessView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
    TRACE("WrappedD3D12ToD3D11CommandList::SetComputeRootUnorderedAccessView(%u, %llu)", RootParameterIndex, BufferLocation);
    // TODO: Implement compute root unordered access view setting
}

void WrappedD3D12ToD3D11CommandList::SetGraphicsRootUnorderedAccessView(
    UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) {
    TRACE("WrappedD3D12ToD3D11CommandList::SetGraphicsRootUnorderedAccessView(%u, %llu)", RootParameterIndex, BufferLocation);
    // TODO: Implement graphics root unordered access view setting
}

void WrappedD3D12ToD3D11CommandList::IASetIndexBuffer(
    const D3D12_INDEX_BUFFER_VIEW* pView) {
    TRACE("WrappedD3D12ToD3D11CommandList::IASetIndexBuffer(%p)", pView);

    if (!pView) {
        m_context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
        return;
    }

    auto resource = static_cast<WrappedD3D12ToD3D11Resource*>(
        reinterpret_cast<ID3D12Resource*>(pView->BufferLocation));
    if (!resource) {
        ERR("Failed to get resource from buffer location");
        return;
    }

    auto d3d11Buffer = resource->GetD3D11Resource();
    if (!d3d11Buffer) {
        ERR("Failed to get D3D11 buffer");
        return;
    }

    m_context->IASetIndexBuffer(
        static_cast<ID3D11Buffer*>(d3d11Buffer),
        pView->Format,
        pView->BufferLocation & 0xFFFFFFFF);  // Use lower 32-bits as offset
}

void WrappedD3D12ToD3D11CommandList::IASetVertexBuffers(
    UINT StartSlot, UINT NumViews, const D3D12_VERTEX_BUFFER_VIEW* pViews) {
    if (!pViews) {
        ERR("Invalid vertex buffer views");
        return;
    }

    std::vector<ID3D11Buffer*> buffers(NumViews);
    std::vector<UINT> strides(NumViews);
    std::vector<UINT> offsets(NumViews);

    for (UINT i = 0; i < NumViews; i++) {
        const auto& view = pViews[i];
        ID3D11Resource* resource = nullptr;
        HRESULT hr = reinterpret_cast<ID3D12Resource*>(view.BufferLocation)->QueryInterface(
            __uuidof(ID3D11Resource), (void**)&resource);
        if (FAILED(hr)) {
            ERR("Failed to get D3D11 resource for vertex buffer %u", i);
            // Clean up previously acquired resources
            for (UINT j = 0; j < i; j++) {
                buffers[j]->Release();
            }
            return;
        }
        buffers[i] = reinterpret_cast<ID3D11Buffer*>(resource);
        strides[i] = view.StrideInBytes;
        offsets[i] = 0;
    }

    m_context->IASetVertexBuffers(StartSlot, NumViews, buffers.data(),
                                 strides.data(), offsets.data());

    // Clean up
    for (UINT i = 0; i < NumViews; i++) {
        buffers[i]->Release();
    }
}

void WrappedD3D12ToD3D11CommandList::SOSetTargets(
    UINT StartSlot, UINT NumViews,
    const D3D12_STREAM_OUTPUT_BUFFER_VIEW* pViews) {
    TRACE("WrappedD3D12ToD3D11CommandList::SOSetTargets(%u, %u, %p)", StartSlot, NumViews, pViews);
    // TODO: Implement stream output target setting
}

void WrappedD3D12ToD3D11CommandList::OMSetRenderTargets(
    UINT NumRenderTargetDescriptors,
    const D3D12_CPU_DESCRIPTOR_HANDLE* pRenderTargetDescriptors,
    BOOL RTsSingleHandleToDescriptorRange,
    const D3D12_CPU_DESCRIPTOR_HANDLE* pDepthStencilDescriptor) {
    TRACE("WrappedD3D12ToD3D11CommandList::OMSetRenderTargets(%u, %p, %d, %p)", NumRenderTargetDescriptors,
          pRenderTargetDescriptors, RTsSingleHandleToDescriptorRange,
          pDepthStencilDescriptor);
    // TODO: Implement render target setting
}

void WrappedD3D12ToD3D11CommandList::ClearDepthStencilView(
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView, D3D12_CLEAR_FLAGS ClearFlags,
    FLOAT Depth, UINT8 Stencil, UINT NumRects, const D3D12_RECT* pRects) {
    TRACE("WrappedD3D12ToD3D11CommandList::ClearDepthStencilView(%llu, %d, %f, %u, %u, %p)", DepthStencilView.ptr, ClearFlags,
          Depth, Stencil, NumRects, pRects);
    // TODO: Implement depth stencil view clearing
}

void WrappedD3D12ToD3D11CommandList::ClearRenderTargetView(
    D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const FLOAT ColorRGBA[4],
    UINT NumRects, const D3D12_RECT* pRects) {
    TRACE("WrappedD3D12ToD3D11CommandList::ClearRenderTargetView(%llu, %p, %u, %p)", RenderTargetView.ptr, ColorRGBA, NumRects,
          pRects);
    // TODO: Implement render target view clearing
}

void WrappedD3D12ToD3D11CommandList::ClearUnorderedAccessViewUint(
    D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap,
    D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource* pResource,
    const UINT Values[4], UINT NumRects, const D3D12_RECT* pRects) {
    TRACE("WrappedD3D12ToD3D11CommandList::ClearUnorderedAccessViewUint(%llu, %llu, %p, %p, %u, %p)", ViewGPUHandleInCurrentHeap.ptr,
          ViewCPUHandle.ptr, pResource, Values, NumRects, pRects);
    // TODO: Implement UAV clearing (uint)
}

void WrappedD3D12ToD3D11CommandList::ClearUnorderedAccessViewFloat(
    D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap,
    D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource* pResource,
    const FLOAT Values[4], UINT NumRects, const D3D12_RECT* pRects) {
    TRACE("WrappedD3D12ToD3D11CommandList::ClearUnorderedAccessViewFloat(%llu, %llu, %p, %p, %u, %p)", ViewGPUHandleInCurrentHeap.ptr,
          ViewCPUHandle.ptr, pResource, Values, NumRects, pRects);
    // TODO: Implement UAV clearing (float)
}

void WrappedD3D12ToD3D11CommandList::DiscardResource(ID3D12Resource* pResource,
                                       const D3D12_DISCARD_REGION* pRegion) {
    TRACE("WrappedD3D12ToD3D11CommandList::DiscardResource(%p, %p)", pResource, pRegion);
    // TODO: Implement resource discarding
}

void WrappedD3D12ToD3D11CommandList::BeginQuery(ID3D12QueryHeap* pQueryHeap,
                                  D3D12_QUERY_TYPE Type, UINT Index) {
    TRACE("WrappedD3D12ToD3D11CommandList::BeginQuery(%p, %d, %u)", pQueryHeap, Type, Index);
    // TODO: Implement query beginning
}

void WrappedD3D12ToD3D11CommandList::EndQuery(ID3D12QueryHeap* pQueryHeap,
                                D3D12_QUERY_TYPE Type, UINT Index) {
    TRACE("WrappedD3D12ToD3D11CommandList::EndQuery(%p, %d, %u)", pQueryHeap, Type, Index);
    // TODO: Implement query ending
}

void WrappedD3D12ToD3D11CommandList::ResolveQueryData(ID3D12QueryHeap* pQueryHeap,
                                        D3D12_QUERY_TYPE Type, UINT StartIndex,
                                        UINT NumQueries,
                                        ID3D12Resource* pDestinationBuffer,
                                        UINT64 AlignedDestinationBufferOffset) {
    TRACE("WrappedD3D12ToD3D11CommandList::ResolveQueryData(%p, %d, %u, %u, %p, %llu)", pQueryHeap, Type, StartIndex,
          NumQueries, pDestinationBuffer, AlignedDestinationBufferOffset);
    // TODO: Implement query data resolution
}

void WrappedD3D12ToD3D11CommandList::SetPredication(ID3D12Resource* pBuffer,
                                      UINT64 AlignedBufferOffset,
                                      D3D12_PREDICATION_OP Operation) {
    TRACE("WrappedD3D12ToD3D11CommandList::SetPredication(%p, %llu, %d)", pBuffer, AlignedBufferOffset, Operation);
    // TODO: Implement predication setting
}

void WrappedD3D12ToD3D11CommandList::SetMarker(UINT Metadata, const void* pData, UINT Size) {
    TRACE("WrappedD3D12ToD3D11CommandList::SetMarker(%u, %p, %u)", Metadata, pData, Size);
    // TODO: Implement marker setting
}

void WrappedD3D12ToD3D11CommandList::BeginEvent(UINT Metadata, const void* pData, UINT Size) {
    TRACE("WrappedD3D12ToD3D11CommandList(%u, %p, %u)", Metadata, pData, Size);
    // TODO: Implement event beginning
}

void WrappedD3D12ToD3D11CommandList::EndEvent() {
    TRACE("WrappedD3D12ToD3D11CommandList()");
    // TODO: Implement event ending
}

void WrappedD3D12ToD3D11CommandList::ExecuteIndirect(
    ID3D12CommandSignature* pCommandSignature, UINT MaxCommandCount,
    ID3D12Resource* pArgumentBuffer, UINT64 ArgumentBufferOffset,
    ID3D12Resource* pCountBuffer, UINT64 CountBufferOffset) {
    TRACE("(%p, %u, %p, %llu, %p, %llu)", pCommandSignature, MaxCommandCount,
          pArgumentBuffer, ArgumentBufferOffset, pCountBuffer,
          CountBufferOffset);
    // TODO: Implement indirect execution
}

void WrappedD3D12ToD3D11CommandList::CopyTextureRegion(
    const D3D12_TEXTURE_COPY_LOCATION* pDst, UINT DstX, UINT DstY, UINT DstZ,
    const D3D12_TEXTURE_COPY_LOCATION* pSrc, const D3D12_BOX* pSrcBox) {
    TRACE("CopyTextureRegion: dst[%u,%u,%u]", DstX, DstY, DstZ);

    if (!pDst || !pSrc) {
        ERR("Invalid source or destination texture location");
        return;
    }

    // Get the underlying D3D11 resources
    ID3D11Resource* d3d11SrcResource = nullptr;
    ID3D11Resource* d3d11DstResource = nullptr;

    auto srcResource = static_cast<WrappedD3D12ToD3D11Resource*>(pSrc->pResource);
    auto dstResource = static_cast<WrappedD3D12ToD3D11Resource*>(pDst->pResource);

    if (!srcResource || !dstResource) {
        ERR("Invalid source or destination resource");
        return;
    }

    d3d11SrcResource = srcResource->GetD3D11Resource();
    d3d11DstResource = dstResource->GetD3D11Resource();

    if (!d3d11SrcResource || !d3d11DstResource) {
        ERR("Failed to get D3D11 resources");
        return;
    }

    D3D12_RESOURCE_DESC srcDesc = {};
    D3D12_RESOURCE_DESC dstDesc = {};
    srcResource->GetDesc(&srcDesc);
    dstResource->GetDesc(&dstDesc);

    // Special handling for buffer to texture copy
    if (srcDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER && 
        dstDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
        
        // Validate resources before proceeding
        if (!d3d11SrcResource || !d3d11DstResource) {
            ERR("Invalid D3D11 resources for copy operation");
            return;
        }

        try {
            // Get footprint from destination texture
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
            UINT numRows = 0;
            UINT64 rowSizeInBytes = 0;
            UINT64 totalBytes = 0;

            // Get copyable footprints
            m_device->GetCopyableFootprints(
                &dstDesc, 
                pDst->SubresourceIndex, 
                1,
                pSrc->PlacedFootprint.Offset, 
                &footprint,
                &numRows, 
                &rowSizeInBytes, 
                &totalBytes);

            // Validate the footprint after getting it
            if (footprint.Footprint.Format == DXGI_FORMAT_UNKNOWN ||
                footprint.Footprint.Width == 0 ||
                footprint.Footprint.Height == 0 ||
                rowSizeInBytes == 0) {
                ERR("Invalid footprint returned from GetCopyableFootprints");
                return;
            }

            // Validate total memory required
            if (totalBytes > (1ULL << 31)) {  // Sanity check for 2GB limit
                ERR("Copy operation requires too much memory: %llu bytes", totalBytes);
                return;
            }

            // For debugging
            TRACE("Copy operation memory requirements:");
            TRACE("Total bytes: %llu", totalBytes);
            TRACE("Row size in bytes: %llu", rowSizeInBytes);
            TRACE("Number of rows: %u", numRows);

            // Validate dimensions
            if (footprint.Footprint.Width == 0 || footprint.Footprint.Height == 0 ||
                footprint.Footprint.Width > dstDesc.Width ||
                footprint.Footprint.Height > dstDesc.Height) {
                ERR("Invalid footprint dimensions: %ux%u (max: %ux%u)",
                    footprint.Footprint.Width, footprint.Footprint.Height,
                    dstDesc.Width, dstDesc.Height);
                return;
            }

            // Use computed footprint dimensions
            D3D11_BOX srcBox = {};
            srcBox.left = 0;
            srcBox.right = footprint.Footprint.Width;
            srcBox.top = 0;
            srcBox.bottom = footprint.Footprint.Height;
            srcBox.front = 0;
            srcBox.back = 1;

            // Perform the copy with exception handling
            m_context->CopySubresourceRegion(
                d3d11DstResource,
                pDst->SubresourceIndex,
                DstX, DstY, DstZ,
                d3d11SrcResource,
                0,
                &srcBox);

        } catch (const std::exception& e) {
            ERR("Exception during copy operation: %s", e.what());
            return;
        }
        return;
    }

    // Regular texture to texture copy
    if (srcDesc.Dimension != dstDesc.Dimension) {
        ERR("Incompatible D3D12 resource dimensions: src=%d, dst=%d", 
            srcDesc.Dimension, dstDesc.Dimension);
        return;
    }

    // Convert D3D12 box to D3D11 box if provided
    D3D11_BOX d3d11SrcBox = {};
    if (pSrcBox) {
        d3d11SrcBox.left = pSrcBox->left;
        d3d11SrcBox.right = pSrcBox->right;
        d3d11SrcBox.top = pSrcBox->top;
        d3d11SrcBox.bottom = pSrcBox->bottom;
        d3d11SrcBox.front = pSrcBox->front;
        d3d11SrcBox.back = pSrcBox->back;
    }

    m_context->CopySubresourceRegion(
        d3d11DstResource,
        pDst->SubresourceIndex,
        DstX, DstY, DstZ,
        d3d11SrcResource,
        pSrc->SubresourceIndex,
        pSrcBox ? &d3d11SrcBox : nullptr);
}

void WrappedD3D12ToD3D11CommandList::DrawInstanced(
    UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation,
    UINT StartInstanceLocation) {
    TRACE("WrappedD3D12ToD3D11CommandList::DrawInstanced: %u, %u, %u, %u", VertexCountPerInstance,
          InstanceCount, StartVertexLocation, StartInstanceLocation);
    m_context->DrawInstanced(VertexCountPerInstance, InstanceCount,
                             StartVertexLocation, StartInstanceLocation);
}

void WrappedD3D12ToD3D11CommandList::DrawIndexedInstanced(
    UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation,
    INT BaseVertexLocation, UINT StartInstanceLocation) {
    TRACE("DrawIndexedInstanced: %u, %u, %u, %d, %u", IndexCountPerInstance,
          InstanceCount, StartIndexLocation, BaseVertexLocation,
          StartInstanceLocation);
    m_context->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount,
                                    StartIndexLocation, BaseVertexLocation,
                                    StartInstanceLocation);
}

void WrappedD3D12ToD3D11CommandList::Dispatch(UINT ThreadGroupCountX,
                                                  UINT ThreadGroupCountY,
                                                  UINT ThreadGroupCountZ) {
    TRACE("WrappedD3D12ToD3D11CommandList::Dispatch: %u, %u, %u", ThreadGroupCountX, ThreadGroupCountY,
          ThreadGroupCountZ);
    m_context->Dispatch(ThreadGroupCountX, ThreadGroupCountY,
                        ThreadGroupCountZ);
}


HRESULT WrappedD3D12ToD3D11CommandList::GetD3D11CommandList(ID3D11CommandList** ppCommandList) {
    TRACE("WrappedD3D12ToD3D11CommandList::GetD3D11CommandList(%p)", ppCommandList);
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

void WrappedD3D12ToD3D11CommandList::ResourceBarrier(
    UINT NumBarriers, const D3D12_RESOURCE_BARRIER* pBarriers) {
    TRACE("ResourceBarrier: %u, %p", NumBarriers, pBarriers);
    // D3D11 handles resource states automatically, so we can ignore barriers
    TRACE("Ignoring %u resource barriers.", NumBarriers);
}

void WrappedD3D12ToD3D11CommandList::ClearState(ID3D12PipelineState* pPipelineState) {
    TRACE("WrappedD3D12ToD3D11CommandList::ClearState(%p)", pPipelineState);

    m_context->ClearState();
}

HRESULT WrappedD3D12ToD3D11CommandList::GetD3D11Resource(
    ID3D12Resource* d3d12Resource,
    Microsoft::WRL::ComPtr<ID3D11Resource>* ppD3D11Resource) {
    
    if (!d3d12Resource || !ppD3D11Resource) {
        return E_INVALIDARG;
    }

    auto* wrappedResource = static_cast<WrappedD3D12ToD3D11Resource*>(d3d12Resource);
    *ppD3D11Resource = wrappedResource->GetD3D11Resource();
    return S_OK;
}

HRESULT WrappedD3D12ToD3D11CommandList::GetD3D11Buffer(
    ID3D12Resource* d3d12Resource,
    Microsoft::WRL::ComPtr<ID3D11Buffer>* ppD3D11Buffer) {
    
    if (!d3d12Resource || !ppD3D11Buffer) {
        return E_INVALIDARG;
    }

    Microsoft::WRL::ComPtr<ID3D11Resource> d3d11Resource;
    HRESULT hr = GetD3D11Resource(d3d12Resource, &d3d11Resource);
    if (FAILED(hr)) {
        return hr;
    }

    return d3d11Resource.As(ppD3D11Buffer);
}
}  // namespace dxiided
