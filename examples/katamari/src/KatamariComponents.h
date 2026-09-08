#pragma once

#include "core/ecs/Component.h"
#include <DirectXMath.h>

// Katamari ball component — the player-controlled rolling ball
struct KatamariBallComponent final : public ComponentBase {
    float radius = 0.5f;        // current radius of the ball
    float mass = 1.0f;          // mass for physics
    float moveSpeed = 5.0f;     // movement speed
    float growthRate = 0.01f;   // how fast radius grows per pickup value
    float maxRadius = 20.0f;    // maximum possible radius
};

// Pickup object component — objects that can be picked up by the ball
struct PickupObjectComponent final : public ComponentBase {
    float radius = 0.5f;        // object's radius (from geometry)
    bool pickedUp = false;      // whether already picked up
    int value = 1;              // size/value contribution to ball growth
    float stickOffset = 0.0f;   // vertical offset when stuck to ball
};

// Bounding sphere component — for collision detection
struct BoundingSphereComponent final : public ComponentBase {
    float radius = 1.0f;        // sphere radius in world units
    DirectX::XMFLOAT3 offset = { 0.0f, 0.0f, 0.0f }; // offset from entity position
};

// Game state component — win/lose conditions, timer
struct KatamariGameStateComponent final : public ComponentBase {
    shadder::Entity ballEntity;       // reference to the ball entity
    float targetRadius = 5.0f;        // win condition radius
    float timeLimit = 300.0f;         // time limit in seconds
    std::chrono::steady_clock::time_point startTime;
    bool gameWon = false;
    bool gameLost = false;
    int objectsCollected = 0;
    int totalObjects = 0;
};