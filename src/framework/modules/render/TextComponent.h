#pragma once

#include "core/ecs/Component.h"
#include "framework/modules/transform/Transform3D.h"
#include <DirectXMath.h>
#include <string>

namespace shadder {

struct TextComponent final : public ComponentBase {
public:
    // Text content
    std::string text = "";

    // Color (RGBA)
    DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

    // Font size in pixels (height of each character)
    float fontSize = 48.0f;

    // Character spacing multiplier (1.0 = normal)
    float charSpacing = 1.0f;

    // Line spacing multiplier (1.0 = normal)
    float lineSpacing = 1.0f;

    // Horizontal alignment: 0 = left, 0.5 = center, 1 = right
    float hAlign = 0.0f;

    // Vertical alignment: 0 = top, 0.5 = center, 1 = bottom
    float vAlign = 0.0f;

    // Internal: dirty flag for when text changes
    bool dirty = true;
};

} // namespace shadder