#pragma once

#include <DirectXMath.h>
#include <cstddef>

// Shared vertex ABI for generated meshes and imported OBJ meshes.
struct Vertex3D {
    DirectX::XMFLOAT4 pos;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT4 color;
    DirectX::XMFLOAT2 uv = { 0.0f, 0.0f };
};
static_assert(offsetof(Vertex3D, uv) == 44);
static_assert(sizeof(Vertex3D) == 52);
