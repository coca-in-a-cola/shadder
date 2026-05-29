#pragma once

#include "ecs/Component.h"
#include <d3d11.h>
#include <wrl.h>

struct MeshComponent : public ComponentBase {
public:
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
    D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    uint32_t indexCount = 0;
    uint32_t vertexStride = 0;
    DXGI_FORMAT indexFormat = DXGI_FORMAT_R32_UINT;
};
