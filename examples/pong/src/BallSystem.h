#pragma once
#include "core/ecs/System.h"

class Game;

class BallSystem : public ISystem {
    Game* game_;
    float paddleW_;
    float paddleH_;
    float ballSize_;
public:
    BallSystem(Game* game, float paddleW, float paddleH,
               float ballSize);
    void OnUpdate(World& world, float deltaTime) override;

    static float DifficultyCurve(int ballHits);
};
