#pragma once
#include "core/ecs/System.h"

class Game;

class PaddleSystem : public ISystem {
    Game* game_;
    float screenH_;
public:
    PaddleSystem(Game* g, float screenH);
    void OnUpdate(World& world, float deltaTime) override;
};
