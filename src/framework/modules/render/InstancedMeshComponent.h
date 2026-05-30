#pragma once

#include "ecs/Component.h"
#include <d3d11.h>
#include <wrl.h>
#include <cstdint>

// -----------------------------------------------------------------------------
// InstancedMeshComponent
// -----------------------------------------------------------------------------
// Хранит ОДНУ общую геометрию (vertex/index буферы) и динамический instance-буфер.
// Все сущности, помеченные InstanceTag, рисуются за ОДИН вызов
// DrawIndexedInstanced одним и тем же HLSL-шейдером.
//
// Это решает проблему текущего RenderSystem, который создавал новый
// constant buffer на КАЖДУЮ сущность КАЖДЫЙ кадр. Здесь все per-instance
// данные (трансформы/цвета) пишутся в один buffer и грузятся одним апдейтом.
//
// Сущность с этим компонентом — это "рендер-батч", а не отдельный треугольник.
// -----------------------------------------------------------------------------
struct InstancedMeshComponent final : public ComponentBase {
public:
    // Общая геометрия одного экземпляра (например, один треугольник).
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;

    // Динамический per-instance буфер (slot 1 в IA).
    Microsoft::WRL::ComPtr<ID3D11Buffer> instanceBuffer;

    D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    uint32_t indexCount = 0;       // индексов на один экземпляр
    uint32_t vertexStride = 0;     // байт на вершину (slot 0)
    uint32_t instanceStride = 0;   // байт на экземпляр (slot 1)
    uint32_t instanceCapacity = 0; // сколько экземпляров вмещает instanceBuffer
    uint32_t instanceCount = 0;    // сколько реально рисуем в этом кадре

    DXGI_FORMAT indexFormat = DXGI_FORMAT_R32_UINT;
};
