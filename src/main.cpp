// main.cpp : Entry point — creates window, initializes Game, runs game loop.

#include <windows.h>
#include <iostream>
#include <memory>

#include "core/DisplayWin32.h"
#include "core/game/Game.h"
#include "core/ecs/components/Transform3D.h"
#include "core/ecs/components/MeshComponent.h"
#include "core/ecs/components/MaterialComponent.h"
#include "core/ecs/systems/RenderSystem.h"

// Global game pointer for the window procedure
Game* g_Game = nullptr;

LRESULT CALLBACK WndProc(HWND hwnd, UINT umessage, WPARAM wparam, LPARAM lparam)
{
    if (g_Game && g_Game->MessageHandler(hwnd, umessage, wparam, lparam))
    {
        return 0;
    }

    switch (umessage)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hwnd, umessage, wparam, lparam);
    }
}

int main()
{
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    // Create the display window
    DisplayWin32 display(L"My3DApp", hInstance, 800, 800, WndProc);

    // Create and initialize the Game
    Game game;
    g_Game = &game;

    if (!game.Initialize(&display))
    {
        std::cout << "Failed to initialize the game engine!" << '\n';
        return 1;
    }

    // --- ECS triangle entity ------------------------------------------------
    auto& world = game.GetWorld();
    world.RegisterSystem<shadder::RenderSystem>(shadder::SystemPhase::RENDER, &game);

    shadder::Entity e = world.CreateEntity();

    // Transform
    auto& tr = world.AddComponent<shadder::Transform3D>(e);
    tr.position = { 0.0f, 0.0f, 0.0f };
    tr.rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
    tr.scale    = { 1.0f, 1.0f, 1.0f };

    // Mesh
    auto& mesh = world.AddComponent<shadder::MeshComponent>(e);
    DirectX::XMFLOAT4 points[8] = {
        { 0.5f,  0.5f, 0.5f, 1.0f}, { 1.0f,  0.0f, 0.0f, 1.0f},
        {-0.5f, -0.5f, 0.5f, 1.0f}, { 0.0f,  0.0f, 1.0f, 1.0f},
        { 0.5f, -0.5f, 0.5f, 1.0f}, { 0.0f,  1.0f, 0.0f, 1.0f},
        {-0.5f,  0.5f, 0.5f, 1.0f}, { 1.0f,  1.0f, 1.0f, 1.0f}
    };
    int indices[] = {0, 1, 2, 1, 0, 3};

    ID3D11Device* device = game.GetDevice();

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = 0;
    vbDesc.MiscFlags = 0;
    vbDesc.StructureByteStride = 0;
    vbDesc.ByteWidth = sizeof(DirectX::XMFLOAT4) * std::size(points);

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = points;
    device->CreateBuffer(&vbDesc, &vbData, &mesh.vertexBuffer);

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.CPUAccessFlags = 0;
    ibDesc.MiscFlags = 0;
    ibDesc.StructureByteStride = 0;
    ibDesc.ByteWidth = sizeof(int) * std::size(indices);

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices;
    device->CreateBuffer(&ibDesc, &ibData, &mesh.indexBuffer);

    mesh.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    mesh.indexCount = 6;
    mesh.vertexStride = sizeof(DirectX::XMFLOAT4) * 2; // position + color interleaved
    mesh.indexFormat = DXGI_FORMAT_R32_UINT;

    // Material
    auto& mat = world.AddComponent<shadder::MaterialComponent>(e);

    ID3DBlob* errorCode = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> vsByteCode;
    HRESULT res = D3DCompileFromFile(
        L"data/MyVeryFirstShader.hlsl",
        nullptr, nullptr,
        "VSMain", "vs_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0,
        vsByteCode.GetAddressOf(), &errorCode);

    if (FAILED(res)) {
        if (errorCode) {
            std::cout << "ECS VS compile error: " << (char*)errorCode->GetBufferPointer() << '\n';
            errorCode->Release();
        } else {
            std::cout << "Missing shader file for ECS entity" << '\n';
        }
        // Run anyway — entity won't render but game continues.
    } else {
        Microsoft::WRL::ComPtr<ID3DBlob> psByteCode;
        D3D_SHADER_MACRO macros[] = {
            "TEST",   "1",
            "TCOLOR", "float4(0.0f, 1.0f, 0.0f, 1.0f)",
            nullptr,  nullptr
        };

        res = D3DCompileFromFile(
            L"data/MyVeryFirstShader.hlsl",
            macros, nullptr,
            "PSMain", "ps_5_0",
            D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0,
            psByteCode.GetAddressOf(), &errorCode);

        if (FAILED(res)) {
            if (errorCode) {
                std::cout << "ECS PS compile error: " << (char*)errorCode->GetBufferPointer() << '\n';
                errorCode->Release();
            }
        } else {
            device->CreateVertexShader(
                vsByteCode->GetBufferPointer(),
                vsByteCode->GetBufferSize(),
                nullptr, &mat.vertexShader);

            device->CreatePixelShader(
                psByteCode->GetBufferPointer(),
                psByteCode->GetBufferSize(),
                nullptr, &mat.pixelShader);

            D3D11_INPUT_ELEMENT_DESC inputElements[] = {
                {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
                 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
                 D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
            };

            device->CreateInputLayout(
                inputElements, 2,
                vsByteCode->GetBufferPointer(),
                vsByteCode->GetBufferSize(),
                &mat.inputLayout);

            CD3D11_RASTERIZER_DESC rastDesc(D3D11_DEFAULT);
            rastDesc.CullMode = D3D11_CULL_NONE;
            device->CreateRasterizerState(&rastDesc, &mat.rasterizerState);
        }
    }

    std::cout << "[ECS] Entity " << e.index << " created with Transform3D + MeshComponent + MaterialComponent" << std::endl;

    // Run the game loop
    game.Run();

    g_Game = nullptr;
    return 0;
}
