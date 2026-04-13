#include "TriangleComponent.h"
#include "Game.h"
#include <iostream>

#pragma comment(lib, "d3dcompiler.lib")

TriangleComponent::TriangleComponent(Game* inGame)
    : GameComponent(inGame)
    , vertexShaderByteCode(nullptr)
    , pixelShaderByteCode(nullptr)
{
    // Triangle vertices: position + color interleaved (4 vertices for a quad)
    points[0] = DirectX::XMFLOAT4( 0.5f,  0.5f, 0.5f, 1.0f);
    points[1] = DirectX::XMFLOAT4( 1.0f,  0.0f, 0.0f, 1.0f);
    points[2] = DirectX::XMFLOAT4(-0.5f, -0.5f, 0.5f, 1.0f);
    points[3] = DirectX::XMFLOAT4( 0.0f,  0.0f, 1.0f, 1.0f);
    points[4] = DirectX::XMFLOAT4( 0.5f, -0.5f, 0.5f, 1.0f);
    points[5] = DirectX::XMFLOAT4( 0.0f,  1.0f, 0.0f, 1.0f);
    points[6] = DirectX::XMFLOAT4(-0.5f,  0.5f, 0.5f, 1.0f);
    points[7] = DirectX::XMFLOAT4( 1.0f,  1.0f, 1.0f, 1.0f);
}

TriangleComponent::~TriangleComponent()
{
    DestroyResources();
}

void TriangleComponent::Initialize()
{
    auto device = game->GetDevice();

    // --- Compile Vertex Shader ---
    ID3DBlob* errorCode = nullptr;
    HRESULT res = D3DCompileFromFile(
        L"data/MyVeryFirstShader.hlsl",
        nullptr /*macros*/, nullptr /*include*/,
        "VSMain", "vs_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0,
        &vertexShaderByteCode, &errorCode);

    if (FAILED(res)) {
        if (errorCode) {
            std::cout << "Vertex shader error: "
                      << (char*)errorCode->GetBufferPointer() << std::endl;
            errorCode->Release();
        } else {
            std::cout << "Missing shader file: data/MyVeryFirstShader.hlsl" << std::endl;
        }
        return;
    }

    // --- Compile Pixel Shader ---
    D3D_SHADER_MACRO macros[] = {
        "TEST",   "1",
        "TCOLOR", "float4(0.0f, 1.0f, 0.0f, 1.0f)",
        nullptr,  nullptr
    };

    res = D3DCompileFromFile(
        L"data/MyVeryFirstShader.hlsl",
        macros /*macros*/, nullptr /*include*/,
        "PSMain", "ps_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0,
        &pixelShaderByteCode, &errorCode);

    if (FAILED(res)) {
        if (errorCode) {
            std::cout << "Pixel shader error: "
                      << (char*)errorCode->GetBufferPointer() << std::endl;
            errorCode->Release();
        }
        return;
    }

    // --- Create Shader Objects ---
    device->CreateVertexShader(
        vertexShaderByteCode->GetBufferPointer(),
        vertexShaderByteCode->GetBufferSize(),
        nullptr, &vertexShader);

    device->CreatePixelShader(
        pixelShaderByteCode->GetBufferPointer(),
        pixelShaderByteCode->GetBufferSize(),
        nullptr, &pixelShader);

    // --- Input Layout ---
    D3D11_INPUT_ELEMENT_DESC inputElements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
         D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    device->CreateInputLayout(
        inputElements, 2,
        vertexShaderByteCode->GetBufferPointer(),
        vertexShaderByteCode->GetBufferSize(),
        &layout);

    // --- Vertex Buffer ---
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = 0;
    vbDesc.MiscFlags = 0;
    vbDesc.StructureByteStride = 0;
    vbDesc.ByteWidth = sizeof(DirectX::XMFLOAT4) * std::size(points);

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = points;

    device->CreateBuffer(&vbDesc, &vbData, &vertices);

    // --- Index Buffer ---
    int indices[] = {0, 1, 2, 1, 0, 3};

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.CPUAccessFlags = 0;
    ibDesc.MiscFlags = 0;
    ibDesc.StructureByteStride = 0;
    ibDesc.ByteWidth = sizeof(int) * std::size(indices);

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices;

    device->CreateBuffer(&ibDesc, &ibData, &indexBuffer);

    // --- Rasterizer State ---
    CD3D11_RASTERIZER_DESC rastDesc(D3D11_DEFAULT);
    rastDesc.CullMode = D3D11_CULL_NONE;

    device->CreateRasterizerState(&rastDesc, &rastState);
}

void TriangleComponent::Update(float deltaTime)
{
    // Nothing to update for a static triangle
}

void TriangleComponent::Draw()
{
    auto context = game->GetContext();

    context->RSSetState(rastState.Get());
    context->IASetInputLayout(layout.Get());
    context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    UINT strides[] = {32};
    UINT offsets[] = {0};
    ID3D11Buffer* vb = vertices.Get();
    context->IASetVertexBuffers(0, 1, &vb, strides, offsets);
    context->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

    context->VSSetShader(vertexShader.Get(), nullptr, 0);
    context->PSSetShader(pixelShader.Get(), nullptr, 0);

    context->DrawIndexed(6, 0, 0);
}

void TriangleComponent::DestroyResources()
{
    if (vertexShaderByteCode) {
        vertexShaderByteCode->Release();
        vertexShaderByteCode = nullptr;
    }
    if (pixelShaderByteCode) {
        pixelShaderByteCode->Release();
        pixelShaderByteCode = nullptr;
    }
}
