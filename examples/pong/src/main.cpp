#include "shadder.hpp"

#include <windows.h>
#include <d3dcompiler.h>
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
static const float kPaddleW = 16.0f;
static const float kPaddleH = 160.0f;
static const float kBallSize = 16.0f;

struct Vertex {
    XMFLOAT4 pos;
    XMFLOAT4 col;
};

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

static bool CreateQuadResources(Game& game,
                                Microsoft::WRL::ComPtr<ID3D11VertexShader>& vs,
                                Microsoft::WRL::ComPtr<ID3D11PixelShader>& ps,
                                Microsoft::WRL::ComPtr<ID3D11InputLayout>& layout,
                                Microsoft::WRL::ComPtr<ID3D11RasterizerState>& rast) {
    ID3D11Device* device = game.GetDevice();

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, err;
    HRESULT hr = D3DCompileFromFile(L"data/PongShader.hlsl", nullptr, nullptr,
        "VSMain", "vs_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0,
        vsBlob.GetAddressOf(), err.GetAddressOf());
    if (FAILED(hr)) {
        if (err) std::cout << "VS error: " << (char*)err->GetBufferPointer() << '\n';
        return false;
    }

    hr = D3DCompileFromFile(L"data/PongShader.hlsl", nullptr, nullptr,
        "PSMain", "ps_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0,
        psBlob.GetAddressOf(), err.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        if (err) std::cout << "PS error: " << (char*)err->GetBufferPointer() << '\n';
        return false;
    }

    device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                               nullptr, vs.GetAddressOf());
    device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                              nullptr, ps.GetAddressOf());

    D3D11_INPUT_ELEMENT_DESC inputLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    device->CreateInputLayout(inputLayout, 2,
                              vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                              layout.GetAddressOf());

    CD3D11_RASTERIZER_DESC rastDesc(D3D11_DEFAULT);
    rastDesc.CullMode = D3D11_CULL_NONE;
    device->CreateRasterizerState(&rastDesc, rast.GetAddressOf());

    return true;
}

static Entity CreateQuadEntity(Game& game, World& world, float width, float height,
                                const XMFLOAT4& color,
                                ID3D11VertexShader* vs, ID3D11PixelShader* ps,
                                ID3D11InputLayout* layout,
                                ID3D11RasterizerState* rast) {
    ID3D11Device* device = game.GetDevice();
    Entity e = world.CreateEntity();

    auto& mesh = world.AddComponent<MeshComponent>(e);
    auto& mat = world.AddComponent<MaterialComponent>(e);

    float hw = width * 0.5f, hh = height * 0.5f;
    Vertex verts[4] = {
        { { -hw, -hh, 0.0f, 1.0f }, color },
        { {  hw, -hh, 0.0f, 1.0f }, color },
        { {  hw,  hh, 0.0f, 1.0f }, color },
        { { -hw,  hh, 0.0f, 1.0f }, color },
    };
    UINT indices[6] = { 0, 1, 2, 2, 3, 0 };

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
    mesh.indexCount = 6;
    mesh.vertexStride = sizeof(Vertex);
    mesh.indexFormat = DXGI_FORMAT_R32_UINT;

    mat.vertexShader = vs;
    mat.pixelShader = ps;
    mat.inputLayout = layout;
    mat.rasterizerState = rast;

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

    // Create shared quad shader resources
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> ps;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rastState;
    if (!CreateQuadResources(game, vs, ps, inputLayout, rastState)) {
        std::cout << "Failed to create quad resources!\n";
        return 1;
    }

    XMFLOAT4 white = { 1.0f, 1.0f, 1.0f, 1.0f };

    // Player paddle (left)
    {
        Entity e = CreateQuadEntity(game, world, kPaddleW, kPaddleH, white,
                                     vs.Get(), ps.Get(), inputLayout.Get(), rastState.Get());
        auto& tr = world.AddComponent<Transform3D>(e);
        tr.position = { kPaddleW * 0.5f + 10.0f, kScreenH * 0.5f, 0.0f };
        tr.scale = { 1.0f, 1.0f, 1.0f };
        world.AddComponent<PlayerPaddleTag>(e);
    }

    // AI paddle (right)
    {
        Entity e = CreateQuadEntity(game, world, kPaddleW, kPaddleH, white,
                                     vs.Get(), ps.Get(), inputLayout.Get(), rastState.Get());
        auto& tr = world.AddComponent<Transform3D>(e);
        tr.position = { kScreenW - kPaddleW * 0.5f - 10.0f, kScreenH * 0.5f, 0.0f };
        tr.scale = { 1.0f, 1.0f, 1.0f };
        world.AddComponent<AiPaddleTag>(e);
    }

    // Ball
    {
        Entity e = CreateQuadEntity(game, world, kBallSize, kBallSize, white,
                                     vs.Get(), ps.Get(), inputLayout.Get(), rastState.Get());
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

    std::cout << "[Pong] Game started! W/S to move left paddle.\n";

    game.Run();

    g_Game = nullptr;
    return 0;
}
