#include "framework/modules/render/RenderSystem.h"
#include "core/ecs/Query.h"
#include "framework/modules/transform/Transform3D.h"
#include "framework/modules/render/MeshComponent.h"
#include "framework/modules/render/MaterialComponent.h"
#include "framework/modules/render/PhongMaterialComponent.h"
#include "framework/modules/render/DirectionalLightComponent.h"
#include "framework/modules/camera/CameraComponent.h"
#include "framework/game/Game.h"
#include <DirectXMath.h>

void RenderSystem::OnUpdate(World& world, float) {
    if (!game_) return;
    auto* ctx = game_->GetContext();
    auto* device = game_->GetDevice();
    if (!ctx || !device) return;

    // Slot b1 (ViewProjection) заполняет CameraSystem в PRE_RENDER.

    // --- Сбор данных DirectionalLight (берём первый активный) ---
    DirectionalLightComponent* light = nullptr;
    Transform3D* lightTransform = nullptr;
    Query<DirectionalLightComponent, Transform3D> lightWithTransformQ(world);
    lightWithTransformQ.ForEach([&](Entity, DirectionalLightComponent& l, Transform3D& t) {
        if (l.active && !light) {
            light = &l;
            lightTransform = &t;
        }
    });
    // Если есть свет без Transform3D, берём его
    if (!light) {
        Query<DirectionalLightComponent> lightOnlyQ(world);
        lightOnlyQ.ForEach([&](Entity, DirectionalLightComponent& l) {
            if (l.active && !light) {
                light = &l;
            }
        });
    }

    // --- Сбор CameraPos из активной камеры ---
    CameraComponent* cam = nullptr;
    Query<CameraComponent> camQ(world);
    camQ.ForEach([&](Entity, CameraComponent& c) {
        if (c.active && !cam) {
            cam = &c;
        }
    });

    // --- Light constant buffer (slot b2) ---
    struct LightBuffer {
        DirectX::XMFLOAT3 dir;
        float intensity;
        DirectX::XMFLOAT3 color;
        float _pad;
    };
    Microsoft::WRL::ComPtr<ID3D11Buffer> lightCB;
    if (light) {
        DirectX::XMFLOAT3 lightDir = light->direction;
        // Если есть Transform3D, вычисляем направление из вращения (forward vector)
        if (lightTransform) {
            using namespace DirectX;
            XMVECTOR q = XMLoadFloat4(&lightTransform->rotation);
            XMVECTOR forward = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), q);
            XMStoreFloat3(&lightDir, forward);
        }
        // Нормализуем
        XMVECTOR v = XMLoadFloat3(&lightDir);
        v = XMVector3Normalize(v);
        XMStoreFloat3(&lightDir, v);

        LightBuffer lb = { lightDir, light->intensity, light->color, 0.0f };
        D3D11_BUFFER_DESC bd = { sizeof(LightBuffer), D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0 };
        D3D11_SUBRESOURCE_DATA sd = { &lb };
        device->CreateBuffer(&bd, &sd, lightCB.GetAddressOf());
    } else {
        // Default light (white, down, intensity 1)
        LightBuffer lb = { { 0.0f, -1.0f, 0.0f }, 1.0f, { 1.0f, 1.0f, 1.0f }, 0.0f };
        D3D11_BUFFER_DESC bd = { sizeof(LightBuffer), D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0 };
        D3D11_SUBRESOURCE_DATA sd = { &lb };
        device->CreateBuffer(&bd, &sd, lightCB.GetAddressOf());
    }
    ctx->VSSetConstantBuffers(2, 1, lightCB.GetAddressOf());
    ctx->PSSetConstantBuffers(2, 1, lightCB.GetAddressOf());

    // --- Camera constant buffer (slot b3) ---
    struct CameraBuffer {
        DirectX::XMFLOAT3 pos;
        float _pad;
    };
    Microsoft::WRL::ComPtr<ID3D11Buffer> cameraCB;
    DirectX::XMFLOAT3 camPos = { 0.0f, 0.0f, -5.0f }; // default
    if (cam) {
        camPos = cam->eye;
    }
    CameraBuffer cb = { camPos, 0.0f };
    D3D11_BUFFER_DESC cbd = { sizeof(CameraBuffer), D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0 };
    D3D11_SUBRESOURCE_DATA csd = { &cb };
    device->CreateBuffer(&cbd, &csd, cameraCB.GetAddressOf());
    ctx->VSSetConstantBuffers(3, 1, cameraCB.GetAddressOf());
    ctx->PSSetConstantBuffers(3, 1, cameraCB.GetAddressOf());

    // --- Render regular MaterialComponent (POS_COLOR) ---
    Query<Transform3D, MeshComponent, MaterialComponent> q(world);
    q.ForEach([&](Entity, Transform3D& tr, MeshComponent& mesh, MaterialComponent& mat) {
        using namespace DirectX;
        XMMATRIX world = XMMatrixScaling(tr.scale.x, tr.scale.y, tr.scale.z)
                        * XMMatrixRotationQuaternion(XMLoadFloat4(&tr.rotation))
                        * XMMatrixTranslation(tr.position.x, tr.position.y, tr.position.z);

        // Per-object world CB (slot b0)
        Microsoft::WRL::ComPtr<ID3D11Buffer> worldCB;
        D3D11_BUFFER_DESC bd = { sizeof(XMMATRIX), D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0 };
        XMMATRIX worldT = XMMatrixTranspose(world);
        D3D11_SUBRESOURCE_DATA sd = { &worldT };
        device->CreateBuffer(&bd, &sd, worldCB.GetAddressOf());
        ctx->VSSetConstantBuffers(0, 1, worldCB.GetAddressOf());

        // Set pipeline
        ctx->IASetInputLayout(mat.inputLayout.Get());
        ctx->IASetPrimitiveTopology(mesh.topology);
        if (mat.rasterizerState) ctx->RSSetState(mat.rasterizerState.Get());

        UINT stride = mesh.vertexStride, offset = 0;
        ctx->IASetVertexBuffers(0, 1, mesh.vertexBuffer.GetAddressOf(), &stride, &offset);
        ctx->IASetIndexBuffer(mesh.indexBuffer.Get(), mesh.indexFormat, 0);
        ctx->VSSetShader(mat.vertexShader.Get(), nullptr, 0);
        ctx->PSSetShader(mat.pixelShader.Get(), nullptr, 0);

        ctx->DrawIndexed(mesh.indexCount, 0, 0);
    });

    // --- Render PhongMaterialComponent (POS_NORMAL_COLOR) ---
    Query<Transform3D, MeshComponent, PhongMaterialComponent> phongQ(world);
    phongQ.ForEach([&](Entity, Transform3D& tr, MeshComponent& mesh, PhongMaterialComponent& mat) {
        using namespace DirectX;
        XMMATRIX world = XMMatrixScaling(tr.scale.x, tr.scale.y, tr.scale.z)
                        * XMMatrixRotationQuaternion(XMLoadFloat4(&tr.rotation))
                        * XMMatrixTranslation(tr.position.x, tr.position.y, tr.position.z);

        // Per-object world CB (slot b0)
        Microsoft::WRL::ComPtr<ID3D11Buffer> worldCB;
        D3D11_BUFFER_DESC bd = { sizeof(XMMATRIX), D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0 };
        XMMATRIX worldT = XMMatrixTranspose(world);
        D3D11_SUBRESOURCE_DATA sd = { &worldT };
        device->CreateBuffer(&bd, &sd, worldCB.GetAddressOf());
        ctx->VSSetConstantBuffers(0, 1, worldCB.GetAddressOf());

        // Per-object material CB (slot b4)
        struct MaterialBuffer {
            DirectX::XMFLOAT3 ambient;
            float _pad1;
            DirectX::XMFLOAT3 diffuse;
            float _pad2;
            DirectX::XMFLOAT3 specular;
            float shininess;
        };
        Microsoft::WRL::ComPtr<ID3D11Buffer> materialCB;
        MaterialBuffer mb = { mat.ambient, 0.0f, mat.diffuse, 0.0f, mat.specular, mat.shininess };
        D3D11_BUFFER_DESC mbd = { sizeof(MaterialBuffer), D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0 };
        D3D11_SUBRESOURCE_DATA msd = { &mb };
        device->CreateBuffer(&mbd, &msd, materialCB.GetAddressOf());
        ctx->VSSetConstantBuffers(4, 1, materialCB.GetAddressOf());
        ctx->PSSetConstantBuffers(4, 1, materialCB.GetAddressOf());

        // Set pipeline
        ctx->IASetInputLayout(mat.inputLayout.Get());
        ctx->IASetPrimitiveTopology(mesh.topology);
        if (mat.rasterizerState) ctx->RSSetState(mat.rasterizerState.Get());

        UINT stride = mesh.vertexStride, offset = 0;
        ctx->IASetVertexBuffers(0, 1, mesh.vertexBuffer.GetAddressOf(), &stride, &offset);
        ctx->IASetIndexBuffer(mesh.indexBuffer.Get(), mesh.indexFormat, 0);
        ctx->VSSetShader(mat.vertexShader.Get(), nullptr, 0);
        ctx->PSSetShader(mat.pixelShader.Get(), nullptr, 0);

        ctx->DrawIndexed(mesh.indexCount, 0, 0);
    });
}