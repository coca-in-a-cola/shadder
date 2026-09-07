#pragma once

#include "core/prefab/Prefab.h"
#include "framework/modules/render/MeshComponent.h"
#include "framework/modules/render/MaterialComponent.h"
#include <DirectXMath.h>

inline Prefab QuadPrefab(float width, float height,
                          DirectX::XMFLOAT4 color = {1.0f, 1.0f, 1.0f, 1.0f}) {
    Prefab p;
    p.With<MeshComponent>([=](MeshComponent& m) {
        m.primitive = MeshComponent::Primitive::QUAD;
        m.quadWidth = width;
        m.quadHeight = height;
        m.quadColor = color;
    });
    p.With<MaterialComponent>([](MaterialComponent&) {});
    return p;
}
