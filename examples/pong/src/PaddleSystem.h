#pragma once
#include "core/ecs/System.h"

class Game;

class PaddleSystem : public ISystem {
    Game* game_;
public:
    explicit PaddleSystem(Game* g);
    void OnUpdate(World& world, float deltaTime) override;
};
