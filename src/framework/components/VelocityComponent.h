#pragma once
#include "core/ecs/Component.h"
#include <DirectXMath.h>

namespace shadder {

struct VelocityComponent : public ComponentBase {
    SHADDER_COMPONENT_HEADER(VelocityComponent)

    DirectX::XMFLOAT3 velocity = {0.0f, 0.0f, 0.0f};

    VelocityComponent() = default;
    explicit VelocityComponent(const DirectX::XMFLOAT3& vel) : velocity(vel) {}
};

} // namespace shadder
