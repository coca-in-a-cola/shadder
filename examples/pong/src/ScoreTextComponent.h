#pragma once

#include "core/ecs/Component.h"

// Marks a text entity created in main.cpp so systems can find them:
// 0 = left score (score1), 1 = right score (score2), 2 = center message.
struct ScoreTextComponent final : public ComponentBase {
    int slot = 0;
};
