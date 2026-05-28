#pragma once
#include "ecs/System.h"

class Game;


class InputSystem : public ISystem {
    Game* game_;
public:
    explicit InputSystem(Game* g) : game_(g) {}
    void OnUpdate(World& world, float deltaTime) override;
};


