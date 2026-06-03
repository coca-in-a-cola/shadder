#pragma once

#include "core/ecs/System.h"

class Game;

class RenderSystem : public ISystem {
    Game* game_;
public:
    explicit RenderSystem(Game* g) : game_(g) {}
    void OnUpdate(World& world, float deltaTime) override;
};
