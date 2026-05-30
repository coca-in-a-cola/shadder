#include "framework/modules/render/InstancedRenderSystem.h"
#include "framework/modules/render/InstancedMeshComponent.h"
#include "framework/modules/render/InstanceTag.h"
#include "framework/modules/render/MaterialComponent.h"
#include "framework/modules/transform/Transform3D.h"
#include "framework/game/Game.h"
#include "dev/display/DisplayWin32.h"
#include "ecs/Query.h"
#include "ecs/World.h"

#include <DirectXMath.h>
#include <unordered_map>
#include <vector>

namespace {
struct InstanceGpuData {
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4 color;
};
} // namespace

void InstancedRenderSystem::OnUpdate(World& world, float) {
    if (!game_) return;
    auto* ctx = game_->GetContext();
    auto* device = game_->GetDevice();
    auto* display = game_->GetDisplay();
    if (!ctx || !device || !display) return;

    // Slot b1 (ViewProjection) заполняет CameraSystem в PRE_RENDER.

    using namespace DirectX;

    std::unordered_map<EntityIndex, std::vector<InstanceGpuData>> byBatch;
    Query<Transform3D, InstanceTag> q(world);
    q.ForEach([&](Entity, Transform3D& tr, InstanceTag& tag) {
        XMMATRIX world =
            XMMatrixScaling(tr.scale.x, tr.scale.y, tr.scale.z) *
            XMMatrixRotationQuaternion(XMLoadFloat4(&tr.rotation)) *
            XMMatrixTranslation(tr.position.x, tr.position.y, tr.position.z);

        InstanceGpuData inst;
        XMStoreFloat4x4(&inst.world, XMMatrixTranspose(world));
        inst.color = tag.color;
        byBatch[tag.batch].push_back(inst);
    });

    if (byBatch.empty()) return;

    for (auto& [batchIdx, instances] : byBatch) {
        Entity batchEntity{ batchIdx, world.GetGeneration(batchIdx) };
        auto* mesh = world.GetComponent<InstancedMeshComponent>(batchEntity);
        auto* mat = world.GetComponent<MaterialComponent>(batchEntity);
        if (!mesh || !mat || instances.empty()) continue;

        const uint32_t needed = static_cast<uint32_t>(instances.size());

        if (!mesh->instanceBuffer || mesh->instanceCapacity < needed) {
            mesh->instanceBuffer.Reset();
            D3D11_BUFFER_DESC bd = {};
            bd.ByteWidth = sizeof(InstanceGpuData) * needed;
            bd.Usage = D3D11_USAGE_DYNAMIC;
            bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            if (FAILED(device->CreateBuffer(&bd, nullptr, mesh->instanceBuffer.GetAddressOf()))) continue;
            mesh->instanceCapacity = needed;
            mesh->instanceStride = sizeof(InstanceGpuData);
        }

        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (SUCCEEDED(ctx->Map(mesh->instanceBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            memcpy(mapped.pData, instances.data(), sizeof(InstanceGpuData) * needed);
            ctx->Unmap(mesh->instanceBuffer.Get(), 0);
        }
        mesh->instanceCount = needed;

        ctx->IASetInputLayout(mat->inputLayout.Get());
        ctx->IASetPrimitiveTopology(mesh->topology);
        if (mat->rasterizerState) ctx->RSSetState(mat->rasterizerState.Get());

        ID3D11Buffer* vbs[2] = { mesh->vertexBuffer.Get(), mesh->instanceBuffer.Get() };
        UINT strides[2] = { mesh->vertexStride, mesh->instanceStride };
        UINT offsets[2] = { 0, 0 };
        ctx->IASetVertexBuffers(0, 2, vbs, strides, offsets);
        ctx->IASetIndexBuffer(mesh->indexBuffer.Get(), mesh->indexFormat, 0);

        ctx->VSSetShader(mat->vertexShader.Get(), nullptr, 0);
        ctx->PSSetShader(mat->pixelShader.Get(), nullptr, 0);

        ctx->DrawIndexedInstanced(mesh->indexCount, mesh->instanceCount, 0, 0, 0);
    }
}
