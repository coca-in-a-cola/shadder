#pragma once
#include "ecs/System.h"

namespace shadder {

class MovementSystem : public ISystem {
public:
    void OnUpdate(World& world, float deltaTime) override;
};

} // namespace shadder
