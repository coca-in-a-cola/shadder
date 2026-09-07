#pragma once

#include "core/prefab/Prefab.h"
#include "framework/modules/camera/CameraComponent.h"

inline Prefab OrthoCameraPrefab(float screenW, float screenH,
                                float nearZ = -1.0f, float farZ = 1.0f) {
    Prefab p;
    p.With<CameraComponent>([=](CameraComponent& c) {
        c.projection = CameraComponent::Projection::ORTHO_SCREEN;
        c.screenW = screenW;
        c.screenH = screenH;
        c.nearZ = nearZ;
        c.farZ = farZ;
        c.active = true;
    });
    return p;
}

inline Prefab PerspectiveCameraPrefab(float fovY, float aspect,
                                       float nearZ = 0.1f, float farZ = 100.0f) {
    Prefab p;
    p.With<CameraComponent>([=](CameraComponent& c) {
        c.projection = CameraComponent::Projection::PERSPECTIVE;
        c.fovY = fovY;
        c.aspect = aspect;
        c.nearZ = nearZ;
        c.farZ = farZ;
        c.active = true;
    });
    return p;
}
