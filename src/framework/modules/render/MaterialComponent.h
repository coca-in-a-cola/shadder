#pragma once

#include "ecs/Component.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl.h>

struct MaterialComponent : public ComponentBase {
    static inline ComponentTypeID component_id = INVALID_COMPONENT_TYPE;
    static constexpr const char* ComponentName() noexcept { return "MaterialComponent"; }

public:
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState;
    // future: texture, constant buffer
};
