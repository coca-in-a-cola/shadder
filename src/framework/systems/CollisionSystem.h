#pragma once
#include "ecs/System.h"
#include "ecs/EcsTypes.h"
#include "shared/delegates/Delegates.h"


class CollisionSystem : public ISystem {
public:
    MulticastDelegate<Entity, Entity> OnCollision;
    void OnUpdate(World& world, float deltaTime) override;
};


