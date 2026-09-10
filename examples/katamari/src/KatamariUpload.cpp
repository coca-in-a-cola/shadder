// KatamariUploadSystem — реализация. См. KatamariUpload.h.

#include "KatamariUpload.h"
#include "KatamariComponents.h"

#include "shadder.hpp"
#include "core/ecs/Query.h"
#include "framework/modules/render/ObjLoader.h"
#include "framework/modules/render/MeshComponent.h"
#include "framework/modules/render/PhongMaterialComponent.h"
#include <d3d11.h>
#include <wrl.h>
#include <iostream>

using Microsoft::WRL::ComPtr;

void KatamariUploadSystem::OnUpdate(World& world, float) {
    if (!device_) return;
    ID3D11Device* device = device_;

    // Шейдеры/лейаут фреймворка берём у уже загруженного Phong-материала
    // (ResourceLoader скомпилировал их для пола/шара — переиспользуем).
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11RasterizerState> rast;
    {
        Query<PhongMaterialComponent> mq(world);
        mq.ForEach([&](Entity, PhongMaterialComponent& m) {
            if (!vs && m.uploaded && m.vertexShader) {
                vs = m.vertexShader;
                ps = m.pixelShader;
                layout = m.inputLayout;
                rast = m.rasterizerState;
            }
        });
    }
    if (!vs) {
        std::cout << "[KatamariUpload] no uploaded Phong material found — custom meshes stay unloaded\n";
        return;
    }

    int uploaded = 0;
    Query<KatamariCustomMesh, MeshComponent, PhongMaterialComponent> q(world);
    q.ForEach([&](Entity, KatamariCustomMesh& cm, MeshComponent& mesh, PhongMaterialComponent& mat) {
        if (cm.uploaded || cm.indices.empty()) return;

        // Буферы кладём в MeshComponent — RenderSystem читает VB/IB оттуда.
        D3D11_BUFFER_DESC vbDesc = {};
        vbDesc.Usage = D3D11_USAGE_DEFAULT;
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbDesc.ByteWidth = static_cast<UINT>(sizeof(ObjMeshData::Vertex) * cm.vertices.size());
        D3D11_SUBRESOURCE_DATA vbData = { cm.vertices.data(), 0, 0 };
        if (FAILED(device->CreateBuffer(&vbDesc, &vbData, mesh.vertexBuffer.ReleaseAndGetAddressOf()))) {
            std::cout << "[KatamariUpload] VB create failed\n";
            return;
        }

        D3D11_BUFFER_DESC ibDesc = {};
        ibDesc.Usage = D3D11_USAGE_DEFAULT;
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        ibDesc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * cm.indices.size());
        D3D11_SUBRESOURCE_DATA ibData = { cm.indices.data(), 0, 0 };
        if (FAILED(device->CreateBuffer(&ibDesc, &ibData, mesh.indexBuffer.ReleaseAndGetAddressOf()))) {
            std::cout << "[KatamariUpload] IB create failed\n";
            return;
        }

        // Пресет POS_NORMAL_COLOR: TOPOLOGY TRIANGLELIST, индекс 32 бит.
        mesh.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        mesh.indexCount = static_cast<uint32_t>(cm.indices.size());
        mesh.vertexStride = sizeof(ObjMeshData::Vertex);
        mesh.indexFormat = DXGI_FORMAT_R32_UINT;
        mesh.uploaded = true;

        // Шейдеры фреймворка (тот же phong-пасс, что у пола/шара).
        mat.vertexShader = vs;
        mat.pixelShader = ps;
        mat.inputLayout = layout;
        mat.rasterizerState = rast;

        cm.uploaded = true;
        ++uploaded;
    });

    std::cout << "[KatamariUpload] uploaded custom meshes: " << uploaded << '\n';
}
