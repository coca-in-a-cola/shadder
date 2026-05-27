#pragma once

#include "core/ecs/Component.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl.h>

namespace shadder {

struct MaterialComponent : public ComponentBase {
    SHADDER_COMPONENT_HEADER(MaterialComponent)
public:
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState;
    // future: texture, constant buffer
};

} // namespace shadder
