// main.cpp — демо Phong-освещения (SH-S3).
//
// Сцена: пол (плоскость) + 5 объектов с разными материалами Фонга
// (разные diffuse-цвета, specular и shininess — чтобы блики отличались),
// направленный свет, орбитальная камера.
//
// Управление:
//   ЛКМ + мышь  — вращение камеры вокруг сцены
//   Колесо      — зум
//   W / S       — поднять/опустить точку, куда смотрим
//   ESC         — выход (фреймворк)
//
// Что должно быть видно на Windows: освещённая сцена с плавным затенением
// и зеркальными бликами на объектах. Блик зависит от угла камеры
// (позиция зрителя приходит в пиксельный шейдер) — при вращении камеры
// блики «ползут» по объектам. Материалы слева направо:
//   1) текстурированный diffuse-куб (1.jpg, без блика)
//   2) глянцевый текстурированный куб (2.jpg, shininess 64)
//   3) блестящая белая сфера      (shininess 96 — малый яркий блик)
//   4) синяя сфера с блик-полосой (shininess 32)
//   5) текстурированный маленький куб (3.jpg, тёплый specular)
// Пол — серый матовый с мягким бликом, чтобы читалось направление света.

#include "shadder.hpp"

#include <windows.h>
#include <DirectXMath.h>
#include <iostream>

#include "OrbitCameraSystem.h"

#include "shared/prefabs/CameraPrefabs.h"

using namespace shadder;
using namespace DirectX;

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

// Создаёт объект: MeshComponent (примитив) + PhongMaterialComponent + Transform3D.
static Entity MakeObject(World& world,
                         MeshComponent::Primitive primitive,
                         XMFLOAT4 color,
                         XMFLOAT3 ambient, XMFLOAT3 diffuse,
                         XMFLOAT3 specular, float shininess,
                         const wchar_t* diffuseTexture = L"") {
    Entity e = world.CreateEntity();

    auto& mesh = world.AddComponent<MeshComponent>(e);
    mesh.primitive = primitive;
    mesh.quadColor = color;

    auto& mat = world.AddComponent<PhongMaterialComponent>(e);
    mat.ambient = ambient;
    mat.diffuse = diffuse;
    mat.specular = specular;
    mat.shininess = shininess;
    mat.diffuseTexturePath = diffuseTexture;

    auto& tr = world.AddComponent<Transform3D>(e);
    tr.rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
    tr.scale = { 1.0f, 1.0f, 1.0f };
    return e;
}

int main() {
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    Game game;
    g_Game = &game;
    if (!game.SetDisplay(std::make_unique<DisplayWin32>(L"Phong Lighting Demo", hInstance, WndProc))
                 .SetScreenSize({1280, 800})
                 .Initialize()) {
        std::cout << "Failed to initialize the game engine!\n";
        return 1;
    }

    World& world = game.GetWorld();
    const ScreenSize screenSize = game.GetScreenSize();

    // --- Камера: перспектива, орбитальное управление ------------------------
    PerspectiveCameraPrefab(XM_PIDIV4, static_cast<float>(screenSize.width) / screenSize.height)
        .Instantiate(world);

    // --- Направленный свет: тёплый, светит вниз-вбок ------------------------
    {
        Entity e = world.CreateEntity();
        auto& light = world.AddComponent<DirectionalLightComponent>(e);
        light.color = { 1.0f, 0.96f, 0.9f };
        light.intensity = 1.0f;
        light.direction = { 0.4f, -0.8f, 0.45f }; // нормализуется в RenderSystem
        light.shadowsEnabled = true;
        light.shadowOrthoSize = 24.0f;
        light.shadowDistance = 25.0f;
        light.shadowFarZ = 60.0f;
        light.active = true;
    }

    // --- Пол: серая матовая плоскость ---------------------------------------
    {
        Entity e = MakeObject(world, MeshComponent::Primitive::PLANE,
                              { 0.55f, 0.55f, 0.58f, 1.0f },
                              { 0.15f, 0.15f, 0.15f }, { 0.55f, 0.55f, 0.55f },
                              { 0.25f, 0.25f, 0.25f }, 24.0f);
        auto* mesh = world.GetComponent<MeshComponent>(e);
        auto* tr = world.GetComponent<Transform3D>(e);
        mesh->planeWidth = 20.0f;
        mesh->planeDepth = 20.0f;
    }

    // --- 5 объектов с разными материалами ------------------------------------
    {
        // 1) Матовый текстурированный куб — diffuse shader.
        Entity e = MakeObject(world, MeshComponent::Primitive::CUBE,
                              { 1.0f, 1.0f, 1.0f, 1.0f },
                              { 0.08f, 0.08f, 0.08f }, { 0.9f, 0.9f, 0.9f },
                              { 0.3f, 0.3f, 0.3f }, 8.0f, L"data/textures/1.jpg");
        world.GetComponent<PhongMaterialComponent>(e)->shading = PhongMaterialDesc::Shading::Diffuse;
        auto* mesh = world.GetComponent<MeshComponent>(e);
        auto* tr = world.GetComponent<Transform3D>(e);
        mesh->cubeSize = 1.2f;
        tr->position = { -4.0f, 0.6f, 0.0f };
    }
    {
        // 2) Глянцевый текстурированный куб — выраженный блик.
        Entity e = MakeObject(world, MeshComponent::Primitive::CUBE,
                              { 1.0f, 1.0f, 1.0f, 1.0f },
                              { 0.08f, 0.08f, 0.08f }, { 0.85f, 0.85f, 0.85f },
                              { 0.9f, 0.9f, 0.9f }, 64.0f, L"data/textures/2.jpg");
        auto* mesh = world.GetComponent<MeshComponent>(e);
        auto* tr = world.GetComponent<Transform3D>(e);
        mesh->cubeSize = 1.0f;
        tr->position = { -1.8f, 0.5f, 1.4f };
    }
    {
        // 3) Белая сфера с узким ярким бликом (shininess 96).
        Entity e = MakeObject(world, MeshComponent::Primitive::SPHERE,
                              { 0.95f, 0.95f, 0.95f, 1.0f },
                              { 0.1f, 0.1f, 0.1f }, { 0.95f, 0.95f, 0.95f },
                              { 1.0f, 1.0f, 1.0f }, 96.0f);
        auto* mesh = world.GetComponent<MeshComponent>(e);
        auto* tr = world.GetComponent<Transform3D>(e);
        mesh->sphereRadius = 0.7f;
        mesh->sphereSlices = 32;
        mesh->sphereStacks = 24;
        tr->position = { 0.6f, 0.7f, -0.6f };
    }
    {
        // 4) Синяя сфера, средний блик (shininess 32).
        Entity e = MakeObject(world, MeshComponent::Primitive::SPHERE,
                              { 0.2f, 0.4f, 0.95f, 1.0f },
                              { 0.02f, 0.04f, 0.1f }, { 0.2f, 0.4f, 0.95f },
                              { 0.8f, 0.8f, 0.8f }, 32.0f);
        auto* mesh = world.GetComponent<MeshComponent>(e);
        auto* tr = world.GetComponent<Transform3D>(e);
        mesh->sphereRadius = 0.5f;
        mesh->sphereSlices = 24;
        mesh->sphereStacks = 18;
        tr->position = { 2.4f, 0.5f, 0.8f };
    }
    {
        // 5) Текстурированный маленький куб — тёплый specular.
        Entity e = MakeObject(world, MeshComponent::Primitive::CUBE,
                              { 1.0f, 1.0f, 1.0f, 1.0f },
                              { 0.1f, 0.1f, 0.1f }, { 0.85f, 0.85f, 0.85f },
                              { 1.0f, 0.85f, 0.5f }, 24.0f, L"data/textures/3.jpg");
        auto* mesh = world.GetComponent<MeshComponent>(e);
        auto* tr = world.GetComponent<Transform3D>(e);
        mesh->cubeSize = 0.8f;
        tr->position = { 4.0f, 0.4f, -0.2f };
    }

    // --- Системы --------------------------------------------------------------
    world.RegisterSystem<CameraSystem>(SystemPhase::PRE_RENDER, &game);
    world.RegisterSystem<OrbitCameraSystem>(SystemPhase::PRE_RENDER, &game,
                                            XMFLOAT3{ 0.0f, 0.5f, 0.0f }, 9.0f);
    world.RegisterSystem<RenderSystem>(SystemPhase::RENDER, &game);

    // GPU-ресурсы (меши + шейдеры) — один раз перед игровым циклом.
    ResourceLoader::UploadAll(world, game.GetDevice());

    std::cout << "[Phong] LMB+mouse — orbit, wheel — zoom, W/S — height, ESC — exit.\n";

    game.Run();

    g_Game = nullptr;
    return 0;
}
