#pragma once

#include "GameComponent.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <wrl.h>

class TriangleComponent : public GameComponent {
public:
    TriangleComponent(Game* inGame);
    ~TriangleComponent() override;

    void Initialize() override;
    void Update(float deltaTime) override;
    void Draw() override;
    void DestroyResources() override;

private:
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> layout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertices;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rastState;

    Microsoft::WRL::ComPtr<ID3DBlob> vertexShaderByteCode;
    Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderByteCode;

    DirectX::XMFLOAT4 points[8];
};
