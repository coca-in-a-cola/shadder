#pragma once

#include "ecs/System.h"

class Game;

namespace shadder {

class RenderSystem : public ISystem {
    Game* game_;
public:
    explicit RenderSystem(Game* g) : game_(g) {}
    void OnUpdate(World& world, float deltaTime) override;
};

} // namespace shadder
