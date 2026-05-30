#pragma once

#include "ecs/Component.h"
#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl.h>

struct MeshComponent : public ComponentBase {
public:
    // --- Описание (CPU, заполняет пользователь) ---------------------------
    // ResourceLoader::UploadAll() читает это описание и создаёт GPU-буферы.
    enum class Primitive { NONE, QUAD };
    Primitive primitive = Primitive::NONE;
    float quadWidth = 0.0f;
    float quadHeight = 0.0f;
    DirectX::XMFLOAT4 quadColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    bool uploaded = false; // выставляет ResourceLoader после создания GPU-ресурсов

    // --- GPU-ресурсы (заполняет ResourceLoader) ---------------------------
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
    D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    uint32_t indexCount = 0;
    uint32_t vertexStride = 0;
    DXGI_FORMAT indexFormat = DXGI_FORMAT_R32_UINT;
};
