#include "framework/systems/RenderSystem.h"
#include "ecs/Query.h"
#include "framework/components/Transform3D.h"
#include "framework/components/MeshComponent.h"
#include "framework/components/MaterialComponent.h"
#include "framework/game/Game.h"
#include "dev/display/DisplayWin32.h"
#include <DirectXMath.h>


void RenderSystem::OnUpdate(World& world, float) {
    if (!game_) return;
    auto* ctx = game_->GetContext();
    auto* device = game_->GetDevice();
    auto* display = game_->GetDisplay();
    if (!ctx || !device || !display) return;

    // Orthographic VP buffer (once)
    static Microsoft::WRL::ComPtr<ID3D11Buffer> vpCB;
    static bool vpInit = false;
    if (!vpInit) {
        using namespace DirectX;
        float w = static_cast<float>(display->GetWidth());
        float h = static_cast<float>(display->GetHeight());
        XMMATRIX vp = XMMatrixOrthographicOffCenterLH(0, w, 0, h, 0, 1);
        D3D11_BUFFER_DESC bd = { sizeof(XMMATRIX), D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0 };
        D3D11_SUBRESOURCE_DATA sd = { &vp };
        device->CreateBuffer(&bd, &sd, vpCB.GetAddressOf());
        ctx->VSSetConstantBuffers(1, 1, vpCB.GetAddressOf());
        vpInit = true;
    }

    Query<Transform3D, MeshComponent, MaterialComponent> q(world);
    q.ForEach([&](Entity, Transform3D& tr, MeshComponent& mesh, MaterialComponent& mat) {
        using namespace DirectX;
        XMMATRIX world = XMMatrixScaling(tr.scale.x, tr.scale.y, tr.scale.z)
                        * XMMatrixRotationQuaternion(XMLoadFloat4(&tr.rotation))
                        * XMMatrixTranslation(tr.position.x, tr.position.y, tr.position.z);

        // Per-object world CB
        Microsoft::WRL::ComPtr<ID3D11Buffer> worldCB;
        D3D11_BUFFER_DESC bd = { sizeof(XMMATRIX), D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0 };
        D3D11_SUBRESOURCE_DATA sd = { &world };
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


