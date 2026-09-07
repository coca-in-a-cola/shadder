// Katamari Damacy clone — rolling ball picks up objects, grows bigger
// WASD to move the ball, camera follows behind

#include "shadder.hpp"

#include <windows.h>
#include <DirectXMath.h>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <iostream>
#include <vector>
#include <random>

#include "shared/prefabs/CameraPrefabs.h"
#include "shared/prefabs/MeshPrefabs.h"
#include "KatamariComponents.h"
#include "KatamariSystems.h"

using namespace shadder;
using namespace DirectX;

static const int kScreenW = 1280;
static const int kScreenH = 720;
static const float kInitialBallRadius = 0.5f;
static const float kMinPickupRadius = 0.1f;
static const float kMaxPickupRadius = 3.0f;
static const int kNumPickupObjects = 200;

static Game* g_Game = nullptr;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (g_Game && g_Game->MessageHandler(hwnd, msg, wparam, lparam))
        return 0;
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(hwnd, msg, wparam, lparam);
    }
}

int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    HINSTANCE hInstance = GetModuleHandle(nullptr);
    DisplayWin32 display(L"Katamari Damacy", hInstance, kScreenW, kScreenH, WndProc);

    Game game;
    g_Game = &game;
    if (!game.Initialize(&display)) {
        std::cout << "Failed to initialize the game engine!\n";
        return 1;
    }

    World& world = game.GetWorld();

    // Register Katamari-specific components
    world.RegisterComponent<KatamariBallComponent>();
    world.RegisterComponent<PickupObjectComponent>();
    world.RegisterComponent<BoundingSphereComponent>();

    // Camera — follows the ball from behind (third-person)
    PerspectiveCameraPrefab(XM_PIDIV4, static_cast<float>(kScreenW) / kScreenH, 0.1f, 100.0f)
        .Instantiate(world);

    XMFLOAT4 white = { 1.0f, 1.0f, 1.0f, 1.0f };
    XMFLOAT4 groundColor = { 0.2f, 0.5f, 0.2f, 1.0f };

    // Ground plane (large quad)
    {
        Entity e = QuadPrefab(100.0f, 100.0f, groundColor).Instantiate(world);
        auto& tr = world.AddComponent<Transform3D>(e);
        tr.position = { 0.0f, 0.0f, 0.0f };
        tr.rotation = { -XM_PIDIV2, 0.0f, 0.0f }; // rotate to horizontal
        tr.scale = { 1.0f, 1.0f, 1.0f };
        auto& bs = world.AddComponent<BoundingSphereComponent>(e);
        bs.radius = 70.0f; // large enough to cover ground
    }

    // The Katamari ball (player-controlled)
    Entity ballEntity;
    {
        ballEntity = QuadPrefab(kInitialBallRadius * 2.0f, kInitialBallRadius * 2.0f, white).Instantiate(world);
        auto& tr = world.AddComponent<Transform3D>(ballEntity);
        tr.position = { 0.0f, kInitialBallRadius, 0.0f };
        tr.scale = { 1.0f, 1.0f, 1.0f };

        auto& ball = world.AddComponent<KatamariBallComponent>(ballEntity);
        ball.radius = kInitialBallRadius;
        ball.mass = 1.0f;
        ball.moveSpeed = 5.0f;

        auto& bs = world.AddComponent<BoundingSphereComponent>(ballEntity);
        bs.radius = kInitialBallRadius;
    }

    // Spawn pickup objects randomly on the ground
    std::mt19937 rng(static_cast<unsigned>(std::time(nullptr)));
    std::uniform_real_distribution<float> distPos(-45.0f, 45.0f);
    std::uniform_real_distribution<float> distSize(kMinPickupRadius, kMaxPickupRadius);
    std::uniform_real_distribution<float> distColor(0.2f, 1.0f);

    for (int i = 0; i < kNumPickupObjects; ++i) {
        Entity e = QuadPrefab(1.0f, 1.0f, white).Instantiate(world);
        auto& tr = world.AddComponent<Transform3D>(e);

        float size = distSize(rng);
        tr.position = { distPos(rng), size, distPos(rng) };
        tr.scale = { size, size, size };

        XMFLOAT4 objColor = { distColor(rng), distColor(rng), distColor(rng), 1.0f };
        auto& mesh = world.AddComponent<MeshComponent>(e);
        mesh.primitive = MeshComponent::Primitive::QUAD;
        mesh.quadWidth = 1.0f;
        mesh.quadHeight = 1.0f;
        mesh.quadColor = objColor;

        auto& pickup = world.AddComponent<PickupObjectComponent>(e);
        pickup.radius = size;
        pickup.pickedUp = false;
        pickup.value = static_cast<int>(size * 10.0f);

        auto& bs = world.AddComponent<BoundingSphereComponent>(e);
        bs.radius = size;
    }

    // Katamari game state entity
    {
        Entity e = world.CreateEntity();
        auto& gs = world.AddComponent<KatamariGameStateComponent>(e);
        gs.ballEntity = ballEntity;
        gs.targetRadius = 5.0f; // win condition: grow to this radius
        gs.timeLimit = 300.0f;  // 5 minutes
        gs.startTime = std::chrono::steady_clock::now();
    }

    // Register systems
    world.RegisterSystem<CameraSystem>(SystemPhase::PRE_RENDER, &game);

    world.RegisterSystem<KatamariInputSystem>(SystemPhase::UPDATE, &game, kScreenW, kScreenH);
    world.RegisterSystem<KatamariMovementSystem>(SystemPhase::UPDATE);
    world.RegisterSystem<KatamariPickupSystem>(SystemPhase::UPDATE);
    world.RegisterSystem<KatamariCameraSystem>(SystemPhase::UPDATE);
    world.RegisterSystem<KatamariGrowthSystem>(SystemPhase::UPDATE);
    world.RegisterSystem<KatamariGameStateSystem>(SystemPhase::UPDATE);

    world.RegisterSystem<RenderSystem>(SystemPhase::RENDER, &game);

    // Create GPU resources
    ResourceLoader::UploadAll(world, game.GetDevice());

    std::cout << "[Katamari] Game started! WASD to roll the ball. Pick up objects to grow!\n";

    game.Run();

    g_Game = nullptr;
    return 0;
}