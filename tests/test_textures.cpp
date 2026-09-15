// Build: scons -C tests. Run from repo root: tests/bin/test_textures.exe
// WARP integration tests: real D3D11 resources, shader compilation, GPU readback.
#include "core/ecs/World.h"
#include "framework/modules/render/ResourceLoader.h"
#include "framework/modules/render/MeshComponent.h"
#include "framework/modules/render/MaterialComponent.h"
#include "framework/modules/render/PhongMaterialComponent.h"
#include "framework/modules/render/Vertex3D.h"
#include "framework/modules/render/ObjLoader.h"
#include <d3d11sdklayers.h>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

using Microsoft::WRL::ComPtr;
using namespace DirectX;
#define CHECK(condition) do { if (!(condition)) { std::cerr << "FAIL line " << __LINE__ << ": " #condition "\n"; std::exit(1); } } while (0)
#define HR(call) CHECK(SUCCEEDED(call))

template<class T>
std::vector<T> ReadBuffer(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Buffer* source) {
    D3D11_BUFFER_DESC desc;
    source->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = desc.MiscFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Buffer> staging;
    HR(device->CreateBuffer(&desc, nullptr, staging.GetAddressOf()));
    context->CopyResource(staging.Get(), source);
    D3D11_MAPPED_SUBRESOURCE mapped;
    HR(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));
    const auto* values = static_cast<const T*>(mapped.pData);
    std::vector<T> result(values, values + desc.ByteWidth / sizeof(T));
    context->Unmap(staging.Get(), 0);
    return result;
}

template<class T>
ComPtr<ID3D11Buffer> Constant(ID3D11Device* device, const T& value) {
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(T);
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    D3D11_SUBRESOURCE_DATA data = { &value };
    ComPtr<ID3D11Buffer> result;
    HR(device->CreateBuffer(&desc, &data, result.GetAddressOf()));
    return result;
}

int main() {
    std::cout << std::unitbuf;
    std::wcout << std::unitbuf;
    std::cout << "Creating WARP device\n";
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    HR(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_DEBUG,
        nullptr, 0, D3D11_SDK_VERSION, device.GetAddressOf(), nullptr, context.GetAddressOf()));
    ComPtr<ID3D11InfoQueue> messages;
    HR(device.As(&messages));
    std::cout << "Loading textures\n";

    auto white = TextureLoader::Load(device.Get(), L"");
    CHECK(white && white->width == 1 && white->mipLevels == 1);
    std::weak_ptr<Texture2D> lifetime;
    {
        auto a = TextureLoader::Load(device.Get(), L"examples/phong/data/textures/1.jpg");
        auto b = TextureLoader::Load(device.Get(), L"examples/phong/data/textures/../textures/1.jpg");
        CHECK(a && a == b && a->width > 1 && a->mipLevels > 1);
        D3D11_SHADER_RESOURCE_VIEW_DESC desc;
        a->view->GetDesc(&desc);
        CHECK(desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);
        lifetime = a;
    }
    CHECK(lifetime.expired()); // Cache must not own the resource.
    auto missing = TextureLoader::Load(device.Get(), L"tests/does-not-exist.jpg");
    CHECK(missing && missing->width == 2);
    auto image = TextureLoader::Load(device.Get(), L"examples/phong/data/textures/1.jpg");
    for (auto name : { L"2.jpg", L"3.jpg" }) {
        auto texture = TextureLoader::Load(device.Get(), std::wstring(L"examples/phong/data/textures/") + name);
        CHECK(texture && texture->width > 2 && texture->mipLevels > 1);
    }

    World world;
    world.RegisterComponent<MeshComponent>();
    world.RegisterComponent<MaterialComponent>();
    world.RegisterComponent<PhongMaterialComponent>();
    std::array<Entity, 3> entities;
    const MeshComponent::Primitive primitives[] = { MeshComponent::Primitive::CUBE,
        MeshComponent::Primitive::SPHERE, MeshComponent::Primitive::PLANE };
    for (int i = 0; i < 3; ++i) {
        entities[i] = world.CreateEntity();
        world.AddComponent<MeshComponent>(entities[i]).primitive = primitives[i];
        auto& mat = world.AddComponent<PhongMaterialComponent>(entities[i]);
        if (i == 0) mat.shading = PhongMaterialDesc::Shading::Diffuse;
        if (i != 2) mat.diffuseTexturePath = L"examples/phong/data/textures/1.jpg";
    }
    ResourceLoader::UploadAll(world, device.Get());
    for (auto entity : entities) {
        auto* mesh = world.GetComponent<MeshComponent>(entity);
        auto* mat = world.GetComponent<PhongMaterialComponent>(entity);
        CHECK(mesh->uploaded && mesh->vertexStride == sizeof(Vertex3D));
        CHECK(mat->uploaded && mat->vertexShader && mat->pixelShader && mat->sampler);
        auto vertices = ReadBuffer<Vertex3D>(device.Get(), context.Get(), mesh->vertexBuffer.Get());
        auto indices = ReadBuffer<unsigned>(device.Get(), context.Get(), mesh->indexBuffer.Get());
        for (auto index : indices) CHECK(index < vertices.size());
        if (mesh->primitive == MeshComponent::Primitive::CUBE) {
            CHECK(vertices.size() == 24);
            for (size_t f = 0; f < 6; ++f) {
                CHECK(vertices[f * 4].uv.x == 0 && vertices[f * 4].uv.y == 1);
                CHECK(vertices[f * 4 + 2].uv.x == 1 && vertices[f * 4 + 2].uv.y == 0);
            }
        }
        if (mesh->primitive == MeshComponent::Primitive::SPHERE) {
            for (int row = 1; row < mesh->sphereStacks; ++row) {
                const auto& a = vertices[row * (mesh->sphereSlices + 1)];
                const auto& b = vertices[row * (mesh->sphereSlices + 1) + mesh->sphereSlices];
                CHECK(a.uv.x == 0 && b.uv.x == 1);
                CHECK(std::abs(a.pos.x - b.pos.x) < 1e-6f && std::abs(a.pos.z - b.pos.z) < 1e-6f);
            }
        }
    }
    auto* diffuse = world.GetComponent<PhongMaterialComponent>(entities[0]);
    CHECK(diffuse->diffuseTexture == image);
    CHECK(world.GetComponent<PhongMaterialComponent>(entities[1])->diffuseTexture == image);
    auto* solid = world.GetComponent<PhongMaterialComponent>(entities[2]);
    CHECK(solid->diffuseTexture == white);
    PhongMaterialDesc preset = *solid;
    preset.diffuseTexturePath = L"examples/phong/data/textures/1.jpg";
    solid->SetMaterial(preset);
    CHECK(!solid->uploaded);
    ResourceLoader::UploadAll(world, device.Get());
    CHECK(solid->uploaded && solid->diffuseTexture == image);

    ObjMeshData obj;
    CHECK(ObjLoader::Load(L"tests/data/uv_seam.wavefront", {1,1,1,1}, obj));
    CHECK(obj.indices.size() == 6 && obj.vertices.size() == 5);
    const auto& seamA = obj.vertices[obj.indices[0]];
    const auto& seamB = obj.vertices[obj.indices[3]];
    CHECK(seamA.uv.x == 0 && seamA.uv.y == 1);
    CHECK(seamB.uv.x == 1 && seamB.uv.y == 0);
    CHECK(seamA.normal.y > 0 && seamA.normal.z > 0);
    CHECK(seamA.normal.y == seamB.normal.y && seamA.normal.z == seamB.normal.z);
    CHECK(ObjLoader::Load(L"tests/data/uv_seam.wavefront", {1,1,1,1}, obj));
    CHECK(obj.indices.size() == 6); // Reusing an output does not append old faces.

    // Draw the production diffuse shader to an offscreen target. Ambient-only
    // lighting makes a 0.5 linear tint become ~188/255 with explicit SRGB output.
    D3D11_TEXTURE2D_DESC targetDesc = {};
    targetDesc.Width = targetDesc.Height = 32;
    targetDesc.MipLevels = targetDesc.ArraySize = 1;
    targetDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    targetDesc.SampleDesc.Count = 1;
    targetDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> target, staging;
    HR(device->CreateTexture2D(&targetDesc, nullptr, target.GetAddressOf()));
    ComPtr<ID3D11RenderTargetView> rtv;
    HR(device->CreateRenderTargetView(target.Get(), nullptr, rtv.GetAddressOf()));
    targetDesc.BindFlags = 0;
    targetDesc.Usage = D3D11_USAGE_STAGING;
    targetDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    HR(device->CreateTexture2D(&targetDesc, nullptr, staging.GetAddressOf()));
    context->OMSetRenderTargets(1, rtv.GetAddressOf(), nullptr);
    D3D11_VIEWPORT viewport = { 0, 0, 32, 32, 0, 1 };
    context->RSSetViewports(1, &viewport);
    context->RSSetState(diffuse->rasterizerState.Get());
    const Vertex3D triangle[] = {
        { {-1,-1,0,1}, {0,0,-1}, {1,1,1,1}, {0,1} },
        { {-1, 3,0,1}, {0,0,-1}, {1,1,1,1}, {0,-1} },
        { { 3,-1,0,1}, {0,0,-1}, {1,1,1,1}, {2,1} }
    };
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof(triangle);
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vbData = { triangle };
    ComPtr<ID3D11Buffer> vb;
    HR(device->CreateBuffer(&vbDesc, &vbData, vb.GetAddressOf()));
    UINT stride = sizeof(Vertex3D), offset = 0;
    context->IASetVertexBuffers(0, 1, vb.GetAddressOf(), &stride, &offset);
    context->IASetInputLayout(diffuse->inputLayout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(diffuse->vertexShader.Get(), nullptr, 0);
    context->PSSetShader(diffuse->pixelShader.Get(), nullptr, 0);
    auto matrix = Constant(device.Get(), XMMatrixIdentity());
    context->VSSetConstantBuffers(0, 1, matrix.GetAddressOf());
    context->VSSetConstantBuffers(1, 1, matrix.GetAddressOf());
    const std::array<float, 8> light = { 0,0,1,1, 1,1,1,0 };
    auto lightCB = Constant(device.Get(), light);
    context->PSSetConstantBuffers(2, 1, lightCB.GetAddressOf());
    const std::array<float, 24> material = { 1,1,1,1, 0,0,0,0, 0,0,0,32,
                                             .5f,.5f,.5f,1, 1,1,0,0, 1,0,0,0 };
    auto materialCB = Constant(device.Get(), material);
    context->PSSetConstantBuffers(4, 1, materialCB.GetAddressOf());
    context->PSSetSamplers(0, 1, diffuse->sampler.GetAddressOf());
    D3D11_SAMPLER_DESC shadowSamplerDesc = {};
    shadowSamplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    shadowSamplerDesc.AddressU = shadowSamplerDesc.AddressV = shadowSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    shadowSamplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    shadowSamplerDesc.BorderColor[0] = shadowSamplerDesc.BorderColor[1] = shadowSamplerDesc.BorderColor[2] = shadowSamplerDesc.BorderColor[3] = 1.0f;
    shadowSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    ComPtr<ID3D11SamplerState> shadowSampler;
    HR(device->CreateSamplerState(&shadowSamplerDesc, shadowSampler.GetAddressOf()));
    context->PSSetSamplers(1, 1, shadowSampler.GetAddressOf());
    auto draw = [&](const std::shared_ptr<Texture2D>& texture) {
        context->PSSetShaderResources(0, 1, texture->view.GetAddressOf());
        context->Draw(3, 0);
        context->CopyResource(staging.Get(), target.Get());
        D3D11_MAPPED_SUBRESOURCE mapped;
        HR(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));
        std::vector<unsigned char> pixels;
        for (unsigned y = 0; y < 32; ++y) {
            const auto* row = static_cast<unsigned char*>(mapped.pData) + y * mapped.RowPitch;
            pixels.insert(pixels.end(), row, row + 128);
        }
        context->Unmap(staging.Get(), 0);
        return pixels;
    };
    const auto neutral = draw(white);
    CHECK(neutral[0] >= 187 && neutral[0] <= 189 && neutral[3] == 255);
    const auto textured = draw(image);
    CHECK(textured != neutral);
    CHECK(draw(white) == neutral); // No stale binding after a textured draw.

    context->ClearState();
    context->Flush();
    for (UINT64 i = 0; i < messages->GetNumStoredMessages(); ++i) {
        SIZE_T size = 0;
        HR(messages->GetMessage(i, nullptr, &size));
        std::vector<unsigned char> bytes(size);
        auto* message = reinterpret_cast<D3D11_MESSAGE*>(bytes.data());
        HR(messages->GetMessage(i, message, &size));
        if (message->Severity <= D3D11_MESSAGE_SEVERITY_WARNING) {
            std::cerr << message->pDescription << '\n';
            CHECK(false);
        }
    }
    std::cout << "PASS: WARP textures, mipmaps, cache/lifetime, material presets, UVs, shaders, SRGB readback, clean debug layer\n";
}
