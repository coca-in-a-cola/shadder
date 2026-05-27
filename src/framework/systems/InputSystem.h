#pragma once
#include "core/ecs/System.h"

namespace shadder {

class InputSystem : public ISystem {
    Game* game_;
public:
    explicit InputSystem(Game* g) : game_(g) {}
    void OnUpdate(World& world, float deltaTime) override;
};

} // namespace shadder
