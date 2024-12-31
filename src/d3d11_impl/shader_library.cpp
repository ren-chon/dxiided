#include "d3d11_impl/shader_library.hpp"
#include "common/debug.hpp"
#include <d3dcompiler.h>

namespace dxiided {

Microsoft::WRL::ComPtr<ID3D11VertexShader> 
D3D11ShaderLibrary::GetBuiltinVertexShader(ID3D11Device* device, uint64_t specialValue) {
    // Extract shader type and variant from special value
    uint32_t shaderType = static_cast<uint32_t>(specialValue >> 32);
    uint32_t variant = static_cast<uint32_t>(specialValue);
    
    TRACE("Looking up built-in vertex shader: type=%u, variant=%u", shaderType, variant);
    
    // Type 1 = vertex shader
    if (shaderType != 1) {
        WARN("Unknown shader type: %u", shaderType);
        return nullptr;
    }

    // Handle known variants
    switch (variant) {
        case 2: // 0x0000000100000002 - Fullscreen triangle
            TRACE("Creating fullscreen triangle vertex shader");
            return CreateFullscreenTriangleVS(device);
            
        default:
            WARN("Unknown vertex shader variant: %u", variant);
            return nullptr;
    }
}

Microsoft::WRL::ComPtr<ID3D11VertexShader>
D3D11ShaderLibrary::CreateFullscreenTriangleVS(ID3D11Device* device) {
    // Common fullscreen triangle vertex shader
    const char* hlsl = R"(
        void main(uint id : SV_VertexID,
                 out float4 pos : SV_Position,
                 out float2 tex : TEXCOORD0) {
            tex = float2((id << 1) & 2, id & 2);
            pos = float4(tex * float2(2,-2) + float2(-1,1), 0, 1);
        }
    )";
    
    // Compile shader
    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    
    TRACE("Compiling fullscreen triangle vertex shader");
    HRESULT hr = D3DCompile(
        hlsl, strlen(hlsl),
        nullptr, nullptr, nullptr,
        "main", "vs_4_0",
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3,
        0, &blob, &errorBlob);
        
    if (FAILED(hr)) {
        if (errorBlob) {
            ERR("Failed to compile fullscreen VS: %s", 
                static_cast<const char*>(errorBlob->GetBufferPointer()));
        } else {
            ERR("Failed to compile fullscreen VS, hr %#x", hr);
        }
        return nullptr;
    }

    TRACE("Successfully compiled shader, creating vertex shader");

    // Create shader
    Microsoft::WRL::ComPtr<ID3D11VertexShader> shader;
    hr = device->CreateVertexShader(
        blob->GetBufferPointer(),
        blob->GetBufferSize(),
        nullptr,
        &shader);
        
    if (FAILED(hr)) {
        ERR("Failed to create fullscreen VS, hr %#x", hr);
        return nullptr;
    }

    TRACE("Successfully created fullscreen triangle vertex shader");
    return shader;
}

} // namespace dxiided