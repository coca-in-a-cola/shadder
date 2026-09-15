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

    // One orthographic shadow map. Disabled by default: examples opt in.
    bool shadowsEnabled = false;
    unsigned shadowResolution = 2048;
    float shadowOrthoSize = 20.0f;
    float shadowDistance = 30.0f;
    float shadowNearZ = 0.1f;
    float shadowFarZ = 100.0f;
    DirectX::XMFLOAT3 shadowCenter = { 0.0f, 0.0f, 0.0f };
    int shadowDepthBias = 1200;
    float shadowSlopeScaledDepthBias = 2.0f;
    float shadowBias = 0.0015f;

    bool active = true;
};
