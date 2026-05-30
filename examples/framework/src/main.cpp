// =============================================================================
//  examples/framework — Демо текущего C++ фреймворка.
//
//  «Дождь» из треугольников: все треугольники рисуются ОДНИМ HLSL-шейдером
//  за один вызов DrawIndexedInstanced (см. InstancedRenderSystem во фреймворке).
//
//  Что показывает демо / какие доработки фреймворка понадобились:
//   * Старый RenderSystem создавал новый constant buffer на КАЖДУЮ сущность
//     КАЖДЫЙ кадр — на сотнях треугольников это убивает производительность.
//     Добавлены InstancedMeshComponent / InstanceTag / InstancedRenderSystem:
//     один шейдер, один draw call, один динамический instance-буфер.
//   * VP-матрица теперь обновляется при ресайзе окна (старый код хардкодил её).
//   * Падение и телепорт реализованы через системы (ECS), а не в игровом цикле:
//       - MovementSystem (фреймворк) — двигает по VelocityComponent;
//       - FallTeleportSystem (локально в демо) — возвращает треугольник наверх.
//
//  Импорт фреймворка — строго через include/shadder.hpp.
// =============================================================================

#include "shadder.hpp"

#include <windows.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace shadder;
using namespace DirectX;

// -----------------------------------------------------------------------------
// Параметры демо
// -----------------------------------------------------------------------------
static const int kScreenW = 900;
static const int kScreenH = 900;
static const int kTriangleCount = 15000; // тысячи треугольников — один draw call
static const float kTriHalfSize = 5.0f; // полразмера треугольника в пикселях

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
static Game* g_Game = nullptr;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
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

// -----------------------------------------------------------------------------
// Per-instance данные на CPU (должны совпадать с InstancedRenderSystem.cpp).
// Здесь только для расчёта размеров; реально заполняет систему рендера.
// -----------------------------------------------------------------------------
struct InstanceGpuData {
    XMFLOAT4X4 world;
    XMFLOAT4 color;
};

// -----------------------------------------------------------------------------
// Создание батча: общая геометрия (один треугольник) + материал + шейдер.
// Возвращает сущность-батч с InstancedMeshComponent + MaterialComponent.
// -----------------------------------------------------------------------------
static Entity CreateTriangleBatch(Game& game, World& world) {
    ID3D11Device* device = game.GetDevice();

    Entity batch = world.CreateEntity();
    auto& mesh = world.AddComponent<InstancedMeshComponent>(batch);
    auto& mat = world.AddComponent<MaterialComponent>(batch);

    // --- Геометрия одного треугольника: position(float4) + color(float4) ---
    struct Vertex {
        XMFLOAT4 pos;
        XMFLOAT4 col;
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
int main() {
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    DisplayWin32 display(L"Framework Demo - Triangle Rain", hInstance, kScreenW, kScreenH, WndProc);

    Game game;
    g_Game = &game;
    if (!game.Initialize(&display)) {
        std::cout << "Failed to initialize the game engine!" << '\n';
        return 1;
    }

    World& world = game.GetWorld();

    // --- Камера: ортографика в экранных пикселях (origin = левый-верхний угол,
    //     ось Y вниз). Её ViewProj CameraSystem кладёт в constant buffer b1,
    //     откуда его читает RainInstanced.hlsl. Без камеры b1 пуст -> чёрный экран.
    {
        Entity cameraEntity = world.CreateEntity();
        auto& cam = world.AddComponent<CameraComponent>(cameraEntity);
        cam.projection = CameraComponent::Projection::ORTHO_SCREEN;
        cam.screenW = static_cast<float>(kScreenW);
        cam.screenH = static_cast<float>(kScreenH);
        cam.nearZ = -1.0f; // 2D: пускаем диапазон Z вокруг нуля
        cam.farZ  = 1.0f;
        cam.active = true;
    }

    // --- Системы (порядок важен) -------------------------------------------
    // PRE_RENDER: камера обновляет ViewProjection (slot b1) до рендера.
    world.RegisterSystem<CameraSystem>(SystemPhase::PRE_RENDER, &game);
    // UPDATE: сначала движение, затем телепорт упавших.
    world.RegisterSystem<MovementSystem>(SystemPhase::UPDATE);
    world.RegisterSystem<FallTeleportSystem>(SystemPhase::UPDATE,
                                             static_cast<float>(kScreenW),
                                             static_cast<float>(kScreenH));
    // RENDER: единый instanced-рендер всех треугольников.
    world.RegisterSystem<InstancedRenderSystem>(SystemPhase::RENDER, &game);

    // --- Батч с общей геометрией + шейдером ---------------------------------
    Entity batch = CreateTriangleBatch(game, world);

    // --- Создание дождя: тысячи сущностей-треугольников ---------------------
    srand(1234);
    for (int i = 0; i < kTriangleCount; ++i) {
        Entity e = world.CreateEntity();

        auto& tr = world.AddComponent<Transform3D>(e);
        tr.position = {
            static_cast<float>(rand() % kScreenW),
            static_cast<float>(rand() % (kScreenH * 2)), // часть стартует выше экрана
            0.0f
        };
        float angle = (static_cast<float>(rand()) / RAND_MAX) * XM_2PI;
        XMStoreFloat4(&tr.rotation, XMQuaternionRotationRollPitchYaw(0, 0, angle));
        float s = 0.5f + (static_cast<float>(rand()) / RAND_MAX) * 1.5f;
        tr.scale = { s, s, 1.0f };

        // Падение вниз (ортопроекция: y вниз = к нулю) — через VelocityComponent.
        auto& vel = world.AddComponent<VelocityComponent>(e);
        float speed = 120.0f + (static_cast<float>(rand()) / RAND_MAX) * 380.0f;
        vel.velocity = { 0.0f, -speed, 0.0f };

        // Привязка к батчу + случайный цвет (синева дождя).
        XMFLOAT4 color = {
            0.3f + (static_cast<float>(rand()) / RAND_MAX) * 0.3f,
            0.5f + (static_cast<float>(rand()) / RAND_MAX) * 0.4f,
            0.8f + (static_cast<float>(rand()) / RAND_MAX) * 0.2f,
            1.0f
        };
        world.AddComponent<InstanceTag>(e, batch.index, color);
    }

    std::cout << "[Demo] " << kTriangleCount
              << " triangles falling, rendered in ONE draw call.\n";

    game.Run();

    g_Game = nullptr;
    return 0;
}
