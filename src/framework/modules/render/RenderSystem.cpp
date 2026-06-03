#include "framework/modules/render/RenderSystem.h"
#include "core/ecs/Query.h"
#include "framework/modules/transform/Transform3D.h"
#include "framework/modules/render/MeshComponent.h"
#include "framework/modules/render/MaterialComponent.h"
#include "framework/game/Game.h"
#include <DirectXMath.h>

void RenderSystem::OnUpdate(World& world, float) {
    if (!game_) return;
    auto* ctx = game_->GetContext();
    auto* device = game_->GetDevice();
    if (!ctx || !device) return;

    // Slot b1 (ViewProjection) заполняет CameraSystem в PRE_RENDER.

    Query<Transform3D, MeshComponent, MaterialComponent> q(world);
    q.ForEach([&](Entity, Transform3D& tr, MeshComponent& mesh, MaterialComponent& mat) {
        using namespace DirectX;
        XMMATRIX world = XMMatrixScaling(tr.scale.x, tr.scale.y, tr.scale.z)
                        * XMMatrixRotationQuaternion(XMLoadFloat4(&tr.rotation))
                        * XMMatrixTranslation(tr.position.x, tr.position.y, tr.position.z);

        // Per-object world CB
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
}
