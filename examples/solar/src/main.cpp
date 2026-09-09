// main.cpp — демо солнечной системы (SH-S1).
//
// ECS-фреймворк shadder, решения как в Godot: данные в компонентах,
// поведение в системах; геометрия и материалы — через описание (MeshComponent +
// PhongMaterialComponent), GPU-ресурсы заливает ResourceLoader.
//
// Сцена: солнце в центре, 4 планеты на орбитах (разные радиусы/скорости),
// у двух планет — луны (орбита вокруг планеты). Все тела вращаются вокруг
// своей оси (SolarSpin), планеты и луны ходят по орбитам (SolarOrbit).
// Итого 7 тел с орбитами/спинами — пункты 1-3 и 6 спецификации SH-S1.
//
// Управление (всё дублируется в stdout при старте):
//   TAB         — переключение камеры FPS <-> ORBIT
//   FPS-режим:  WASD — ходьба, мышь — взгляд, Q/E — вниз/вверх, Shift — ускорение
//   ORBIT-режим: RMB+мышь — взгляд вокруг цели, колесо — зум, MMB+мышь — панорама
//   1 / 2 / 3   — проекция: FOV 45° / FOV 90° / ортогональная
//   P           — следующий пресет проекции (циклически)
//   ESC         — выход
// Активный пресет проекции и смена камеры печатаются в stdout.

#include "shadder.hpp"

#include <windows.h>
#include <DirectXMath.h>
#include <cmath>
#include <iostream>

#include "SolarComponents.h"
#include "SolarMotionSystem.h"
#include "SolarCameraSystem.h"

#include "shared/prefabs/CameraPrefabs.h"

using namespace shadder;
using namespace DirectX;

static const int kScreenW = 1280;
static const int kScreenH = 800;

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

// Создаёт небесное тело: сфера + Phong-материал + трансформ + спин.
static Entity MakeBody(World& world, float radius, int slices, int stacks,
                       XMFLOAT4 color, XMFLOAT3 ambient, XMFLOAT3 diffuse,
                       XMFLOAT3 specular, float shininess,
                       XMFLOAT3 spinAxis, float spinSpeed, const XMFLOAT3& position) {
    Entity e = world.CreateEntity();

    auto& mesh = world.AddComponent<MeshComponent>(e);
    mesh.primitive = MeshComponent::Primitive::SPHERE;
    mesh.sphereRadius = radius;
    mesh.sphereSlices = slices;
    mesh.sphereStacks = stacks;
    mesh.quadColor = color;

    auto& mat = world.AddComponent<PhongMaterialComponent>(e);
    mat.ambient = ambient;
    mat.diffuse = diffuse;
    mat.specular = specular;
    mat.shininess = shininess;

    auto& tr = world.AddComponent<Transform3D>(e);
    tr.rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
    tr.scale = { 1.0f, 1.0f, 1.0f };
    tr.position = position;

    auto& spin = world.AddComponent<SolarSpin>(e);
    spin.axis = spinAxis;
    spin.speed = spinSpeed;

    return e;
}

int main() {
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    DisplayWin32 display(L"Solar System", hInstance, kScreenW, kScreenH, WndProc);

    Game game;
    g_Game = &game;
    if (!game.Initialize(&display)) {
        std::cout << "Failed to initialize the game engine!\n";
        return 1;
    }

    World& world = game.GetWorld();

    // Пользовательские компоненты примера.
    world.RegisterComponent<SolarOrbit>();
    world.RegisterComponent<SolarSpin>();
    world.RegisterComponent<SolarSettings>();

    // --- Камера: перспектива 45°, стартовый режим FPS -------------------------
    PerspectiveCameraPrefab(XM_PIDIV4, static_cast<float>(kScreenW) / kScreenH,
                            0.1f, 500.0f)
        .Instantiate(world);

    // --- Настройки сцены ------------------------------------------------------
    Entity settingsEntity = world.CreateEntity();
    auto& settings = world.AddComponent<SolarSettings>(settingsEntity);
    settings.projection = SolarSettings::ProjectionPreset::FOV45;
    settings.cameraFPS = true;

    // --- Направленный свет: «солнечный» тёплый свет сверху-сбоку --------------
    {
        Entity e = world.CreateEntity();
        auto& light = world.AddComponent<DirectionalLightComponent>(e);
        light.color = { 1.0f, 0.96f, 0.9f };
        light.intensity = 1.0f;
        light.direction = { 0.35f, -0.6f, 0.35f }; // нормализуется в RenderSystem
        light.active = true;
    }

    // --- Солнце в центре (светится сам: ambient ~ diffuse, specular = 0) ------
    const XMFLOAT3 origin = { 0.0f, 0.0f, 0.0f };
    Entity sun = MakeBody(world, 5.0f, 48, 32,
                          { 1.0f, 0.85f, 0.3f, 1.0f },
                          { 0.9f, 0.7f, 0.25f }, { 1.0f, 0.85f, 0.3f },
                          { 0.0f, 0.0f, 0.0f }, 1.0f,
                          { 0.0f, 1.0f, 0.0f }, 0.05f, origin);

    // --- Планеты: осмысленные радиусы орбит и скорости (п.6) ------------------
    struct PlanetSpec {
        float orbitRadius;   // радиус орбиты вокруг солнца
        float orbitSpeed;    // рад/сек
        float height;        // наклон плоскости орбиты (смещение по Y)
        float radius;        // размер планеты
        int   slices, stacks;
        XMFLOAT4 color;
        XMFLOAT3 ambient, diffuse, specular;
        float shininess;
        float spinSpeed;
        XMFLOAT3 spinAxis;
    };
    const PlanetSpec planetSpecs[4] = {
        // «Меркурий»: ближняя, быстрая, маленькая, серо-жёлтая.
        {  16.0f, 0.62f,  0.0f, 1.1f, 32, 20,
           { 0.75f, 0.62f, 0.45f, 1.0f },
           { 0.12f, 0.09f, 0.06f }, { 0.75f, 0.62f, 0.45f }, { 0.15f, 0.12f, 0.08f }, 14.0f,
           0.9f, { 0.0f, 1.0f, 0.15f } },
        // «Венера»: оранжевая, плотная.
        {  26.0f, 0.44f,  0.0f, 1.7f, 36, 24,
           { 0.95f, 0.55f, 0.2f, 1.0f },
           { 0.15f, 0.08f, 0.02f }, { 0.95f, 0.55f, 0.2f },  { 0.4f, 0.3f, 0.15f },  22.0f,
           1.1f, { 0.05f, 1.0f, 0.0f } },
        // «Земля»: голубая, орбита с наклоном.
        {  38.0f, 0.30f,  1.0f, 1.9f, 40, 26,
           { 0.25f, 0.5f, 0.95f, 1.0f },
           { 0.03f, 0.07f, 0.16f }, { 0.25f, 0.5f, 0.95f },  { 0.5f, 0.6f, 0.8f },   30.0f,
           0.8f, { 0.0f, 1.0f, 0.0f } },
        // «Марс»: красная, орбита ниже, самая медленная.
        {  52.0f, 0.20f, -1.5f, 2.6f, 40, 26,
           { 0.85f, 0.4f, 0.25f, 1.0f },
           { 0.14f, 0.05f, 0.03f }, { 0.85f, 0.4f, 0.25f },  { 0.3f, 0.2f, 0.12f },  18.0f,
           1.0f, { 0.0f, 1.0f, 0.05f } },
    };

    Entity planetEntities[4] = {};
    for (int i = 0; i < 4; ++i) {
        const PlanetSpec& p = planetSpecs[i];
        // Стартовая фаза 0: планета на +X от солнца, ходит против часовой (LH).
        const XMFLOAT3 start = { p.orbitRadius, p.height, 0.0f };
        planetEntities[i] = MakeBody(world, p.radius, p.slices, p.stacks,
                                     p.color, p.ambient, p.diffuse, p.specular, p.shininess,
                                     p.spinAxis, p.spinSpeed, start);
        auto& orbit = world.AddComponent<SolarOrbit>(planetEntities[i]);
        orbit.parentEntity = sun;
        orbit.radius = p.orbitRadius;
        orbit.speed = p.orbitSpeed;
        orbit.height = p.height;
        orbit.angle = 0.0f;
    }

    // --- Луны: орбита вокруг своей планеты (п.2-3) ----------------------------
    // Луна Земли (планеты[2]).
    {
        Entity earth = planetEntities[2];
        Entity moon = MakeBody(world, 0.5f, 24, 16,
                               { 0.8f, 0.8f, 0.82f, 1.0f },
                               { 0.08f, 0.08f, 0.09f }, { 0.8f, 0.8f, 0.82f },
                               { 0.1f, 0.1f, 0.1f }, 8.0f,
                               { 0.0f, 1.0f, 0.0f }, 0.3f,
                               { 3.2f, 0.0f, 0.0f });
        auto& orbit = world.AddComponent<SolarOrbit>(moon);
        orbit.parentEntity = earth;
        orbit.radius = 3.2f;
        orbit.speed = 2.4f; // заметно быстрее орбиты Земли
        orbit.height = 0.0f;
        orbit.angle = 1.0f; // стартовая фаза — чтобы не слипалась с планетой визуально
    }
    // «Фобос» — луна Марса (планеты[3]).
    {
        Entity mars = planetEntities[3];
        Entity moon = MakeBody(world, 0.4f, 20, 14,
                               { 0.6f, 0.55f, 0.5f, 1.0f },
                               { 0.07f, 0.06f, 0.05f }, { 0.6f, 0.55f, 0.5f },
                               { 0.08f, 0.08f, 0.08f }, 6.0f,
                               { 0.0f, 1.0f, 0.0f }, 0.5f,
                               { 4.0f, 0.0f, 0.0f });
        auto& orbit = world.AddComponent<SolarOrbit>(moon);
        orbit.parentEntity = mars;
        orbit.radius = 4.0f;
        orbit.speed = 3.1f;
        orbit.height = 0.5f; // слегка наклонная орбита
        orbit.angle = 2.0f;
    }

    // --- Системы ---------------------------------------------------------------
    world.RegisterSystem<SolarMotionSystem>(SystemPhase::UPDATE);
    world.RegisterSystem<SolarCameraSystem>(SystemPhase::UPDATE, &game);
    world.RegisterSystem<CameraSystem>(SystemPhase::PRE_RENDER, &game);
    world.RegisterSystem<RenderSystem>(SystemPhase::RENDER, &game);

    // GPU-ресурсы (меши + шейдеры) — один раз перед игровым циклом.
    ResourceLoader::UploadAll(world, game.GetDevice());

    std::cout << "[Solar] Bodies: sun + 4 planets + 2 moons = 7\n";
    std::cout << "[Solar] TAB - switch camera (FPS <-> ORBIT)\n";
    std::cout << "[Solar] FPS:  WASD move, mouse look, Q/E down/up, Shift boost\n";
    std::cout << "[Solar] ORBIT: RMB+mouse look, wheel zoom, MMB+mouse pan\n";
    std::cout << "[Solar] Projection: 1 = FOV45, 2 = FOV90, 3 = ORTHO, P = cycle. Active: PERSPECTIVE FOV 45deg\n";
    std::cout << "[Solar] ESC - exit\n";

    game.Run();

    g_Game = nullptr;
    return 0;
}
