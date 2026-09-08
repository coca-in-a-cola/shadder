#pragma once

// -----------------------------------------------------------------------------
// RainScene.h — demo PackedScene for examples/framework (SH-P migration demo).
//
// Builds the whole demo world as ONE PackedScene (Godot-style: the scene is a
// serializable description of the world, instantiated into the ECS World):
//
//   RainDemo (container root, no components)
//   ├── Camera (camera prefab -> CameraComponent)            [nested prefab scene]
//   ├── Rain (batch components; instances attached at runtime)
//   │   └── TriangleBatch (created at runtime, D3D resources)
//   └── Marker (quad prefab -> Mesh+Material, local (430,120)) [spliced prefab,
//                                                              per-instance
//                                                              transform override]
//
// Prefab scenes are passed IN from main() and stay alive there — PackedScene
// nesting stores SHARED POINTER REFERENCES (Godot instanced-scene semantics):
// the nested scene must outlive every Instantiate() of the built scene.
//
// The 15000 ECS triangle instances can't live in the packed description (they
// are runtime entities), so the "Rain" node keeps only its batch components;
// the instance entities are attached by InstantiateRainScene.
// -----------------------------------------------------------------------------

#include "shadder.hpp"

#include <windows.h>
#include <d3dcompiler.h>

#include "shared/prefabs/CameraPrefabs.h"
#include "shared/prefabs/MeshPrefabs.h"

#include <DirectXMath.h>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace rain_demo {

// -----------------------------------------------------------------------------
// Параметры демо
// -----------------------------------------------------------------------------
static const int kScreenW = 900;
static const int kScreenH = 900;
static const int kTriangleCount = 15000; // тысячи треугольников — один draw call
static const float kTriHalfSize = 5.0f;  // полразмера треугольника в пикселях

inline bool PosNear(const DirectX::XMFLOAT3& p, float x, float y, float z,
                    float eps = 1e-4f) {
    return std::fabs(p.x - x) <= eps && std::fabs(p.y - y) <= eps && std::fabs(p.z - z) <= eps;
}

// -----------------------------------------------------------------------------
// FallTeleportSystem — система-наследник ISystem (живёт в коде демо).
// Когда треугольник падает ниже экрана, телепортирует его обратно наверх
// со случайным X. Работает через ECS, в фазе UPDATE после MovementSystem.
// -----------------------------------------------------------------------------
class FallTeleportSystem : public ISystem {
    float screenW_;
    float screenH_;
public:
    FallTeleportSystem(float w, float h) : screenW_(w), screenH_(h) {}

    void OnUpdate(World& world, float) override {
        const float margin = kTriHalfSize * 2.0f;
        Query<Transform3D, VelocityComponent> q(world);
        q.ForEach([&](Entity, Transform3D& tr, VelocityComponent&) {
            if (tr.position.y < -margin) {
                // Появляемся над верхней кромкой со случайным X.
                tr.position.y = screenH_ + margin;
                tr.position.x = static_cast<float>(rand() % static_cast<int>(screenW_));
            }
        });
    }
};

// -----------------------------------------------------------------------------
// Window proc / глобальный указатель на игру
// -----------------------------------------------------------------------------
inline Game* g_Game = nullptr;

inline LRESULT CALLBACK RainWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (g_Game && g_Game->MessageHandler(hwnd, msg, wparam, lparam)) {
        return 0;
    }
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(hwnd, msg, wparam, lparam);
    }
}

// Per-instance данные на CPU (должны совпадать с InstancedRenderSystem.cpp).
struct InstanceGpuData {
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4 color;
};

// -----------------------------------------------------------------------------
// CreateTriangleBatch: общая геометрия (один треугольник) + материал + шейдер.
// Возвращает сущность-батч с InstancedMeshComponent + MaterialComponent.
// -----------------------------------------------------------------------------
inline Entity CreateTriangleBatch(Game& game, World& world) {
    ID3D11Device* device = game.GetDevice();

    Entity batch = world.CreateEntity();
    auto& mesh = world.AddComponent<InstancedMeshComponent>(batch);
    auto& mat = world.AddComponent<MaterialComponent>(batch);

    // --- Геометрия одного треугольника: position(float4) + color(float4) ---
    struct Vertex {
        DirectX::XMFLOAT4 pos;
        DirectX::XMFLOAT4 col;
    };
    Vertex verts[3] = {
        { {  0.0f,  kTriHalfSize, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
        { {  kTriHalfSize, -kTriHalfSize, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
        { { -kTriHalfSize, -kTriHalfSize, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    };
    UINT indices[3] = { 0, 1, 2 };

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.ByteWidth = sizeof(verts);
    D3D11_SUBRESOURCE_DATA vbData = { verts, 0, 0 };
    device->CreateBuffer(&vbDesc, &vbData, mesh.vertexBuffer.GetAddressOf());

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.ByteWidth = sizeof(indices);
    D3D11_SUBRESOURCE_DATA ibData = { indices, 0, 0 };
    device->CreateBuffer(&ibDesc, &ibData, mesh.indexBuffer.GetAddressOf());

    mesh.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    mesh.indexCount = 3;
    mesh.vertexStride = sizeof(Vertex);
    mesh.instanceStride = sizeof(InstanceGpuData);
    mesh.indexFormat = DXGI_FORMAT_R32_UINT;

    // --- Компиляция единого шейдера для всех треугольников -----------------
    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, err;
    HRESULT res = D3DCompileFromFile(
        L"data/RainInstanced.hlsl", nullptr, nullptr,
        "VSMain", "vs_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0,
        vsBlob.GetAddressOf(), err.GetAddressOf());
    if (FAILED(res)) {
        if (err) std::cout << "VS error: " << (char*)err->GetBufferPointer() << '\n';
        return batch;
    }

    res = D3DCompileFromFile(
        L"data/RainInstanced.hlsl", nullptr, nullptr,
        "PSMain", "ps_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0,
        psBlob.GetAddressOf(), err.ReleaseAndGetAddressOf());
    if (FAILED(res)) {
        if (err) std::cout << "PS error: " << (char*)err->GetBufferPointer() << '\n';
        return batch;
    }

    device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                               nullptr, mat.vertexShader.GetAddressOf());
    device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                              nullptr, mat.pixelShader.GetAddressOf());

    // --- Input layout: slot 0 (per-vertex) + slot 1 (per-instance) ---------
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,                            D3D11_INPUT_PER_VERTEX_DATA,   0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA,   0 },
        { "WORLD",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0,                            D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD",    1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD",    2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD",    3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "ICOLOR",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    };
    device->CreateInputLayout(layout, 7, vsBlob->GetBufferPointer(),
                              vsBlob->GetBufferSize(), mat.inputLayout.GetAddressOf());

    CD3D11_RASTERIZER_DESC rastDesc(D3D11_DEFAULT);
    rastDesc.CullMode = D3D11_CULL_NONE;
    device->CreateRasterizerState(&rastDesc, mat.rasterizerState.GetAddressOf());

    return batch;
}

// -----------------------------------------------------------------------------
// BuildRainScene: собрать ВСЮ демо-сцену как один PackedScene.
//
// cameraPrefab / quadPrefab — вложенные префабы (PackedScene-механизм),
// созданные в main() и живущие там (shared reference: сцена хранит УКАЗАТЕЛЬ
// на них, они должны переживать каждый Instantiate() собранной сцены).
// -----------------------------------------------------------------------------
inline PackedScene BuildRainScene(const PackedScene& cameraPrefab,
                                  const PackedScene& quadPrefab) {
    return SceneBuilder()
        .Node("RainDemo")
        // 1) Вложенная сцена-префаб: камера. Instantiate() рекурсивно
        //    распакует её корень как ребёнка RainDemo (Godot instanced scene).
        .Inline(cameraPrefab, "Camera")
        // 2) Батч дождя: компоненты сплайсом прямо в описание сцены.
        .Node("Rain")
        .With<MaterialComponent>([](MaterialComponent&) {})
        .End()
        // 3) Вложенная сцена-префаб: квад-маркер с per-instance позицией
        //    (Godot: placement transform of an instanced scene). Сплайс копией,
        //    поэтому lifetime-ограничений на quadPrefab нет.
        .WithScene(quadPrefab, "Marker",
                   [] {
                       Transform3D t;
                       t.position = {430.0f, 120.0f, 0.0f};
                       return t;
                   }())
        .End()
        .Build();
}

// -----------------------------------------------------------------------------
// InstantiateRainScene: runtime-часть (батч и треугольники) поверх корня сцены.
// Возвращает корневую SceneNode сцены.
// -----------------------------------------------------------------------------
inline SceneNode::Ptr AttachRainInstances(Game& game, World& world,
                                          SceneNode::Ptr sceneRoot) {
    // --- Runtime-часть: батч (GPU-ресурсы) -----------------------------------
    // Батч не может жить в packed-описании (нужен D3D-девайс), поэтому его
    // сущность создаётся отдельно и прикрепляется к узлу "Rain" как ребёнок —
    // структура дерева при этом остаётся Godot-подобной.
    Entity batch = CreateTriangleBatch(game, world);
    SceneNode::Ptr rainNode = sceneRoot->FindChild("Rain");
    if (rainNode) {
        SceneNode::Ptr batchNode = std::make_shared<SceneNode>(batch);
        batchNode->SetName("TriangleBatch");
        rainNode->AddChild(batchNode);
    }

    // --- Треугольники: тысячи ECS-сущностей + привязка к батчу ---------------
    srand(1234);
    for (int i = 0; i < kTriangleCount; ++i) {
        Entity e = world.CreateEntity();

        auto& tr = world.AddComponent<Transform3D>(e);
        tr.position = {
            static_cast<float>(rand() % kScreenW),
            static_cast<float>(rand() % (kScreenH * 2)), // часть стартует выше экрана
            0.0f
        };
        float angle = (static_cast<float>(rand()) / RAND_MAX) * DirectX::XM_2PI;
        DirectX::XMStoreFloat4(&tr.rotation,
            DirectX::XMQuaternionRotationRollPitchYaw(0, 0, angle));
        float s = 0.5f + (static_cast<float>(rand()) / RAND_MAX) * 1.5f;
        tr.scale = { s, s, 1.0f };

        // Падение вниз (ортопроекция: y вниз = к нулю) — через VelocityComponent.
        auto& vel = world.AddComponent<VelocityComponent>(e);
        float speed = 120.0f + (static_cast<float>(rand()) / RAND_MAX) * 380.0f;
        vel.velocity = { 0.0f, -speed, 0.0f };

        // Привязка к батчу + случайный цвет (синева дождя).
        DirectX::XMFLOAT4 color = {
            0.3f + (static_cast<float>(rand()) / RAND_MAX) * 0.3f,
            0.5f + (static_cast<float>(rand()) / RAND_MAX) * 0.4f,
            0.8f + (static_cast<float>(rand()) / RAND_MAX) * 0.2f,
            1.0f
        };
        world.AddComponent<InstanceTag>(e, batch.index, color);
    }

    return sceneRoot;
}

// -----------------------------------------------------------------------------
// LogSceneTree: проверяемый вывод структуры дерева (entity id, иерархия,
// глобальные трансформы) — то, что требует задача.
// -----------------------------------------------------------------------------
inline void LogSceneTree(World& world, const SceneNode::Ptr& node, int depth = 0) {
    for (int i = 0; i < depth; ++i) std::cout << "  ";
    std::cout << "- \"" << node->GetName() << "\" entity=" << node->GetEntity().index
              << " global=(" << node->GetGlobalTransform().position.x << ", "
              << node->GetGlobalTransform().position.y << ", "
              << node->GetGlobalTransform().position.z << ")";
    if (node->GetEntity().IsValid()) {
        if (world.HasComponent<CameraComponent>(node->GetEntity())) std::cout << " [CameraComponent]";
        if (world.HasComponent<MeshComponent>(node->GetEntity())) std::cout << " [MeshComponent]";
        if (world.HasComponent<InstancedMeshComponent>(node->GetEntity())) std::cout << " [InstancedMesh]";
    }
    std::cout << '\n';
    for (const auto& child : node->GetChildren()) {
        LogSceneTree(world, child, depth + 1);
    }
}

} // namespace rain_demo
