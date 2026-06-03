#pragma once

#include "core/ecs/System.h"
#include <d3d11.h>
#include <wrl.h>
#include <DirectXMath.h>

class Game;

// -----------------------------------------------------------------------------
// CameraSystem
// -----------------------------------------------------------------------------
// Обновляет ViewProjection матрицу активной CameraComponent и загружает
// её в constant buffer slot b1 (PRE_RENDER фаза).
//
// Система создаёт и масштабирует буфер под ресайз окна, а также
// пересчитывает aspect ratio. Если ни одной активной камеры нет — ничего
// не происходит.
// -----------------------------------------------------------------------------
class CameraSystem : public ISystem {
    Game* game_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> cb_;
    int width_ = 0;
    int height_ = 0;

    struct CameraBuffer {
        DirectX::XMFLOAT4X4 viewProj;
    };

public:
    explicit CameraSystem(Game* g) : game_(g) {}
    void OnUpdate(World& world, float deltaTime) override;
};
