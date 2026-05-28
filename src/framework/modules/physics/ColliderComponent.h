#pragma once

#include "ecs/Component.h"
#include <DirectXMath.h>

struct ColliderComponent : public ComponentBase {
    static inline ComponentTypeID component_id = INVALID_COMPONENT_TYPE;
    static constexpr const char* ComponentName() noexcept { return "ColliderComponent"; }

public:
    // Half-extents for AABB (x,y for 2D, z ignored for Pong)
    DirectX::XMFLOAT3 halfExtents = {0.5f, 0.5f, 0.0f};

    ColliderComponent() = default;
    ColliderComponent(float halfWidth, float halfHeight)
        : halfExtents(halfWidth, halfHeight, 0.0f) {}
};
