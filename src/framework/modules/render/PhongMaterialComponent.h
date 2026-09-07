#pragma once

#include "core/ecs/Component.h"
#include <DirectXMath.h>

// -----------------------------------------------------------------------------
// PhongMaterialComponent
// -----------------------------------------------------------------------------
// Phong shading material parameters (per-object constant buffer).
// ambient/diffuse/specular colors + shininess exponent.
// -----------------------------------------------------------------------------
struct PhongMaterialComponent final : public ComponentBase {
    // Ambient term (constant base color)
    DirectX::XMFLOAT3 ambient = { 0.1f, 0.1f, 0.1f };

    // Diffuse term (Lambert: max(dot(N, L), 0))
    DirectX::XMFLOAT3 diffuse = { 0.8f, 0.8f, 0.8f };

    // Specular term (Blinn-Phong: pow(max(dot(N, H), 0), shininess))
    DirectX::XMFLOAT3 specular = { 1.0f, 1.0f, 1.0f };

    // Shininess exponent (higher = tighter highlight)
    float shininess = 32.0f;

    // Shader path (empty = use framework default phong shader)
    std::wstring shaderPath;
    bool uploaded = false;

    // GPU resources (filled by ResourceLoader)
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState;
};