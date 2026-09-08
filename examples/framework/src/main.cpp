// =============================================================================
//  examples/framework — Демо текущего C++ фреймворка (SH-P: префабы и сцены).
//
//  «Дождь» из треугольников + маркер-квад: структура мира собирается как
//  ОДИН PackedScene (Godot-style) с вложенными префабами и инстанцируется
//  в ECS World. Префабы migrated на PackedScene-механизм:
//
//   * OrthoCameraScene(w,h) — вложенный PackedScene-префаб (камера),
//     Inline() = shared reference (Godot instanced-scene semantics);
//   * QuadScene(w,h,color)  — вложенный PackedScene-префаб (маркер-квад),
//     позиция маркера задаётся per-instance transform override (Godot:
//     placement of an instanced scene);
//   * структура дерева логируется в консоль (entity id, иерархия,
//     глобальные трансформы) + assert-чеки — проверяемый результат;
//   * InstancedMeshComponent / InstanceTag / InstancedRenderSystem: один
//     шейдер, один draw call, один динамический instance-буфер (15k треугольников).
//
//  Импорт фреймворка — строго через include/shadder.hpp.
// =============================================================================

#include "RainScene.h"

#include <iostream>

// -----------------------------------------------------------------------------
int main() {
    using namespace rain_demo;

    HINSTANCE hInstance = GetModuleHandle(nullptr);
    DisplayWin32 display(L"Framework Demo - Scene Rain", hInstance, kScreenW, kScreenH, RainWndProc);

    Game game;
    g_Game = &game;
    if (!game.Initialize(&display)) {
        std::cout << "Failed to initialize the game engine!" << '\n';
        return 1;
    }

    World& world = game.GetWorld();

    // =========================================================================
    // 1) Вложенные префабы (PackedScene-механизм) — ЖИВУТ в main: сцена хранит
    //    на них shared-reference, они должны переживать Instantiate().
    // =========================================================================
    PackedScene cameraPrefab = OrthoCameraScene(static_cast<float>(kScreenW),
                                                static_cast<float>(kScreenH));
    PackedScene quadPrefab = QuadScene(60.0f, 40.0f, {0.9f, 0.55f, 0.15f, 1.0f});

    // 2) Собрать всю демо-сцену как один PackedScene и инстанцировать в World.
    PackedScene scene = BuildRainScene(cameraPrefab, quadPrefab);

    SceneNode::Ptr sceneRoot;
    Entity rootEntity = scene.Instantiate(world, &sceneRoot);
    if (!sceneRoot || !rootEntity.IsValid()) {
        std::cout << "[Demo] FAILED: scene instantiation produced no root!" << '\n';
        return 1;
    }

    // 3) Runtime-часть: батч + треугольники (attach к узлу сцены).
    AttachRainInstances(game, world, sceneRoot);

    // 4) Проверяемые чеки структуры сцены (как в tests/test_scene.cpp).
    int failures = 0;
#define DEMO_CHECK(cond)                                                                  \
    do {                                                                                  \
        if (!(cond)) {                                                                    \
            std::cout << "[Demo] CHECK FAILED: " #cond << '\n';                           \
            ++failures;                                                                   \
        }                                                                                 \
    } while (0)

    SceneNode::Ptr camNode = sceneRoot->FindChild("Camera");
    SceneNode::Ptr markerNode = sceneRoot->FindChild("Marker");
    DEMO_CHECK(camNode != nullptr);
    DEMO_CHECK(markerNode != nullptr);
    DEMO_CHECK(camNode && world.HasComponent<CameraComponent>(camNode->GetEntity()));
    DEMO_CHECK(markerNode && world.HasComponent<MeshComponent>(markerNode->GetEntity()));
    DEMO_CHECK(markerNode &&
               PosNear(markerNode->GetGlobalTransform().position, 430.0f, 120.0f, 0.0f));
    // ECS синхронизирован с per-instance override вложенной сцены.
    const Transform3D* markerTr =
        markerNode ? world.GetComponent<Transform3D>(markerNode->GetEntity()) : nullptr;
    DEMO_CHECK(markerTr &&
               PosNear(markerTr->position, 430.0f, 120.0f, 0.0f));

    // =========================================================================
    // 5) Системы (порядок важен)
    // =========================================================================
    // PRE_RENDER: камера обновляет ViewProjection (slot b1) до рендера.
    world.RegisterSystem<CameraSystem>(SystemPhase::PRE_RENDER, &game);
    // UPDATE: сначала движение, затем телепорт упавших.
    world.RegisterSystem<MovementSystem>(SystemPhase::UPDATE);
    world.RegisterSystem<FallTeleportSystem>(SystemPhase::UPDATE,
                                             static_cast<float>(kScreenW),
                                             static_cast<float>(kScreenH));
    // RENDER: единый instanced-рендер всех треугольников + RenderSystem для маркера.
    world.RegisterSystem<InstancedRenderSystem>(SystemPhase::RENDER, &game);
    world.RegisterSystem<RenderSystem>(SystemPhase::RENDER, &game);

    // 6) GPU-ресурсы маркера (Mesh/Material) — из описаний в сущностях сцены.
    ResourceLoader::UploadAll(world, game.GetDevice());

    // =========================================================================
    // 7) Проверяемый вывод структуры дерева (entity id, иерархия, глоб. трансформы)
    // =========================================================================
    std::cout << "[Demo] Scene tree (" << scene.GetNodeCount() << " packed nodes):" << '\n';
    LogSceneTree(world, sceneRoot);

    if (failures == 0) {
        std::cout << "[Demo] ALL SCENE CHECKS PASSED" << '\n';
    } else {
        std::cout << "[Demo] " << failures << " CHECK(S) FAILED" << '\n';
    }

    std::cout << "[Demo] " << kTriangleCount
              << " triangles falling, rendered in ONE draw call." << '\n';

    game.Run();

    g_Game = nullptr;
    return failures == 0 ? 0 : 1;
}
