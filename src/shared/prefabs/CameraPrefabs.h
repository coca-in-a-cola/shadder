#pragma once

// -----------------------------------------------------------------------------
// Camera prefabs — migrated to the PackedScene mechanism (Godot-style).
//
// The canonical builders return PackedScene:
//     OrthoCameraScene(w, h)   -> single-node scene (CameraComponent)
//     PerspectiveCameraScene() -> single-node scene (CameraComponent)
//
// The legacy Prefab facades are thin bridges (Prefab::FromPackedScene) with
// unchanged signatures and unchanged runtime behavior for call sites:
//     OrthoCameraPrefab(w, h).Instantiate(world)  // one entity + CameraComponent
// (pong keeps using them; see SH-0 zone).
//
// NOTE ( lifetime ): FromPackedScene COPIES the scene, so the facade
// does not depend on the temporary returned by the canonical builders.
// Nested scenes added via SceneBuilder::Inline remain SHARED BY POINTER —
// a referenced scene must outlive every Instantiate() of the built scene.
// -----------------------------------------------------------------------------

#include "core/prefab/Prefab.h"
#include "core/scene/PackedScene.h"
#include "framework/modules/camera/CameraComponent.h"

// Canonical: single-node PackedScene with an ORTHO_SCREEN camera.
inline PackedScene OrthoCameraScene(float screenW, float screenH,
                                    float nearZ = -1.0f, float farZ = 1.0f) {
    return SceneBuilder()
        .Node("Camera")
        .With<CameraComponent>([=](CameraComponent& c) {
            c.projection = CameraComponent::Projection::ORTHO_SCREEN;
            c.screenW = screenW;
            c.screenH = screenH;
            c.nearZ = nearZ;
            c.farZ = farZ;
            c.active = true;
        })
        .End()
        .Build();
}

// Canonical: single-node PackedScene with a PERSPECTIVE camera.
inline PackedScene PerspectiveCameraScene(float fovY, float aspect,
                                          float nearZ = 0.1f, float farZ = 100.0f) {
    return SceneBuilder()
        .Node("Camera")
        .With<CameraComponent>([=](CameraComponent& c) {
            c.projection = CameraComponent::Projection::PERSPECTIVE;
            c.fovY = fovY;
            c.aspect = aspect;
            c.nearZ = nearZ;
            c.farZ = farZ;
            c.active = true;
        })
        .End()
        .Build();
}

// Legacy facade: unchanged signature, scene-backed instantiation.
// Bind the returned Prefab to a temporary (does not outlive the scene:
// SceneApplier only holds a pointer to the temporary scene returned above).
inline Prefab OrthoCameraPrefab(float screenW, float screenH,
                                float nearZ = -1.0f, float farZ = 1.0f) {
    return Prefab::FromPackedScene(OrthoCameraScene(screenW, screenH, nearZ, farZ));
}

inline Prefab PerspectiveCameraPrefab(float fovY, float aspect,
                                      float nearZ = 0.1f, float farZ = 100.0f) {
    return Prefab::FromPackedScene(PerspectiveCameraScene(fovY, aspect, nearZ, farZ));
}
