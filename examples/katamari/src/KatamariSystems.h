#pragma once

#include "core/ecs/System.h"
#include "core/ecs/World.h"
#include "framework/game/Game.h"
#include <DirectXMath.h>
#include <chrono>

// Input system — handles WASD movement for the ball
class KatamariInputSystem : public SystemBase {
public:
    KatamariInputSystem(Game* game, int screenW, int screenH)
        : game_(game), screenW_(screenW), screenH_(screenH) {}

    void Update(World& world, float dt) override {
        auto* input = game_->GetInput();
        if (!input) return;

        // Get input state
        float moveX = 0.0f;
        float moveZ = 0.0f;

        if (input->IsKeyDown('W') || input->IsKeyDown('w')) moveZ += 1.0f;
        if (input->IsKeyDown('S') || input->IsKeyDown('s')) moveZ -= 1.0f;
        if (input->IsKeyDown('A') || input->IsKeyDown('a')) moveX -= 1.0f;
        if (input->IsKeyDown('D') || input->IsKeyDown('d')) moveX += 1.0f;

        // Normalize diagonal movement
        float len = std::sqrt(moveX * moveX + moveZ * moveZ);
        if (len > 0.0f) {
            moveX /= len;
            moveZ /= len;
        }

        // Store input direction in ball component for movement system
        world.ForEach<KatamariBallComponent>([&](Entity e, KatamariBallComponent& ball) {
            ball.inputDirection = { moveX, 0.0f, moveZ };
        });
    }

private:
    Game* game_;
    int screenW_, screenH_;
};

// Movement system — applies physics-based rolling movement
class KatamariMovementSystem : public SystemBase {
public:
    void Update(World& world, float dt) override {
        world.ForEach<KatamariBallComponent, Transform3D>([&](Entity e, KatamariBallComponent& ball, Transform3D& tr) {
            // Calculate movement based on input direction
            DirectX::XMFLOAT3 dir = ball.inputDirection;
            float speed = ball.moveSpeed * dt;

            // Move the ball
            tr.position.x += dir.x * speed;
            tr.position.z += dir.z * speed;

            // Keep ball on ground (Y = radius)
            tr.position.y = ball.radius;

            // Apply simple rotation based on movement (visual feedback for rolling)
            if (std::abs(dir.x) > 0.01f || std::abs(dir.z) > 0.01f) {
                float rollSpeed = speed / ball.radius * 10.0f; // faster roll for smaller ball
                tr.rotation.z -= dir.x * rollSpeed; // roll left/right
                tr.rotation.x += dir.z * rollSpeed; // roll forward/back
            }

            // Reset input for next frame
            ball.inputDirection = { 0.0f, 0.0f, 0.0f };
        });
    }
};

// Pickup system — detects collisions between ball and pickup objects
class KatamariPickupSystem : public SystemBase {
public:
    void Update(World& world, float dt) override {
        // First, get ball info
        DirectX::XMFLOAT3 ballPos = { 0, 0, 0 };
        float ballRadius = 0.5f;

        world.ForEach<KatamariBallComponent, Transform3D>([&](Entity e, KatamariBallComponent& ball, Transform3D& tr) {
            ballPos = tr.position;
            ballRadius = ball.radius;
        });

        // Check each pickup object
        world.ForEach<PickupObjectComponent, Transform3D, BoundingSphereComponent>([&](Entity e, PickupObjectComponent& pickup, Transform3D& tr, BoundingSphereComponent& bs) {
            if (pickup.pickedUp) return;

            // Simple sphere-sphere collision
            DirectX::XMVECTOR ballVec = DirectX::XMLoadFloat3(&ballPos);
            DirectX::XMVECTOR objVec = DirectX::XMLoadFloat3(&tr.position);

            DirectX::XMVECTOR diff = DirectX::XMVectorSubtract(objVec, ballVec);
            float distSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(diff));
            float combinedRadius = ballRadius + bs.radius;

            if (distSq < combinedRadius * combinedRadius) {
                // Check if object is smaller than ball (can only pick up smaller objects)
                if (bs.radius < ballRadius * 0.8f) {
                    PickupObject(world, e, ballRadius, pickup, tr, bs);
                }
            }
        });
    }

private:
    void PickupObject(World& world, Entity pickupEntity, float ballRadius,
                      PickupObjectComponent& pickup, Transform3D& tr, BoundingSphereComponent& bs) {
        pickup.pickedUp = true;

        // Scale down the object and attach to ball visually
        tr.scale = { 0.1f, 0.1f, 0.1f };
        tr.position.y = ballRadius + 0.1f; // stick on top

        // Update game state
        world.ForEach<KatamariGameStateComponent>([&](Entity e, KatamariGameStateComponent& gs) {
            gs.objectsCollected++;
            // Notify ball component of growth
            world.ForEach<KatamariBallComponent>([&](Entity be, KatamariBallComponent& ball) {
                ball.pendingGrowth += pickup.value * ball.growthRate;
            });
        });
    }
};

// Camera system — follows the ball from behind
class KatamariCameraSystem : public SystemBase {
public:
    KatamariCameraSystem() = default;

    void Update(World& world, float dt) override {
        // Find ball position
        DirectX::XMFLOAT3 ballPos = { 0, 0, 0 };
        float ballRadius = 0.5f;
        DirectX::XMFLOAT3 ballRotation = { 0, 0, 0 };

        world.ForEach<KatamariBallComponent, Transform3D>([&](Entity e, KatamariBallComponent& ball, Transform3D& tr) {
            ballPos = tr.position;
            ballRadius = ball.radius;
            ballRotation = tr.rotation;
        });

        // Update camera to follow ball
        world.ForEach<CameraComponent, Transform3D>([&](Entity e, CameraComponent& cam, Transform3D& tr) {
            if (!cam.active) return;

            // Camera offset: behind and above the ball
            float camDistance = 8.0f + ballRadius * 2.0f;
            float camHeight = 4.0f + ballRadius;

            // Calculate camera position based on ball's rotation (yaw)
            float yaw = ballRotation.y; // assuming Y is up, rotation around Y
            float camX = ballPos.x - std::sin(yaw) * camDistance;
            float camZ = ballPos.z - std::cos(yaw) * camDistance;
            float camY = ballPos.y + camHeight;

            tr.position = { camX, camY, camZ };

            // Camera looks at ball
            tr.rotation.x = -0.3f; // slight downward angle
            tr.rotation.y = yaw;
            tr.rotation.z = 0.0f;
        });
    }
};

// Growth system — gradually increases ball size based on collected objects
class KatamariGrowthSystem : public SystemBase {
public:
    void Update(World& world, float dt) override {
        world.ForEach<KatamariBallComponent, Transform3D, BoundingSphereComponent>([&](Entity e, KatamariBallComponent& ball, Transform3D& tr, BoundingSphereComponent& bs) {
            if (ball.pendingGrowth > 0.0f) {
                float growth = std::min(ball.pendingGrowth, 0.05f); // cap per frame
                ball.radius += growth;
                ball.pendingGrowth -= growth;

                // Update visual scale
                float scale = ball.radius * 2.0f; // diameter
                tr.scale = { scale, scale, scale };
                tr.position.y = ball.radius; // keep on ground

                // Update bounding sphere
                bs.radius = ball.radius;

                // Cap at max radius
                if (ball.radius > ball.maxRadius) {
                    ball.radius = ball.maxRadius;
                    ball.pendingGrowth = 0.0f;
                }
            }
        });
    }
};

// Game state system — win/lose conditions, timer
class KatamariGameStateSystem : public SystemBase {
public:
    void Update(World& world, float dt) override {
        world.ForEach<KatamariGameStateComponent, KatamariBallComponent>([&](Entity e, KatamariGameStateComponent& gs, KatamariBallComponent& ball) {
            if (gs.gameWon || gs.gameLost) return;

            // Check win condition
            if (ball.radius >= gs.targetRadius) {
                gs.gameWon = true;
                std::cout << "[Katamari] YOU WIN! Ball radius: " << ball.radius << " (target: " << gs.targetRadius << ")\n";
                return;
            }

            // Check time limit
            auto now = std::chrono::steady_clock::now();
            float elapsed = std::chrono::duration<float>(now - gs.startTime).count();
            if (elapsed >= gs.timeLimit) {
                gs.gameLost = true;
                std::cout << "[Katamari] TIME'S UP! Ball radius: " << ball.radius << " (target: " << gs.targetRadius << ")\n";
                return;
            }

            // Periodic status update
            static float lastPrint = 0.0f;
            lastPrint += dt;
            if (lastPrint >= 5.0f) {
                std::cout << "[Katamari] Radius: " << ball.radius << "/" << gs.targetRadius
                          << " | Objects: " << gs.objectsCollected << "/" << gs.totalObjects
                          << " | Time: " << (gs.timeLimit - elapsed) << "s\n";
                lastPrint = 0.0f;
            }
        });
    }
};

// Add inputDirection and pendingGrowth to KatamariBallComponent
namespace shadder {
    // Extension of KatamariBallComponent with runtime fields
    struct KatamariBallComponentRuntime {
        DirectX::XMFLOAT3 inputDirection = { 0.0f, 0.0f, 0.0f };
        float pendingGrowth = 0.0f;
    };
}