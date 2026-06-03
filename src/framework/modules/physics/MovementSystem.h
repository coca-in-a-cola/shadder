#pragma once

#include "core/ecs/System.h"

class MovementSystem : public ISystem {
public:
    void OnUpdate(World& world, float deltaTime) override;
};
