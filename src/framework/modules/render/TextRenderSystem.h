#pragma once

#include "core/ecs/System.h"
#include "framework/modules/render/TextComponent.h"
#include <d3d11.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <string>
#include <vector>

class Game;

namespace shadder {

struct TextVertex {
    DirectX::XMFLOAT4 pos;
    DirectX::XMFLOAT2 uv;
    DirectX::XMFLOAT4 color;
};

class TextRenderSystem : public ISystem {
    Game* game_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> fontTexture_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> fontSRV_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blendState_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilState_;
    bool resourcesCreated_ = false;

    // Per-character vertex buffer (recreated when text changes)
    struct CharQuad {
        TextVertex vertices[6]; // 2 triangles = 6 vertices
    };

    void CreateFontTexture(ID3D11Device* device);
    void CreateShaders(ID3D11Device* device);
    void CreateStates(ID3D11Device* device);
    void EnsureResources(ID3D11Device* device);

    // Build vertex data for a single text entity
    void BuildTextQuads(World& world, Entity entity, TextComponent& text, Transform3D& transform,
                        std::vector<TextVertex>& outVertices);

public:
    explicit TextRenderSystem(Game* g) : game_(g) {}
    void OnUpdate(World& world, float deltaTime) override;
};

} // namespace shadder