#include <d3d11.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <fstream>
#include <mutex>
#include <chrono>
#include <ctime>
#include <d3d11shader.h>    // For D3D11_SIT_ enums
#include <d3dcompiler.h>    // For D3DCreateBlob
#include <d3dcommon.h>
#include <atomic>
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
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}

extern "C" HRESULT WINAPI
D3D12CreateDevice(IUnknown* adapter, D3D_FEATURE_LEVEL minimum_feature_level,
                  REFIID riid, void** device) {
    TRACE("adapter %p, minimum_feature_level %#x, riid %s, device %p.",
          adapter, minimum_feature_level, debugstr_guid(&riid).c_str(), device);

    // If device is null, this is just a capability check
    if (device == nullptr) {
        TRACE("  Capability check - returning S_FALSE to indicate device could be created");
        return S_FALSE;  // Return S_FALSE to indicate device could be created but wasn't
    }

    // It's okay for *device to be null, we'll set it ourselves
    *device = nullptr;
    TRACE("  Attempting to create device...");
    return D3D11Device::Create(adapter, minimum_feature_level, riid, device);
}

extern "C" HRESULT WINAPI D3D12GetDebugInterface(REFIID riid, void** debug) {
    TRACE("riid %s, debug %p.", debugstr_guid(&riid).c_str(), debug);

    if (!debug) {
        return E_INVALIDARG;
    }

    // For now, return success but don't provide a debug interface
    // This prevents games from failing if they expect debug to work
    *debug = nullptr;
    return S_OK;
}

extern "C" HRESULT WINAPI CreateDXGIFactory2(UINT Flags, REFIID riid, void** ppFactory) {
    TRACE("Flags %u, riid %s, ppFactory %p.", Flags, debugstr_guid(&riid).c_str(), ppFactory);

    if (!ppFactory) {
        return E_INVALIDARG;
    }

    // For MVP, we create a basic factory without additional features
    // return D3D11Device::CreateDXGIFactory(Flags, riid, ppFactory);
    return E_NOTIMPL;
}

extern "C" HRESULT WINAPI D3D12CreateCommandQueue(ID3D12Device* pDevice,
                                                  const D3D12_COMMAND_QUEUE_DESC* pDesc,
                                                  REFIID riid,
                                                  void** ppCommandQueue) {
    TRACE("pDevice %p, pDesc %p, riid %s, ppCommandQueue %p.",
          pDevice, pDesc, debugstr_guid(&riid).c_str(), ppCommandQueue);

    if (!pDevice || !pDesc || !ppCommandQueue) {
        return E_INVALIDARG;
    }

    return pDevice->CreateCommandQueue(pDesc, riid, ppCommandQueue);
}


extern "C" HRESULT WINAPI D3D12EnableExperimentalFeatures(
    UINT feature_count, const IID* iids, void* configurations,
    UINT* configurations_sizes) {
    TRACE(
        "feature_count %u, iids %p, configurations %p, configurations_sizes "
        "%p.",
        feature_count, iids, configurations, configurations_sizes);

    // For MVP, we don't support experimental features
    return E_NOTIMPL;
}

extern "C" HRESULT WINAPI D3D12SerializeRootSignature(
    const D3D12_ROOT_SIGNATURE_DESC* root_signature_desc,
    D3D_ROOT_SIGNATURE_VERSION version, ID3DBlob** blob,
    ID3DBlob** error_blob) {
    TRACE("root_signature_desc %p, version %#x, blob %p, error_blob %p.",
          root_signature_desc, version, blob, error_blob);

    if (!root_signature_desc || !blob) {
        return E_INVALIDARG;
    }

    // Convert D3D12 root parameters to D3D11 binding info
    struct D3D11BindingInfo {
        UINT register_space;
        UINT register_index;
        D3D_SHADER_INPUT_TYPE type;
        UINT num_constants;  // For inline CBV/constants
    };
    std::vector<D3D11BindingInfo> d3d11_bindings;

    // Parse root parameters
    for (UINT i = 0; i < root_signature_desc->NumParameters; i++) {
        const auto& param = root_signature_desc->pParameters[i];
        D3D11BindingInfo binding = {};
        
        switch (param.ParameterType) {
            case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE: {
                // Convert descriptor tables to D3D11 resource views
                for (UINT r = 0; r < param.DescriptorTable.NumDescriptorRanges; r++) {
                    const auto& range = param.DescriptorTable.pDescriptorRanges[r];
                    binding.register_space = range.RegisterSpace;
                    binding.register_index = range.BaseShaderRegister;
                    binding.num_constants = 0;
                    // Map D3D12 descriptor types to D3D11
                    switch (range.RangeType) {
                        case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
                            binding.type = D3D_SIT_TEXTURE;
                            break;
                        case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
                            binding.type = D3D_SIT_UAV_RWTYPED;
                            break;
                        case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
                            binding.type = D3D_SIT_CBUFFER;
                            break;
                        case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
                            binding.type = D3D_SIT_SAMPLER;
                            break;
                    }
                    d3d11_bindings.push_back(binding);
                }
                break;
            }
            case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS: {
                // Map root constants to D3D11 constant buffer
                binding.register_space = 0;  // D3D11 doesn't use register spaces
                binding.register_index = param.Constants.ShaderRegister;
                binding.type = D3D_SIT_CBUFFER;
                binding.num_constants = param.Constants.Num32BitValues;
                d3d11_bindings.push_back(binding);
                break;
            }
            case D3D12_ROOT_PARAMETER_TYPE_CBV: {
                // Direct CBV mapping
                binding.register_space = 0;
                binding.register_index = param.Descriptor.ShaderRegister;
                binding.type = D3D_SIT_CBUFFER;
                binding.num_constants = 0;  // Full buffer
                d3d11_bindings.push_back(binding);
                break;
            }
            case D3D12_ROOT_PARAMETER_TYPE_SRV: {
                binding.register_space = 0;
                binding.register_index = param.Descriptor.ShaderRegister;
                binding.type = D3D_SIT_TEXTURE;
                binding.num_constants = 0;
                d3d11_bindings.push_back(binding);
                break;
            }
            case D3D12_ROOT_PARAMETER_TYPE_UAV: {
                binding.register_space = 0;
                binding.register_index = param.Descriptor.ShaderRegister;
                binding.type = D3D_SIT_UAV_RWTYPED;
                binding.num_constants = 0;
                d3d11_bindings.push_back(binding);
                break;
            }
        }
    }

    // Create a blob that contains our D3D11 binding information
    const UINT blob_size = sizeof(UINT) * (4 + d3d11_bindings.size() * 4);  // Added space for num_constants
    ID3DBlob* result = nullptr;
    HRESULT hr = D3DCreateBlob(blob_size, &result);
    
    if (FAILED(hr)) {
        return hr;
    }

    // Store the binding information in a format we can later deserialize
    UINT* data = (UINT*)result->GetBufferPointer();
    data[0] = 0x00000001;  // Version
    data[1] = root_signature_desc->Flags;  // D3D12 flags (we'll handle translation during usage)
    data[2] = (UINT)d3d11_bindings.size();  // Number of bindings
    data[3] = root_signature_desc->NumStaticSamplers;

    // Store binding information
    UINT offset = 4;
    for (const auto& binding : d3d11_bindings) {
        data[offset++] = binding.register_space;
        data[offset++] = binding.register_index;
        data[offset++] = (UINT)binding.type;
        data[offset++] = binding.num_constants;
    }

    *blob = result;
    TRACE("Created D3D11 binding blob with %zu bindings", d3d11_bindings.size());
    return S_OK;
}

extern "C" HRESULT WINAPI D3D12CreateRootSignatureDeserializer(
    const void* serialized_root_signature,
    SIZE_T serialized_root_signature_size, REFIID riid, void** deserializer) {
    TRACE("serialized_root_signature %p, serialized_root_signature_size %zu, riid %s, deserializer %p.",
          serialized_root_signature, serialized_root_signature_size, debugstr_guid(&riid).c_str(), deserializer);

    if (!serialized_root_signature || !deserializer || serialized_root_signature_size < 16) {
        return E_INVALIDARG;
    }

    // Create our deserializer implementation
    struct D3D11RootSignatureDeserializer : public ID3D12RootSignatureDeserializer {
        virtual ~D3D11RootSignatureDeserializer() {}  // Add virtual destructor
        D3D11RootSignatureDeserializer(const void* data, SIZE_T size) 
            : ref_count(1), blob_data(data), blob_size(size) {
            // Parse the header
            UINT* header = (UINT*)data;
            // Version is not part of D3D12_ROOT_SIGNATURE_DESC, skip it
            desc.Flags = static_cast<D3D12_ROOT_SIGNATURE_FLAGS>(header[1]);
            UINT num_bindings = header[2];
            desc.NumStaticSamplers = header[3];

            // Allocate and parse parameters
            parameters.resize(num_bindings);
            desc.NumParameters = num_bindings;
            desc.pParameters = parameters.data();

            // Parse each binding
            const UINT* binding_data = header + 4;
            for (UINT i = 0; i < num_bindings; i++) {
                D3D12_ROOT_PARAMETER& param = parameters[i];
                UINT register_space = binding_data[i * 4];
                UINT register_index = binding_data[i * 4 + 1];
                D3D_SHADER_INPUT_TYPE type = (D3D_SHADER_INPUT_TYPE)binding_data[i * 4 + 2];
                UINT num_constants = binding_data[i * 4 + 3];

                // Convert back to D3D12 parameter type
                switch (type) {
                    case D3D_SIT_CBUFFER:
                        if (num_constants > 0) {
                            param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
                            param.Constants.ShaderRegister = register_index;
                            param.Constants.RegisterSpace = register_space;
                            param.Constants.Num32BitValues = num_constants;
                        } else {
                            param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
                            param.Descriptor.ShaderRegister = register_index;
                            param.Descriptor.RegisterSpace = register_space;
                        }
                        break;
                    case D3D_SIT_TEXTURE:
                        param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
                        param.Descriptor.ShaderRegister = register_index;
                        param.Descriptor.RegisterSpace = register_space;
                        break;
                    case D3D_SIT_UAV_RWTYPED:
                        param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
                        param.Descriptor.ShaderRegister = register_index;
                        param.Descriptor.RegisterSpace = register_space;
                        break;
                    case D3D_SIT_SAMPLER:
                        // Handle samplers specially since they're static in our implementation
                        break;
                    default:
                        // Handle other types as needed or skip
                        break;
                }
            }
        }

        // IUnknown methods
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override {
            if (!ppvObject) return E_POINTER;
            *ppvObject = nullptr;

            if (riid == __uuidof(ID3D12RootSignatureDeserializer) || 
                riid == __uuidof(IUnknown)) {
                AddRef();
                *ppvObject = this;
                return S_OK;
            }

            return E_NOINTERFACE;
        }

        ULONG STDMETHODCALLTYPE AddRef() override {
            return ++ref_count;
        }

        ULONG STDMETHODCALLTYPE Release() override {
            ULONG ref = --ref_count;
            if (ref == 0) delete this;
            return ref;
        }

        // ID3D12RootSignatureDeserializer methods
        const D3D12_ROOT_SIGNATURE_DESC* STDMETHODCALLTYPE GetRootSignatureDesc() override {
            return &desc;
        }

    private:
        std::atomic<ULONG> ref_count;
        const void* blob_data;
        SIZE_T blob_size;
        D3D12_ROOT_SIGNATURE_DESC desc;
        std::vector<D3D12_ROOT_PARAMETER> parameters;
    };

    // Create and return the deserializer
    D3D11RootSignatureDeserializer* impl = new D3D11RootSignatureDeserializer(
        serialized_root_signature, serialized_root_signature_size);
    
    HRESULT hr = impl->QueryInterface(riid, deserializer);
    impl->Release();  // Release our reference, QueryInterface added one if successful
    
    return hr;
}

extern "C" HRESULT WINAPI D3D12CreateVersionedRootSignatureDeserializer(
    const void* serialized_root_signature,
    SIZE_T serialized_root_signature_size, REFIID riid, void** deserializer) {
    TRACE("serialized_root_signature %p, serialized_root_signature_size %zu, riid %s, deserializer %p.",
          serialized_root_signature, serialized_root_signature_size, debugstr_guid(&riid).c_str(), deserializer);

    if (!serialized_root_signature || !deserializer || serialized_root_signature_size < 16) {
        return E_INVALIDARG;
    }

    // Create our versioned deserializer implementation
    struct D3D11VersionedRootSignatureDeserializer : public ID3D12VersionedRootSignatureDeserializer {
        D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc;
        const void* blob_data;
        SIZE_T blob_size;
        ULONG ref_count;
        std::vector<D3D12_ROOT_PARAMETER1> parameters_1_1;

        D3D11VersionedRootSignatureDeserializer(const void* data, SIZE_T size) 
            : blob_data(data), blob_size(size), ref_count(1) {
            // Parse the header similar to non-versioned, but store as versioned desc
            const UINT* header = static_cast<const UINT*>(data);
            desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;  // Always use latest version
            desc.Desc_1_1.Flags = (D3D12_ROOT_SIGNATURE_FLAGS)header[1];
            UINT num_bindings = header[2];
            desc.Desc_1_1.NumStaticSamplers = header[3];

            // Allocate and parse parameters
            parameters_1_1.resize(num_bindings);
            desc.Desc_1_1.NumParameters = num_bindings;
            desc.Desc_1_1.pParameters = parameters_1_1.data();

            // Parse bindings similar to non-versioned deserializer
            const UINT* binding_data = header + 4;
            for (UINT i = 0; i < num_bindings; i++) {
                D3D12_ROOT_PARAMETER1& param = parameters_1_1[i];
                UINT register_space = binding_data[i * 4];
                UINT register_index = binding_data[i * 4 + 1];
                D3D_SHADER_INPUT_TYPE type = (D3D_SHADER_INPUT_TYPE)binding_data[i * 4 + 2];
                UINT num_constants = binding_data[i * 4 + 3];

                // Convert to versioned parameter type (similar to non-versioned)
                switch (type) {
                    case D3D_SIT_CBUFFER:
                        if (num_constants > 0) {
                            param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
                            param.Constants.ShaderRegister = register_index;
                            param.Constants.RegisterSpace = register_space;
                            param.Constants.Num32BitValues = num_constants;
                        } else {
                            param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
                            param.Descriptor.ShaderRegister = register_index;
                            param.Descriptor.RegisterSpace = register_space;
                            param.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
                        }
                        break;
                    case D3D_SIT_TEXTURE:
                        param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
                        param.Descriptor.ShaderRegister = register_index;
                        param.Descriptor.RegisterSpace = register_space;
                        param.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
                        break;
                    case D3D_SIT_UAV_RWTYPED:
                        param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
                        param.Descriptor.ShaderRegister = register_index;
                        param.Descriptor.RegisterSpace = register_space;
                        param.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
                        break;
                    default:
                        // Handle other types as needed or skip
                        break;
                }
                
                // Set shader visibility (versioned API requires this)
                param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            }
        }

        // IUnknown methods
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override {
            if (!ppvObject) return E_POINTER;
            *ppvObject = nullptr;

            if (riid == __uuidof(ID3D12VersionedRootSignatureDeserializer) || 
                riid == __uuidof(IUnknown)) {
                AddRef();
                *ppvObject = this;
                return S_OK;
            }

            return E_NOINTERFACE;
        }

        ULONG STDMETHODCALLTYPE AddRef() override {
            return InterlockedIncrement(&ref_count);
        }

        ULONG STDMETHODCALLTYPE Release() override {
            ULONG ref = InterlockedDecrement(&ref_count);
            if (ref == 0) delete this;
            return ref;
        }

        // ID3D12VersionedRootSignatureDeserializer methods
        virtual HRESULT STDMETHODCALLTYPE GetRootSignatureDescAtVersion(
            D3D_ROOT_SIGNATURE_VERSION version,
            const D3D12_VERSIONED_ROOT_SIGNATURE_DESC** ppDesc) override {
            if (!ppDesc) return E_INVALIDARG;
            *ppDesc = &desc;
            return S_OK;
        }

        virtual const D3D12_VERSIONED_ROOT_SIGNATURE_DESC* STDMETHODCALLTYPE GetUnconvertedRootSignatureDesc() override {
            return &desc;
        }

        virtual ~D3D11VersionedRootSignatureDeserializer() {}
    };

    // Create and return the versioned deserializer
    D3D11VersionedRootSignatureDeserializer* impl = new D3D11VersionedRootSignatureDeserializer(
        serialized_root_signature, serialized_root_signature_size);
    
    HRESULT hr = impl->QueryInterface(riid, deserializer);
    impl->Release();  // Release our reference, QueryInterface added one if successful
    
    return hr;
}

extern "C" HRESULT WINAPI D3D12SerializeVersionedRootSignature(
    const D3D12_VERSIONED_ROOT_SIGNATURE_DESC* desc, ID3DBlob** blob,
    ID3DBlob** error_blob) {
    TRACE("desc %p, blob %p, error_blob %p.", desc, blob, error_blob);

    // For MVP, we don't implement versioned root signature serialization yet
    return E_NOTIMPL;
}
