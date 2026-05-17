#pragma once

#include "../System.h"

class Game;

namespace shadder::ecs {

class RenderSystem : public ISystem {
    Game* game_;
public:
    explicit RenderSystem(Game* g) : game_(g) {}
    void OnUpdate(World& world, float deltaTime) override;
};

} // namespace shadder::ecs
