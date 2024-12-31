#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>

namespace dxiided {

class D3D11ShaderLibrary {
public:
    // Get a built-in vertex shader based on a special value
    static Microsoft::WRL::ComPtr<ID3D11VertexShader> GetBuiltinVertexShader(
        ID3D11Device* device, uint64_t specialValue);

private:
    // Create a fullscreen triangle vertex shader
    static Microsoft::WRL::ComPtr<ID3D11VertexShader> CreateFullscreenTriangleVS(
        ID3D11Device* device);

    // Add more shader creation helpers as needed
};

} // namespace dxiided