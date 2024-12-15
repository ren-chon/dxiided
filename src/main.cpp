#include <d3d11.h>
#include <d3d12.h>
#include <wrl/client.h>

#include "common/debug.hpp"
#include "d3d11_impl/device.hpp"

using Microsoft::WRL::ComPtr;
using namespace dxiided;

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void* reserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(instance);
            debug::Logger::Instance().Initialize();
            break;
    }
    return TRUE;
}

extern "C" HRESULT WINAPI
D3D12CreateDevice(IUnknown* adapter, D3D_FEATURE_LEVEL minimum_feature_level,
                  REFIID riid, void** device) {
    TRACE("adapter %p, minimum_feature_level %#x, riid %s, device %p.\n",
          adapter, minimum_feature_level, debugstr_guid(&riid).c_str(), device);

    if (!device) {
        ERR("Invalid device pointer.\n");
        return E_INVALIDARG;
    }

    return D3D11Device::Create(adapter, minimum_feature_level, riid, device);
}

extern "C" HRESULT WINAPI D3D12GetDebugInterface(REFIID riid, void** debug) {
    TRACE("riid %s, debug %p.\n", debugstr_guid(&riid).c_str(), debug);

    // For MVP, we don't implement debug interface yet
    return E_NOTIMPL;
}

extern "C" HRESULT WINAPI D3D12EnableExperimentalFeatures(
    UINT feature_count, const IID* iids, void* configurations,
    UINT* configurations_sizes) {
    TRACE(
        "feature_count %u, iids %p, configurations %p, configurations_sizes "
        "%p.\n",
        feature_count, iids, configurations, configurations_sizes);

    // For MVP, we don't support experimental features
    return E_NOTIMPL;
}

extern "C" HRESULT WINAPI D3D12SerializeRootSignature(
    const D3D12_ROOT_SIGNATURE_DESC* root_signature_desc,
    D3D_ROOT_SIGNATURE_VERSION version, ID3DBlob** blob,
    ID3DBlob** error_blob) {
    TRACE("root_signature_desc %p, version %#x, blob %p, error_blob %p.\n",
          root_signature_desc, version, blob, error_blob);

    // For MVP, we don't implement root signatures yet
    return E_NOTIMPL;
}

extern "C" HRESULT WINAPI D3D12CreateRootSignatureDeserializer(
    const void* serialized_root_signature,
    SIZE_T serialized_root_signature_size, REFIID riid, void** deserializer) {
    TRACE(
        "serialized_root_signature %p, serialized_root_signature_size %zu, "
        "riid %s, deserializer %p.\n",
        serialized_root_signature, serialized_root_signature_size,
        debugstr_guid(&riid).c_str(), deserializer);

    // For MVP, we don't implement root signature deserialization yet
    return E_NOTIMPL;
}

extern "C" HRESULT WINAPI D3D12CreateVersionedRootSignatureDeserializer(
    const void* serialized_root_signature,
    SIZE_T serialized_root_signature_size, REFIID riid, void** deserializer) {
    TRACE(
        "serialized_root_signature %p, serialized_root_signature_size %zu, "
        "riid %s, deserializer %p.\n",
        serialized_root_signature, serialized_root_signature_size,
        debugstr_guid(&riid).c_str(), deserializer);

    // For MVP, we don't implement versioned root signature deserialization yet
    return E_NOTIMPL;
}

extern "C" HRESULT WINAPI D3D12SerializeVersionedRootSignature(
    const D3D12_VERSIONED_ROOT_SIGNATURE_DESC* desc, ID3DBlob** blob,
    ID3DBlob** error_blob) {
    TRACE("desc %p, blob %p, error_blob %p.\n", desc, blob, error_blob);

    // For MVP, we don't implement versioned root signature serialization yet
    return E_NOTIMPL;
}
