#pragma once

// KatamariBallSystem — управление шаром (SH-S2 п.4): WASD катит шар по
// плоскости, шар визуально вращается (кватернион), Y фиксирован = радиус.
// См. KatamariSystems.cpp.

#include "core/ecs/System.h"
#include "core/ecs/EcsTypes.h"

class Game;

class KatamariBallSystem : public ISystem {
public:
    explicit KatamariBallSystem(Game* game) : game_(game) {}
    void OnUpdate(World& world, float deltaTime) override;

private:
    Game* game_;
};

// KatamariPickupSystem — подбор объектов (SH-S2 п.4-5): пересечение bounding
// сфер; подбираются только объекты меньше шара; шар растёт.
class KatamariPickupSystem : public ISystem {
public:
    void OnUpdate(World& world, float deltaTime) override;
};

// KatamariCameraSystem — камера следует за шаром (SH-S2 п.6): орбитально-
// независимая, всегда за спиной (как в Katamari Damacy). Колесо мыши — зум.
class KatamariCameraSystem : public ISystem {
public:
    explicit KatamariCameraSystem(Game* game) : game_(game) {}
    void OnUpdate(World& world, float deltaTime) override;

private:
    Game* game_;
    float distance_ = 10.0f; // дистанция камеры от шара (зум колесом)
    float height_ = 5.0f;    // высота камеры над шаром
    int wheelAccum_ = 0;
};

// KatamariStatsSystem — статус в stdout (собрано/всего, радиус).
class KatamariStatsSystem : public ISystem {
public:
    void OnUpdate(World& world, float deltaTime) override;

private:
    float sincePrint_ = 0.0f;
};
