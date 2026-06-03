// ДОП: СЧЁТ.

#include "shadder.hpp"

#include <windows.h>
#include <DirectXMath.h>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <iostream>

#include "PongComponents.h"
#include "PaddleSystem.h"
#include "BallSystem.h"

using namespace shadder;
using namespace DirectX;

static const int kScreenW = 800;
static const int kScreenH = 800;
static const float kPaddleW = 12.0f;
static const float kPaddleH = 120.0f;
static const float kBallSize = 10.0f;

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

// Создаёт сущность-quad: только ОПИСАНИЕ ресурсов. GPU-буферы/шейдеры создаст
// ResourceLoader::UploadAll() — фреймворк, перед стартом игрового цикла.
static Entity CreateQuadEntity(World& world, float width, float height,
                               const XMFLOAT4& color) {
    Entity e = world.CreateEntity();

    auto& mesh = world.AddComponent<MeshComponent>(e);
    mesh.primitive = MeshComponent::Primitive::QUAD;
    mesh.quadWidth = width;
    mesh.quadHeight = height;
    mesh.quadColor = color;

    // Материал без shaderPath => дефолтный 2D-шейдер фреймворка.
    world.AddComponent<MaterialComponent>(e);

    return e;
}

int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    HINSTANCE hInstance = GetModuleHandle(nullptr);
    DisplayWin32 display(L"Pong", hInstance, kScreenW, kScreenH, WndProc);

    Game game;
    g_Game = &game;
    if (!game.Initialize(&display)) {
        std::cout << "Failed to initialize the game engine!\n";
        return 1;
    }

    World& world = game.GetWorld();

    // Register custom components
    world.RegisterComponent<PongStateComponent>();
    world.RegisterComponent<PlayerPaddleTag>();
    world.RegisterComponent<AiPaddleTag>();
    world.RegisterComponent<BallTag>();

    // Camera
    {
        Entity cameraEntity = world.CreateEntity();
        auto& cam = world.AddComponent<CameraComponent>(cameraEntity);
        cam.projection = CameraComponent::Projection::ORTHO_SCREEN;
        cam.screenW = static_cast<float>(kScreenW);
        cam.screenH = static_cast<float>(kScreenH);
        cam.nearZ = -1.0f;
        cam.farZ = 1.0f;
        cam.active = true;
    }

    // Сущности задают только описание (размер/цвет); ресурсы создаст ResourceLoader.
    XMFLOAT4 white = { 1.0f, 1.0f, 1.0f, 1.0f };

    // Player paddle (left)
    {
        Entity e = CreateQuadEntity(world, kPaddleW, kPaddleH, white);
        auto& tr = world.AddComponent<Transform3D>(e);
        tr.position = { kPaddleW * 0.5f + 10.0f, kScreenH * 0.5f, 0.0f };
        tr.scale = { 1.0f, 1.0f, 1.0f };
        world.AddComponent<PlayerPaddleTag>(e);
    }

    // AI paddle (right)
    {
        Entity e = CreateQuadEntity(world, kPaddleW, kPaddleH, white);
        auto& tr = world.AddComponent<Transform3D>(e);
        tr.position = { kScreenW - kPaddleW * 0.5f - 10.0f, kScreenH * 0.5f, 0.0f };
        tr.scale = { 1.0f, 1.0f, 1.0f };
        world.AddComponent<AiPaddleTag>(e);
    }

    // Ball
    {
        Entity e = CreateQuadEntity(world, kBallSize, kBallSize, white);
        auto& tr = world.AddComponent<Transform3D>(e);
        tr.position = { kScreenW * 0.5f, kScreenH * 0.5f, 0.0f };
        tr.scale = { 1.0f, 1.0f, 1.0f };
        world.AddComponent<BallTag>(e);

        auto& vel = world.AddComponent<VelocityComponent>(e);
        float angle = (static_cast<float>(std::rand()) / RAND_MAX) * XM_PI * 0.5f - XM_PI * 0.25f;
        if (std::rand() % 2 == 0) angle += XM_PI;
        vel.velocity = { std::cos(angle), std::sin(angle), 0.0f };
    }

    // Game state
    {
        Entity e = world.CreateEntity();
        auto& ps = world.AddComponent<PongStateComponent>(e);
        ps.continueTime = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    }

    // Register systems
    world.RegisterSystem<CameraSystem>(SystemPhase::PRE_RENDER, &game);

    world.RegisterSystem<PaddleSystem>(SystemPhase::UPDATE, &game,
                                       static_cast<float>(kScreenH));
    world.RegisterSystem<BallSystem>(SystemPhase::UPDATE,
                                     static_cast<float>(kScreenW),
                                     static_cast<float>(kScreenH),
                                     kPaddleW, kPaddleH, kBallSize);

    world.RegisterSystem<RenderSystem>(SystemPhase::RENDER, &game);

    // Создаём GPU-ресурсы из описаний (Mesh/Material) — один раз перед циклом.
    ResourceLoader::UploadAll(world, game.GetDevice());

    std::cout << "[Pong] Game started! W/S to move left paddle.\n";

    game.Run();

    g_Game = nullptr;
    return 0;
}
