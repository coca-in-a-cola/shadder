#pragma once
#include "core/ecs/System.h"

class BallSystem : public ISystem {
    float screenW_;
    float screenH_;
    float paddleW_;
    float paddleH_;
    float ballSize_;
public:
    BallSystem(float screenW, float screenH,
               float paddleW, float paddleH,
               float ballSize);
    void OnUpdate(World& world, float deltaTime) override;

    static float DifficultyCurve(int ballHits);
};
