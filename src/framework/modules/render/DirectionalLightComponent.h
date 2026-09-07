#pragma once

#include "core/ecs/Component.h"
#include <DirectXMath.h>

// -----------------------------------------------------------------------------
// DirectionalLightComponent
// -----------------------------------------------------------------------------
// Godot-style directional light (like Light3D/DirectionalLight3D).
// Direction is derived from Transform3D.rotation (forward vector).
// Color + intensity packed into cbuffer for pixel shader.
// -----------------------------------------------------------------------------
struct DirectionalLightComponent final : public ComponentBase {
    DirectX::XMFLOAT3 color = { 1.0f, 1.0f, 1.0f }; // RGB
    float intensity = 1.0f;                          // multiplier

    // Light direction in world space (computed from Transform3D if present)
    // If no Transform3D, defaults to (0, -1, 0) i.e. straight down.
    DirectX::XMFLOAT3 direction = { 0.0f, -1.0f, 0.0f };

    bool active = true;
};