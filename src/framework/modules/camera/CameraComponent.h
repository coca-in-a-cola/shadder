#pragma once

#include "ecs/Component.h"
#include <DirectXMath.h>

// -----------------------------------------------------------------------------
// CameraComponent
// -----------------------------------------------------------------------------
// Заготовка модуля камеры.
// Содержит все параметры, необходимые для построения View и Projection.
// В будущем позиция/ориентация камеры будут читаться из Transform3D;
// сейчас eye/target/up заданы напрямую как заглушка.
//
// active — если false, система пропускает эту сущность.
//
// projection — режим построения матрицы проекции:
//   PERSPECTIVE — перспектива по fovY/aspect/nearZ/farZ (eye/target/up для View);
//   ORTHO_SCREEN — ортографика в экранных пикселях с началом в левом-верхнем
//                  углу и осью Y, направленной вниз (как в 2D). View = identity,
//                  размеры берутся из окна (screenW/screenH обновляет CameraSystem).
//                  Удобно для 2D-сцен в пиксельных координатах (дождь и т.п.).
// -----------------------------------------------------------------------------
struct CameraComponent final : public ComponentBase {
    enum class Projection { PERSPECTIVE, ORTHO_SCREEN };

    Projection projection = Projection::PERSPECTIVE;

    DirectX::XMFLOAT3 eye    = { 0.0f, 0.0f, -5.0f };
    DirectX::XMFLOAT3 target = { 0.0f, 0.0f,  0.0f };
    DirectX::XMFLOAT3 up     = { 0.0f, 1.0f,  0.0f };

    float fovY   = DirectX::XM_PIDIV4; // 45°
    float aspect = 16.0f / 9.0f;
    float nearZ  = 0.1f;
    float farZ   = 100.0f;

    // Для ORTHO_SCREEN: размеры вьюпорта в пикселях. CameraSystem обновляет их
    // под размер окна. Стартовые значения — запасной вариант до первого кадра.
    float screenW = 1.0f;
    float screenH = 1.0f;

    bool active = true;
};
