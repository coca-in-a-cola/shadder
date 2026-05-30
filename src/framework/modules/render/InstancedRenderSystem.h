#pragma once

#include "ecs/System.h"

class Game;

// -----------------------------------------------------------------------------
// InstancedRenderSystem
// -----------------------------------------------------------------------------
// Собирает все сущности с InstanceTag + Transform3D, пишет их per-instance
// данные (world-матрица + цвет) в динамический instance-буфер своего батча
// (InstancedMeshComponent + MaterialComponent на сущности-батче) и рисует ВСЁ
// одним вызовом DrawIndexedInstanced на батч.
//
// Один HLSL-шейдер. Один draw call на тысячи треугольников.
// -----------------------------------------------------------------------------
class InstancedRenderSystem : public ISystem {
    Game* game_;
public:
    explicit InstancedRenderSystem(Game* g) : game_(g) {}
    void OnUpdate(World& world, float deltaTime) override;
};
