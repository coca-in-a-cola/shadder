// main.cpp — Katamari Damacy на плоскости (SH-S2).
//
// ECS-фреймворк shadder, решения как в Godot: данные в компонентах,
// поведение в системах (KatamariSystems.h/.cpp). Модели — Wavefront OBJ из
// data/models (Kenney Furniture Kit, CC0 — см. data/models/LICENSE.txt),
// загружаются ObjLoader'ом фреймворка (framework/modules/render/ObjLoader).
//
// Геймплей (спека SH-S2):
//   * катящийся шар (WASD, скорость относительно камеры), Y фиксирован (2.5D);
//   * шар подбирает объекты МЕНЬШЕ себя (пересечение bounding-сфер из
//     геометрии) и растёт; объекты больше шара не подбираются;
//   * подобранные объекты прилипают к поверхности и вращаются вместе с шаром;
//   * камера следует за шаром сзади (не зависит от вращения шара).
//
// Управление:
//   WASD  — катить шар (W = вперёд/от камеры)
//   колесо мыши — зум камеры
//   ESC   — выход
//
// Если модели не загрузились (нет data/models рядом с exe) — пример падает в
// FALLBACK на процедурные сферы/кубы (см. MakePickupMesh) и печатает это в stdout.

#include "shadder.hpp"

#include <windows.h>
#include <DirectXMath.h>
#include <cmath>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "KatamariComponents.h"
#include "KatamariSystems.h"
#include "KatamariUpload.h"

#include "shared/prefabs/CameraPrefabs.h"

using namespace shadder;
using namespace DirectX;

static const int    kScreenW = 1280;
static const int    kScreenH = 720;
static const float  kFieldHalf = 40.0f;   // половина стороны игрового поля
static const float  kBallStartRadius = 1.0f;
static const int    kPickupCount = 120;   // объектов на поле
static const uint32_t kSeed = 20260910;   // фиксированный seed (воспроизводимость, спека п.3)

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

// Описание модели-пикапа: имя файла + базовый масштаб (модели Kenney ~1м).
struct PickupModelSpec {
    const wchar_t* file;
    const wchar_t* name;
    float scale;          // визуальный масштаб модели
    XMFLOAT4 color;       // цвет (OBJ не несёт цветов — красим сами)
};

// 8 моделей Kenney Furniture Kit (CC0) — см. data/models/LICENSE.txt.
static const PickupModelSpec kModels[] = {
    { L"data/models/chair.obj",              L"chair",    1.0f, { 0.85f, 0.55f, 0.30f, 1.0f } },
    { L"data/models/table.obj",              L"table",    1.0f, { 0.60f, 0.42f, 0.26f, 1.0f } },
    { L"data/models/toaster.obj",            L"toaster",  1.4f, { 0.85f, 0.85f, 0.88f, 1.0f } },
    { L"data/models/televisionModern.obj",   L"tv",       1.2f, { 0.25f, 0.27f, 0.32f, 1.0f } },
    { L"data/models/cardboardBoxClosed.obj", L"box",      1.3f, { 0.80f, 0.64f, 0.42f, 1.0f } },
    { L"data/models/lampSquareFloor.obj",    L"lamp",     1.0f, { 0.95f, 0.90f, 0.55f, 1.0f } },
    { L"data/models/pottedPlant.obj",        L"plant",    1.2f, { 0.35f, 0.70f, 0.35f, 1.0f } },
    { L"data/models/loungeSofa.obj",         L"sofa",     1.0f, { 0.45f, 0.55f, 0.75f, 1.0f } },
};

namespace {

// Ресурс геометрии: загруженный OBJ (или процедурный fallback), готовый к заливке.
struct GeometryResource {
    ObjMeshData mesh;
    bool fromObj = false; // false = процедурная сфера-фоллбек
};

// Загружает все модели; при неудаче хотя бы одной — включаем fallback-режим.
bool LoadAllModels(std::vector<GeometryResource>& out) {
    out.clear();
    out.reserve(std::size(kModels));
    bool allOk = true;
    for (const auto& spec : kModels) {
        GeometryResource res;
        res.fromObj = ObjLoader::Load(spec.file, spec.color, res.mesh);
        if (!res.fromObj) allOk = false;
        out.push_back(std::move(res));
    }
    return allOk;
}

// Процедурный фоллбек: «модель» = куб со скруглением не нужен, просто куб
// разных размеров (спека п.2: сферы/коробки случайных размеров как fallback).
// Заполняет ObjMeshData вручную, чтобы дальше один код-путь.
GeometryResource MakeFallbackCube(float size, XMFLOAT4 color) {
    GeometryResource res;
    const float h = size * 0.5f;
    const XMFLOAT3 normals[6] = {
        { 0, 0, -1 }, { 0, 0, 1 }, { -1, 0, 0 }, { 1, 0, 0 }, { 0, -1, 0 }, { 0, 1, 0 },
    };
    const XMFLOAT3 corners[8] = {
        { -h,-h,-h }, { h,-h,-h }, { h, h,-h }, { -h, h,-h },
        { -h,-h, h }, { h,-h, h }, { h, h, h }, { -h, h, h },
    };
    const int faces[6][4] = {
        { 0, 1, 2, 3 }, { 5, 4, 7, 6 }, { 4, 0, 3, 7 },
        { 1, 5, 6, 2 }, { 4, 5, 1, 0 }, { 3, 2, 6, 7 },
    };
    uint32_t vi = 0;
    for (int f = 0; f < 6; ++f) {
        for (int c = 0; c < 4; ++c) {
            const XMFLOAT3& p = corners[faces[f][c]];
            res.mesh.vertices.push_back({ { p.x, p.y, p.z, 1.0f }, normals[f], color });
        }
        res.mesh.indices.push_back(vi + 0);
        res.mesh.indices.push_back(vi + 1);
        res.mesh.indices.push_back(vi + 2);
        res.mesh.indices.push_back(vi + 0);
        res.mesh.indices.push_back(vi + 2);
        res.mesh.indices.push_back(vi + 3);
        vi += 4;
    }
    res.mesh.radius = h * std::sqrt(3.0f);
    res.fromObj = false;
    return res;
}

} // namespace

int main() {
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    DisplayWin32 display(L"Katamari", hInstance, kScreenW, kScreenH, WndProc);

    Game game;
    g_Game = &game;
    if (!game.Initialize(&display)) {
        std::cout << "Failed to initialize the game engine!\n";
        return 1;
    }

    World& world = game.GetWorld();

    // Компоненты примера.
    world.RegisterComponent<KatamariBallComponent>();
    world.RegisterComponent<KatamariPickupComponent>();
    world.RegisterComponent<KatamariStatsComponent>();

    // --- Модели: OBJ (Kenney CC0) или процедурный fallback --------------------
    std::vector<GeometryResource> geometries;
    const bool modelsOk = LoadAllModels(geometries);
    if (!modelsOk) {
        std::cout << "[Katamari] OBJ models not found -> FALLBACK: procedural cubes/spheres\n";
        std::mt19937 fb(static_cast<uint32_t>(kSeed));
        std::uniform_real_distribution<float> fsz(0.3f, 1.2f);
        for (auto& spec : kModels) {
            geometries.push_back(MakeFallbackCube(fsz(fb), spec.color));
        }
    }

    // --- Направленный свет (Phong-освещение фреймворка) ------------------------
    {
        Entity e = world.CreateEntity();
        auto& light = world.AddComponent<DirectionalLightComponent>(e);
        light.color = { 1.0f, 0.97f, 0.92f };
        light.intensity = 1.0f;
        light.direction = { -0.4f, -0.8f, 0.45f }; // сверху-слева-спереди
        light.active = true;
    }

    // --- Пол: большая плоскость (MeshComponent::PLANE, нормали вверх) ----------
    {
        Entity e = world.CreateEntity();
        auto& mesh = world.AddComponent<MeshComponent>(e);
        mesh.primitive = MeshComponent::Primitive::PLANE;
        mesh.planeWidth = kFieldHalf * 2.0f;
        mesh.planeDepth = kFieldHalf * 2.0f;
        mesh.planeSegmentsX = 1;
        mesh.planeSegmentsZ = 1;
        mesh.quadColor = { 0.30f, 0.55f, 0.30f, 1.0f };

        auto& mat = world.AddComponent<PhongMaterialComponent>(e);
        mat.ambient = { 0.10f, 0.16f, 0.10f };
        mat.diffuse = { 0.30f, 0.55f, 0.30f };
        mat.specular = { 0.06f, 0.08f, 0.06f };
        mat.shininess = 8.0f;

        auto& tr = world.AddComponent<Transform3D>(e);
        tr.position = { 0.0f, 0.0f, 0.0f };
        tr.rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
        tr.scale = { 1.0f, 1.0f, 1.0f };

        // Границы поля печатаем в stdout (ограничение поля — по kFieldHalf).
        (void)tr;
    }

    // --- Шар игрока ------------------------------------------------------------
    Entity ballEntity{};
    {
        ballEntity = world.CreateEntity();

        auto& mesh = world.AddComponent<MeshComponent>(ballEntity);
        mesh.primitive = MeshComponent::Primitive::SPHERE;
        mesh.sphereRadius = kBallStartRadius;
        mesh.sphereSlices = 32;
        mesh.sphereStacks = 20;
        mesh.quadColor = { 0.95f, 0.35f, 0.30f, 1.0f }; // katamari-красный

        auto& mat = world.AddComponent<PhongMaterialComponent>(ballEntity);
        mat.ambient = { 0.25f, 0.08f, 0.07f };
        mat.diffuse = { 0.95f, 0.35f, 0.30f };
        mat.specular = { 0.9f, 0.9f, 0.9f };
        mat.shininess = 40.0f;

        auto& tr = world.AddComponent<Transform3D>(ballEntity);
        tr.position = { 0.0f, kBallStartRadius, 0.0f };
        tr.rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
        tr.scale = { 1.0f, 1.0f, 1.0f };

        auto& ball = world.AddComponent<KatamariBallComponent>(ballEntity);
        ball.radius = kBallStartRadius;
        ball.moveSpeed = 7.0f;
        ball.growPerPickup = 0.9f;
    }

    // --- Пикапы: модели по полю, seed фиксирован (спека п.3) --------------------
    // Размерные классы: мелкие (подбираются сразу), средние, крупные (позже).
    std::mt19937 rng(kSeed);
    std::uniform_real_distribution<float> distPos(-kFieldHalf + 2.0f, kFieldHalf - 2.0f);
    std::uniform_real_distribution<float> distScale(0.55f, 1.8f);
    std::uniform_int_distribution<int> distModel(0, static_cast<int>(std::size(kModels)) - 1);
    std::uniform_real_distribution<float> distYaw(0.0f, XM_2PI);

    int totalPickups = 0;
    for (int i = 0; i < kPickupCount; ++i) {
        const int modelIdx = distModel(rng);
        const PickupModelSpec& spec = kModels[modelIdx];
        const GeometryResource& geo = geometries[modelIdx];
        const float scale = spec.scale * distScale(rng);

        Entity e = world.CreateEntity();

        auto& mesh = world.AddComponent<MeshComponent>(e);
        mesh.primitive = MeshComponent::Primitive::NONE; // геометрию заливаем сами ниже

        auto& mat = world.AddComponent<PhongMaterialComponent>(e);
        mat.ambient = { spec.color.x * 0.22f, spec.color.y * 0.22f, spec.color.z * 0.22f };
        mat.diffuse = { spec.color.x, spec.color.y, spec.color.z };
        mat.specular = { 0.25f, 0.25f, 0.25f };
        mat.shininess = 16.0f;

        auto& tr = world.AddComponent<Transform3D>(e);
        tr.position = { distPos(rng), 0.0f, distPos(rng) };
        tr.rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
        tr.scale = { scale, scale, scale };

        // Bounding sphere ИЗ ГЕОМЕТРИИ (спека п.5): радиус модели * масштаб.
        // Модели Kenney стоят на Y=0 (низ в нуле) — приподнимаем на радиус.
        const float geoRadius = geo.mesh.radius * scale;
        tr.position.y = geoRadius * 0.6f; // низ у пола (модели ~вытянуты вверх)

        auto& pickup = world.AddComponent<KatamariPickupComponent>(e);
        pickup.radius = geoRadius;
        pickup.pickedUp = false;

        // Нестандартная геометрия: заливаем OBJ/fallback вручную через
        // MeshComponent-совместимый путь. ObjMeshData уже в пресете
        // POS_NORMAL_COLOR фреймворка, потому пишем буферы напрямую.
        // (ResourceLoader умеет только примитивы — см. UploadMesh.)
        KatamariCustomMesh& custom = world.AddComponent<KatamariCustomMesh>(e);
        custom.vertices = geo.mesh.vertices;
        custom.indices = geo.mesh.indices;

        ++totalPickups;
    }

    // --- Статистика игры --------------------------------------------------------
    {
        Entity e = world.CreateEntity();
        auto& stats = world.AddComponent<KatamariStatsComponent>(e);
        stats.ballEntity = ballEntity;
        stats.total = totalPickups;
        stats.collected = 0;
    }

    // --- Системы -----------------------------------------------------------------
    world.RegisterSystem<KatamariBallSystem>(SystemPhase::UPDATE, &game);
    world.RegisterSystem<KatamariPickupSystem>(SystemPhase::UPDATE);
    world.RegisterSystem<KatamariCameraSystem>(SystemPhase::UPDATE, &game);
    world.RegisterSystem<KatamariStatsSystem>(SystemPhase::UPDATE);
    world.RegisterSystem<CameraSystem>(SystemPhase::PRE_RENDER, &game);
    world.RegisterSystem<RenderSystem>(SystemPhase::RENDER, &game);

    // GPU-ресурсы: пол и шар — примитивы фреймворка; OBJ-модели зальёт
    // KatamariUploadSystem (одноразовый проход по KatamariCustomMesh).
    ResourceLoader::UploadAll(world, game.GetDevice());
    KatamariUploadSystem upload(&game, game.GetDevice());
    upload.OnUpdate(world, 0.0f);

    std::cout << "[Katamari] models: " << (modelsOk ? "Kenney Furniture Kit OBJ (CC0)" : "FALLBACK procedural")
              << " | pickups: " << totalPickups << " | seed: " << kSeed << '\n';
    std::cout << "[Katamari] WASD - roll the ball, mouse wheel - zoom, ESC - exit\n";
    std::cout << "[Katamari] Pick up objects smaller than you. Grow!\n";

    game.Run();

    g_Game = nullptr;
    return 0;
}
