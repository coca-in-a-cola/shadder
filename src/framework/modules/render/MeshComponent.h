#pragma once

#include "core/ecs/Component.h"
#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl.h>

struct MeshComponent : public ComponentBase {
public:
    // --- Описание (CPU, заполняет пользователь) ---------------------------
    // ResourceLoader::UploadAll() читает это описание и создаёт GPU-буферы.
    enum class Primitive { NONE, QUAD, CUBE, SPHERE, PLANE };
    Primitive primitive = Primitive::NONE;
    float quadWidth = 0.0f;
    float quadHeight = 0.0f;
    DirectX::XMFLOAT4 quadColor = { 1.0f, 1.0f, 1.0f, 1.0f };

    // CUBE: размер по каждой оси (полноразмерный, вершины на ±size/2).
    float cubeSize = 1.0f;

    // SPHERE: радиус + детализация (stacks по широте, slices по долготе).
    float sphereRadius = 0.5f;
    int sphereSlices = 24;
    int sphereStacks = 16;

    // PLANE: сетка width x height на XZ (Y = 0), сегменты по каждой оси.
    // Нормали смотрят вверх (+Y). Для освещённого пола в 3D-сценах.
    float planeWidth = 10.0f;
    float planeDepth = 10.0f;
    int planeSegmentsX = 1;
    int planeSegmentsZ = 1;
    bool uploaded = false; // выставляет ResourceLoader после создания GPU-ресурсов

    // --- GPU-ресурсы (заполняет ResourceLoader) ---------------------------
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
    D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    uint32_t indexCount = 0;
    uint32_t vertexStride = 0;
    DXGI_FORMAT indexFormat = DXGI_FORMAT_R32_UINT;
};
