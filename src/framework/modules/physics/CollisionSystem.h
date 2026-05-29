#pragma once

#include "ecs/System.h"
#include "ecs/EcsTypes.h"
#include "core/callable/callable.h"

class CollisionSystem : public ISystem {
public:
    Signal<Entity, Entity> OnCollision;
    void OnUpdate(World& world, float deltaTime) override;
};
