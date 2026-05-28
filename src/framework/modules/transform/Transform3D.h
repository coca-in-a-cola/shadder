#pragma once

#include "ecs/Component.h"
#include <DirectXMath.h>

struct Transform3D final : public ComponentBase {
    static inline ComponentTypeID component_id = INVALID_COMPONENT_TYPE;
    static constexpr const char* ComponentName() noexcept { return "Transform3D"; }

public:
    DirectX::XMFLOAT3 position{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT4 rotation{0.0f, 0.0f, 0.0f, 1.0f}; // quaternion (x,y,z,w)
    DirectX::XMFLOAT3 scale{1.0f, 1.0f, 1.0f};
};
