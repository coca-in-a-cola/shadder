#pragma once

// -----------------------------------------------------------------------------
// Quad prefab — migrated to the PackedScene mechanism (Godot-style).
//
// Canonical builder returns PackedScene:
//     QuadScene(w, h, color) -> single-node scene (MeshComponent+Material)
//
// The legacy Prefab facade keeps the unchanged signature for call sites:
//     QuadPrefab(w, h, color).Instantiate(world)  // one entity, NO forced
//                                                 // Transform3D (pong adds
//                                                 // its own right after)
// (pong keeps using the facade; see SH-0 zone).
//
// NOTE ( lifetime ): SceneApplier captures the scene BY POINTER (shared
// reference, Godot instanced-scene semantics). A scene-backed Prefab must not
// outlive the scene it wraps — keep the scene alive as long as the prefab is
// instantiated (e.g. bind to a temporary Prefab or to a longer-lived scene).
// -----------------------------------------------------------------------------

#include "core/prefab/Prefab.h"
#include "core/scene/PackedScene.h"
#include "framework/modules/render/MeshComponent.h"
#include "framework/modules/render/MaterialComponent.h"
#include <DirectXMath.h>

// Canonical: single-node PackedScene with a QUAD MeshComponent + MaterialComponent.
inline PackedScene QuadScene(float width, float height,
                             DirectX::XMFLOAT4 color = {1.0f, 1.0f, 1.0f, 1.0f}) {
    return SceneBuilder()
        .Node("Quad")
        .With<MeshComponent>([=](MeshComponent& m) {
            m.primitive = MeshComponent::Primitive::QUAD;
            m.quadWidth = width;
            m.quadHeight = height;
            m.quadColor = color;
        })
        .With<MaterialComponent>([](MaterialComponent&) {})
        .End()
        .Build();
}

// Legacy facade: unchanged signature, scene-backed instantiation.
// Instantiate keeps legacy semantics: ONE entity, NO forced Transform3D.
inline Prefab QuadPrefab(float width, float height,
                         DirectX::XMFLOAT4 color = {1.0f, 1.0f, 1.0f, 1.0f}) {
    return Prefab::FromPackedScene(QuadScene(width, height, color));
}
