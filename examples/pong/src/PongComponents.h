#pragma once
#include "ecs/Component.h"
#include <chrono>

struct PongStateComponent final : public ComponentBase {
    enum State : uint32_t {
        COOLDOWN = 0,
        NORMAL,
        GAMEOVER
    };
    State state = COOLDOWN;
    int score1 = 0;
    int score2 = 0;
    int ballHits = 0;
    float speedMultiplier = 1.0f;
    std::chrono::steady_clock::time_point continueTime;
};

struct PlayerPaddleTag final : public ComponentBase {};

struct AiPaddleTag final : public ComponentBase {};

struct BallTag final : public ComponentBase {};
