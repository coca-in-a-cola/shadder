#pragma once

#include "core/ecs/System.h"
#include "core/ecs/EcsTypes.h"
#include "util/callable/callable.h"

class CollisionSystem : public ISystem {
public:
    Signal<Entity, Entity> OnCollision;
    void OnUpdate(World& world, float deltaTime) override;
};
