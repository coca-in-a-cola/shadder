#pragma once

#include "ecs/Component.h"
#include <DirectXMath.h>

struct VelocityComponent : public ComponentBase {
    static inline ComponentTypeID component_id = INVALID_COMPONENT_TYPE;
    static constexpr const char* ComponentName() noexcept { return "VelocityComponent"; }

public:
    DirectX::XMFLOAT3 velocity = {0.0f, 0.0f, 0.0f};

    VelocityComponent() = default;
    explicit VelocityComponent(const DirectX::XMFLOAT3& vel) : velocity(vel) {}
};
