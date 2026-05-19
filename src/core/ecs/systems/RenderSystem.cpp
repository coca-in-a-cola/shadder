#include "RenderSystem.h"
#include "../Query.h"
#include "../components/Transform3D.h"
#include "../components/MeshComponent.h"
#include "../components/MaterialComponent.h"
#include "../../game/Game.h"
#include <iostream>

namespace shadder {

void RenderSystem::OnUpdate(World& world, float /*deltaTime*/) {
    if (!game_) return;

    auto* ctx = game_->GetContext();
    if (!ctx) return;

    Query<Transform3D, MeshComponent, MaterialComponent> q(world);
    q.ForEach([&](Entity /*e*/, Transform3D& /*tr*/, MeshComponent& mesh, MaterialComponent& mat) {
        // --- Set Pipeline State ---
        ctx->IASetInputLayout(mat.inputLayout.Get());
        ctx->IASetPrimitiveTopology(mesh.topology);
        if (mat.rasterizerState) {
            ctx->RSSetState(mat.rasterizerState.Get());
        }

        UINT stride = mesh.vertexStride;
        UINT offset = 0;
        ID3D11Buffer* vb = mesh.vertexBuffer.Get();
        ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
        ctx->IASetIndexBuffer(mesh.indexBuffer.Get(), mesh.indexFormat, 0);

        ctx->VSSetShader(mat.vertexShader.Get(), nullptr, 0);
        ctx->PSSetShader(mat.pixelShader.Get(), nullptr, 0);

        ctx->DrawIndexed(mesh.indexCount, 0, 0);
    });
}

} // namespace shadder
