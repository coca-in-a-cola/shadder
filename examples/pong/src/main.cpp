// ДОП: СЧЁТ.

#include "shadder.hpp"

#include <windows.h>
#include <DirectXMath.h>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <iostream>
#include <string>

#include "PongComponents.h"
#include "PaddleSystem.h"
#include "BallSystem.h"

#include "shared/prefabs/CameraPrefabs.h"
#include "shared/prefabs/MeshPrefabs.h"

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

    // Register custom components not managed by prefabs
    world.RegisterComponent<PongStateComponent>();
    world.RegisterComponent<PlayerPaddleTag>();
    world.RegisterComponent<AiPaddleTag>();
    world.RegisterComponent<BallTag>();

    // Camera — 2D orthographic in screen pixels
    OrthoCameraPrefab(static_cast<float>(kScreenW),
                      static_cast<float>(kScreenH)).Instantiate(world);

    XMFLOAT4 white = { 1.0f, 1.0f, 1.0f, 1.0f };

    // Player paddle (left)
    {
        Entity e = QuadPrefab(kPaddleW, kPaddleH, white).Instantiate(world);
        auto& tr = world.AddComponent<Transform3D>(e);
        tr.position = { kPaddleW * 0.5f + 10.0f, kScreenH * 0.5f, 0.0f };
        tr.scale = { 1.0f, 1.0f, 1.0f };
        world.AddComponent<PlayerPaddleTag>(e);
    }

    // AI paddle (right)
    {
        Entity e = QuadPrefab(kPaddleW, kPaddleH, white).Instantiate(world);
        auto& tr = world.AddComponent<Transform3D>(e);
        tr.position = { kScreenW - kPaddleW * 0.5f - 10.0f, kScreenH * 0.5f, 0.0f };
        tr.scale = { 1.0f, 1.0f, 1.0f };
        world.AddComponent<AiPaddleTag>(e);
    }

    // Ball
    {
        Entity e = QuadPrefab(kBallSize, kBallSize, white).Instantiate(world);
        auto& tr = world.AddComponent<Transform3D>(e);
        tr.position = { kScreenW * 0.5f, kScreenH * 0.5f, 0.0f };
        tr.scale = { 1.0f, 1.0f, 1.0f };
        world.AddComponent<BallTag>(e);

        auto& vel = world.AddComponent<VelocityComponent>(e);
        float angle = (static_cast<float>(std::rand()) / RAND_MAX) * XM_PI * 0.5f - XM_PI * 0.25f;
        if (std::rand() % 2 == 0) angle += XM_PI;
        vel.velocity = { std::cos(angle), std::sin(angle), 0.0f };
    }

    // Game state entity
    Entity stateEntity;
    {
        Entity e = world.CreateEntity();
        auto& ps = world.AddComponent<PongStateComponent>(e);
        ps.continueTime = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        stateEntity = e;
    }

    // Score text entities (left and right)
    Entity score1Entity, score2Entity;
    {
        // Left score (score1) - centered at ~100px left of center
        Entity e = world.CreateEntity();
        auto& tr = world.AddComponent<Transform3D>(e);
        tr.position = { kScreenW * 0.5f - 100.0f, kScreenH * 0.5f - 200.0f, 0.0f };
        tr.scale = { 1.0f, 1.0f, 1.0f };
        auto& text = world.AddComponent<TextComponent>(e);
        text.text = "0";
        text.color = { 1.0f, 1.0f, 1.0f, 1.0f };
        text.fontSize = 48.0f;
        text.hAlign = 0.5f; // center
        text.vAlign = 0.5f; // center
        score1Entity = e;
    }
    {
        // Right score (score2) - centered at ~100px right of center
        Entity e = world.CreateEntity();
        auto& tr = world.AddComponent<Transform3D>(e);
        tr.position = { kScreenW * 0.5f + 100.0f, kScreenH * 0.5f - 200.0f, 0.0f };
        tr.scale = { 1.0f, 1.0f, 1.0f };
        auto& text = world.AddComponent<TextComponent>(e);
        text.text = "0";
        text.color = { 1.0f, 1.0f, 1.0f, 1.0f };
        text.fontSize = 48.0f;
        text.hAlign = 0.5f; // center
        text.vAlign = 0.5f; // center
        score2Entity = e;
    }

    // Message text entity (GO!, WINNER LEFT/RIGHT)
    Entity messageEntity;
    {
        Entity e = world.CreateEntity();
        auto& tr = world.AddComponent<Transform3D>(e);
        tr.position = { kScreenW * 0.5f, kScreenH * 0.5f, 0.0f };
        tr.scale = { 1.0f, 1.0f, 1.0f };
        auto& text = world.AddComponent<TextComponent>(e);
        text.text = "GO!";
        text.color = { 1.0f, 1.0f, 0.0f, 1.0f }; // yellow
        text.fontSize = 64.0f;
        text.hAlign = 0.5f;
        text.vAlign = 0.5f;
        messageEntity = e;
    }

    // Register systems (manual — will be addressed later)
    world.RegisterSystem<CameraSystem>(SystemPhase::PRE_RENDER, &game);

    world.RegisterSystem<PaddleSystem>(SystemPhase::UPDATE, &game,
                                       static_cast<float>(kScreenH));
    world.RegisterSystem<BallSystem>(SystemPhase::UPDATE,
                                     static_cast<float>(kScreenW),
                                     static_cast<float>(kScreenH),
                                     kPaddleW, kPaddleH, kBallSize);

    world.RegisterSystem<RenderSystem>(SystemPhase::RENDER, &game);
    world.RegisterSystem<TextRenderSystem>(SystemPhase::RENDER, &game);

    // Создаём GPU-ресурсы из описаний (Mesh/Material) — один раз перед циклом.
    ResourceLoader::UploadAll(world, game.GetDevice());

    std::cout << "[Pong] Game started! W/S to move left paddle.\n";

    // Lambda to update score text from state
    auto updateScoreText = [&](PongStateComponent& state) {
        // Update score texts
        if (auto* t1 = world.GetComponent<TextComponent>(score1Entity)) {
            t1->text = std::to_string(state.score1);
            t1->dirty = true;
        }
        if (auto* t2 = world.GetComponent<TextComponent>(score2Entity)) {
            t2->text = std::to_string(state.score2);
            t2->dirty = true;
        }

        // Update message text based on state
        if (auto* msg = world.GetComponent<TextComponent>(messageEntity)) {
            if (state.state == PongStateComponent::COOLDOWN) {
                msg->text = "GO!";
                msg->color = { 1.0f, 1.0f, 0.0f, 1.0f };
            } else if (state.state == PongStateComponent::GAMEOVER) {
                if (state.score1 > state.score2) {
                    msg->text = "WINNER LEFT";
                } else {
                    msg->text = "WINNER RIGHT";
                }
                msg->color = { 0.0f, 1.0f, 0.0f, 1.0f }; // green
            } else {
                msg->text = "";
            }
            msg->dirty = true;
        }
    };

    // Initial score update
    {
        Query<PongStateComponent> stateQ(world);
        stateQ.ForEach([&](Entity, PongStateComponent& s) {
            updateScoreText(s);
        });
    }

    game.Run();

    g_Game = nullptr;
    return 0;
}