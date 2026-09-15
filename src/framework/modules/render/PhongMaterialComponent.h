#pragma once

#include "core/ecs/Component.h"
#include <DirectXMath.h>
#include "TextureLoader.h"

// Copyable CPU description. A preset can be assigned to any number of objects.
struct PhongMaterialDesc {
    enum class Shading { Phong, Diffuse };
    DirectX::XMFLOAT3 ambient = { 0.1f, 0.1f, 0.1f };
    DirectX::XMFLOAT3 diffuse = { 0.8f, 0.8f, 0.8f };
    DirectX::XMFLOAT3 specular = { 1.0f, 1.0f, 1.0f };
    float shininess = 32.0f;
    DirectX::XMFLOAT4 baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    std::wstring diffuseTexturePath;
    DirectX::XMFLOAT2 uvScale = { 1.0f, 1.0f };
    DirectX::XMFLOAT2 uvOffset = { 0.0f, 0.0f };
    D3D11_FILTER filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    D3D11_TEXTURE_ADDRESS_MODE addressMode = D3D11_TEXTURE_ADDRESS_WRAP;
    unsigned maxAnisotropy = 8;
    Shading shading = Shading::Phong;
    // Textured materials always use linear lighting. Opt in for solid colors too.
    // False preserves the output of existing, untextured examples.
    bool linearLighting = false;
    bool receivesShadow = true;
    std::wstring shaderPath;
};

// -----------------------------------------------------------------------------
// PhongMaterialComponent
// -----------------------------------------------------------------------------
// Phong shading material parameters (per-object constant buffer).
// ambient/diffuse/specular colors + shininess exponent.
// -----------------------------------------------------------------------------
struct PhongMaterialComponent final : public ComponentBase, public PhongMaterialDesc {
    void SetMaterial(const PhongMaterialDesc& description) {
        static_cast<PhongMaterialDesc&>(*this) = description;
        uploaded = false;
    }
    bool uploaded = false;
    std::shared_ptr<Texture2D> diffuseTexture;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;

    // GPU resources (filled by ResourceLoader)
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState;
};
