#pragma once

// SolarMotionSystem — орбиты планет/лун + вращение вокруг своей оси.
// См. SolarMotionSystem.cpp.

#include "core/ecs/System.h"

class SolarMotionSystem : public ISystem {
public:
    void OnUpdate(World& world, float deltaTime) override;
};
