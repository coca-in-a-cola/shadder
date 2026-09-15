#include "framework/modules/render/RenderSystem.h"
#include "core/ecs/Query.h"
#include "framework/modules/transform/Transform3D.h"
#include "framework/modules/render/MeshComponent.h"
#include "framework/modules/render/MaterialComponent.h"
#include "framework/modules/render/PhongMaterialComponent.h"
#include "framework/modules/render/ResourceLoader.h"
#include "framework/modules/render/DirectionalLightComponent.h"
#include "framework/modules/camera/CameraComponent.h"
#include "framework/game/Game.h"
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <wrl.h>
#include <algorithm>
#include <cmath>
#include <iostream>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace {
struct ShadowResources {
    ID3D11Device* device = nullptr;
    unsigned resolution = 0;
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11DepthStencilView> dsv;
    ComPtr<ID3D11ShaderResourceView> srv;
    ComPtr<ID3D11VertexShader> vertexShader;
    ComPtr<ID3D11InputLayout> inputLayout;
    ComPtr<ID3D11SamplerState> sampler;
    ComPtr<ID3D11RasterizerState> rasterizer;
};

XMMATRIX WorldMatrix(const Transform3D& tr) {
    return XMMatrixScaling(tr.scale.x, tr.scale.y, tr.scale.z)
         * XMMatrixRotationQuaternion(XMLoadFloat4(&tr.rotation))
         * XMMatrixTranslation(tr.position.x, tr.position.y, tr.position.z);
}

bool EnsureShadowResources(ID3D11Device* device, unsigned resolution, int depthBias,
                           float slopeBias, ShadowResources& out) {
    resolution = (std::max)(256u, (std::min)(8192u, resolution));
    if (out.device != device) {
        out = {};
        out.device = device;
    }
    if (!out.vertexShader) {
        ComPtr<ID3DBlob> shader, errors;
        if (FAILED(D3DCompileFromFile(L"data/shaders/shadows/DepthVertexShader.hlsl", nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0",
            D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0,
            shader.GetAddressOf(), errors.GetAddressOf()))) {
            if (errors) std::cerr << "[Shadows] " << static_cast<const char*>(errors->GetBufferPointer()) << '\n';
            return false;
        }
        if (FAILED(device->CreateVertexShader(shader->GetBufferPointer(), shader->GetBufferSize(),
            nullptr, out.vertexShader.GetAddressOf()))) return false;
        D3D11_INPUT_ELEMENT_DESC position = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,
                                               0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 };
        if (FAILED(device->CreateInputLayout(&position, 1, shader->GetBufferPointer(),
            shader->GetBufferSize(), out.inputLayout.GetAddressOf()))) return false;
        D3D11_SAMPLER_DESC sampler = {};
        sampler.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
        sampler.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
        sampler.BorderColor[0] = sampler.BorderColor[1] = sampler.BorderColor[2] = sampler.BorderColor[3] = 1.0f;
        sampler.MaxLOD = D3D11_FLOAT32_MAX;
        if (FAILED(device->CreateSamplerState(&sampler, out.sampler.GetAddressOf()))) return false;
    }
    if (!out.texture || out.resolution != resolution) {
        out.texture.Reset(); out.dsv.Reset(); out.srv.Reset();
        D3D11_TEXTURE2D_DESC texture = {};
        texture.Width = texture.Height = resolution;
        texture.MipLevels = texture.ArraySize = 1;
        texture.Format = DXGI_FORMAT_R32_TYPELESS;
        texture.SampleDesc.Count = 1;
        texture.Usage = D3D11_USAGE_DEFAULT;
        texture.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(device->CreateTexture2D(&texture, nullptr, out.texture.GetAddressOf()))) return false;
        D3D11_DEPTH_STENCIL_VIEW_DESC dsv = {};
        dsv.Format = DXGI_FORMAT_D32_FLOAT;
        dsv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        dsv.Texture2D.MipSlice = 0;
        if (FAILED(device->CreateDepthStencilView(out.texture.Get(), &dsv, out.dsv.GetAddressOf()))) return false;
        D3D11_SHADER_RESOURCE_VIEW_DESC srv = {};
        srv.Format = DXGI_FORMAT_R32_FLOAT;
        srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srv.Texture2D.MostDetailedMip = 0;
        srv.Texture2D.MipLevels = 1;
        if (FAILED(device->CreateShaderResourceView(out.texture.Get(), &srv, out.srv.GetAddressOf()))) return false;
        out.resolution = resolution;
    }
    out.rasterizer.Reset();
    D3D11_RASTERIZER_DESC raster = {};
    raster.FillMode = D3D11_FILL_SOLID;
    raster.CullMode = D3D11_CULL_BACK;
    raster.DepthClipEnable = TRUE;
    raster.DepthBias = depthBias;
    raster.SlopeScaledDepthBias = slopeBias;
    raster.DepthBiasClamp = 0.0f;
    if (FAILED(device->CreateRasterizerState(&raster, out.rasterizer.GetAddressOf()))) return false;
    return true;
}
}

void RenderSystem::OnUpdate(World& world, float) {
    if (!game_) return;
    auto* ctx = game_->GetContext();
    auto* device = game_->GetDevice();
    if (!ctx || !device) return;
    // Lazy upload also handles newly created objects and SetMaterial() at runtime.
    ResourceLoader::UploadAll(world, device);

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
    XMFLOAT3 resolvedLightDir = { 0.0f, -1.0f, 0.0f };
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
        DirectX::XMVECTOR v = DirectX::XMLoadFloat3(&lightDir);
        v = DirectX::XMVector3Normalize(v);
        DirectX::XMStoreFloat3(&lightDir, v);
        resolvedLightDir = lightDir;

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

    struct ShadowMatrixBuffer {
        XMMATRIX lightViewProj;
        XMFLOAT2 texelSize;
        float bias;
        float enabled;
    };
    static ShadowResources shadow;
    ComPtr<ID3D11Buffer> shadowCB;
    const bool shadowsActive = light && light->shadowsEnabled &&
        EnsureShadowResources(device, light->shadowResolution, light->shadowDepthBias,
                              light->shadowSlopeScaledDepthBias, shadow);
    ShadowMatrixBuffer shadowData = { XMMatrixIdentity(), { 0.0f, 0.0f }, 0.0f, 0.0f };
    if (shadowsActive) {
        const XMVECTOR center = XMLoadFloat3(&light->shadowCenter);
        const XMVECTOR eye = XMVectorSubtract(center,
            XMVectorScale(XMLoadFloat3(&resolvedLightDir), light->shadowDistance));
        const XMFLOAT3 up = std::abs(resolvedLightDir.y) > 0.98f
            ? XMFLOAT3{ 0.0f, 0.0f, 1.0f } : XMFLOAT3{ 0.0f, 1.0f, 0.0f };
        const float size = (std::max)(0.01f, light->shadowOrthoSize);
        const XMMATRIX view = XMMatrixLookAtLH(eye, center, XMLoadFloat3(&up));
        const XMMATRIX projection = XMMatrixOrthographicLH(size, size,
            (std::max)(0.001f, light->shadowNearZ),
            (std::max)(light->shadowNearZ + 0.001f, light->shadowFarZ));
        shadowData.lightViewProj = XMMatrixTranspose(XMMatrixMultiply(view, projection));
        shadowData.texelSize = { 1.0f / shadow.resolution, 1.0f / shadow.resolution };
        shadowData.bias = light->shadowBias;
        shadowData.enabled = 1.0f;
    }
    D3D11_BUFFER_DESC shadowDesc = { sizeof(ShadowMatrixBuffer), D3D11_USAGE_DEFAULT,
        D3D11_BIND_CONSTANT_BUFFER, 0, 0 };
    D3D11_SUBRESOURCE_DATA shadowInit = { &shadowData };
    if (SUCCEEDED(device->CreateBuffer(&shadowDesc, &shadowInit, shadowCB.GetAddressOf()))) {
        ctx->VSSetConstantBuffers(5, 1, shadowCB.GetAddressOf());
        ctx->PSSetConstantBuffers(5, 1, shadowCB.GetAddressOf());
    }
    if (shadowsActive && shadowCB) {
        ID3D11ShaderResourceView* noShadow = nullptr;
        ctx->PSSetShaderResources(1, 1, &noShadow);
        ctx->OMSetRenderTargets(0, nullptr, shadow.dsv.Get());
        ctx->ClearDepthStencilView(shadow.dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
        D3D11_VIEWPORT viewport = { 0, 0, static_cast<float>(shadow.resolution),
            static_cast<float>(shadow.resolution), 0, 1 };
        ctx->RSSetViewports(1, &viewport);
        ctx->RSSetState(shadow.rasterizer.Get());
        ctx->IASetInputLayout(shadow.inputLayout.Get());
        ctx->VSSetShader(shadow.vertexShader.Get(), nullptr, 0);
        ctx->PSSetShader(nullptr, nullptr, 0);
        Query<Transform3D, MeshComponent> casters(world);
        casters.ForEach([&](Entity, Transform3D& tr, MeshComponent& mesh) {
            if (!mesh.castsShadow || !mesh.uploaded || !mesh.vertexBuffer || !mesh.indexBuffer) return;
            const XMMATRIX worldT = XMMatrixTranspose(WorldMatrix(tr));
            D3D11_BUFFER_DESC worldDesc = { sizeof(XMMATRIX), D3D11_USAGE_DEFAULT,
                D3D11_BIND_CONSTANT_BUFFER, 0, 0 };
            D3D11_SUBRESOURCE_DATA worldInit = { &worldT };
            ComPtr<ID3D11Buffer> worldCB;
            if (FAILED(device->CreateBuffer(&worldDesc, &worldInit, worldCB.GetAddressOf()))) return;
            ctx->VSSetConstantBuffers(0, 1, worldCB.GetAddressOf());
            UINT stride = mesh.vertexStride, offset = 0;
            ctx->IASetPrimitiveTopology(mesh.topology);
            ctx->IASetVertexBuffers(0, 1, mesh.vertexBuffer.GetAddressOf(), &stride, &offset);
            ctx->IASetIndexBuffer(mesh.indexBuffer.Get(), mesh.indexFormat, 0);
            ctx->DrawIndexed(mesh.indexCount, 0, 0);
        });
        game_->RestoreDefaultTargets();
    }
    static ComPtr<ID3D11SamplerState> disabledShadowSampler;
    if (!disabledShadowSampler) {
        D3D11_SAMPLER_DESC sampler = {};
        sampler.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
        sampler.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
        sampler.BorderColor[0] = sampler.BorderColor[1] = sampler.BorderColor[2] = sampler.BorderColor[3] = 1.0f;
        sampler.MaxLOD = D3D11_FLOAT32_MAX;
        device->CreateSamplerState(&sampler, disabledShadowSampler.GetAddressOf());
    }
    ID3D11ShaderResourceView* shadowView = shadowsActive ? shadow.srv.Get() : nullptr;
    ID3D11SamplerState* shadowSampler = shadowsActive ? shadow.sampler.Get() : disabledShadowSampler.Get();
    ctx->PSSetShaderResources(1, 1, &shadowView);
    ctx->PSSetSamplers(1, 1, &shadowSampler);

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
        if (!mat.uploaded || !mesh.vertexBuffer || !mesh.indexBuffer) return;
        XMMATRIX world = WorldMatrix(tr);

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
        if (!mat.uploaded || !mesh.vertexBuffer || !mesh.indexBuffer || !mat.diffuseTexture) return;
        XMMATRIX world = WorldMatrix(tr);

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
            float encodeSRGB;
            DirectX::XMFLOAT3 diffuse;
            float _pad2;
            DirectX::XMFLOAT3 specular;
            float shininess;
            DirectX::XMFLOAT4 baseColor;
            DirectX::XMFLOAT2 uvScale;
            DirectX::XMFLOAT2 uvOffset;
            float receiveShadows;
            float _pad3[3];
        };
        static_assert(sizeof(MaterialBuffer) == 96);
        Microsoft::WRL::ComPtr<ID3D11Buffer> materialCB;
        MaterialBuffer mb = { mat.ambient,
            (mat.linearLighting || !mat.diffuseTexturePath.empty()) ? 1.0f : 0.0f,
            mat.diffuse, 0.0f, mat.specular, mat.shininess, mat.baseColor, mat.uvScale, mat.uvOffset,
            mat.receivesShadow ? 1.0f : 0.0f, { 0.0f, 0.0f, 0.0f } };
        D3D11_BUFFER_DESC mbd = { sizeof(MaterialBuffer), D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0 };
        D3D11_SUBRESOURCE_DATA msd = { &mb };
        device->CreateBuffer(&mbd, &msd, materialCB.GetAddressOf());
        ctx->VSSetConstantBuffers(4, 1, materialCB.GetAddressOf());
        ctx->PSSetConstantBuffers(4, 1, materialCB.GetAddressOf());
        ctx->PSSetShaderResources(0, 1, mat.diffuseTexture->view.GetAddressOf());
        ctx->PSSetSamplers(0, 1, mat.sampler.GetAddressOf());

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
    // Do not retain the last material through the context across frames/passes.
    ID3D11ShaderResourceView* noTexture = nullptr;
    ID3D11SamplerState* noSampler = nullptr;
    ctx->PSSetShaderResources(0, 1, &noTexture);
    ctx->PSSetSamplers(0, 1, &noSampler);
    ctx->PSSetShaderResources(1, 1, &noTexture);
    ctx->PSSetSamplers(1, 1, &noSampler);
}
