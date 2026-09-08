#include "framework/modules/render/ResourceLoader.h"
#include "framework/modules/render/MeshComponent.h"
#include "framework/modules/render/MaterialComponent.h"
#include "framework/modules/render/PhongMaterialComponent.h"
#include "core/ecs/Query.h"
#include <windows.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <wrl.h>
#include <cmath>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace {

// Путь к дефолтному шейдеру фреймворка (используется, если shaderPath пуст).
const wchar_t* kDefaultShaderPath = L"data/Default2D.hlsl";

// Дефолтные Phong-шейдеры (VS/PS в отдельных файлах одной папки).
const wchar_t* kDefaultPhongVSPath = L"data/shaders/phong/VertexShader.hlsl";
const wchar_t* kDefaultPhongPSPath = L"data/shaders/phong/PixelShader.hlsl";

// Каталог, где лежит исполняемый файл (с завершающим '\').
std::wstring ExeDir() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring path(buf, n);
    size_t slash = path.find_last_of(L"\\/");
    return (slash == std::wstring::npos) ? std::wstring() : path.substr(0, slash + 1);
}

// Резолвит путь к ассету: сначала относительно текущей рабочей директории,
// затем относительно каталога exe. Это убирает зависимость от того, откуда
// запущен исполняемый файл (CWD может быть любым).
std::wstring ResolveAssetPath(const std::wstring& rel) {
    if (GetFileAttributesW(rel.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return rel; // найден относительно CWD
    }
    std::wstring nearExe = ExeDir() + rel;
    if (GetFileAttributesW(nearExe.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return nearExe; // найден рядом с exe
    }
    return rel; // не найден — вернём как есть, чтобы получить осмысленную ошибку
}

// Формат вершины пресета POS_COLOR (совпадает с входом Default2D.hlsl).
struct VertexPosColor {
    XMFLOAT4 pos;
    XMFLOAT4 col;
};

// Формат вершины пресета POS_NORMAL_COLOR (совпадает с входом phong/VertexShader.hlsl):
// POSITION0 (float4) + NORMAL0 (float3) + COLOR0 (float4).
struct VertexPosNormalColor {
    XMFLOAT4 pos;
    XMFLOAT3 normal;
    XMFLOAT4 col;
};

// Скомпилированный материал-шейдер, кешируемый по пути.
struct CachedShader {
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11RasterizerState> rast;
};

// Пресет layout'а вершины (должен совпадать со входом VS скомпилированного шейдера).
enum class LayoutPreset { POS_COLOR, POS_NORMAL_COLOR };

bool CompileShaderFromFile(const wchar_t* path, const char* entry,
                           const char* target, ComPtr<ID3DBlob>& blob) {
    ComPtr<ID3DBlob> err;
    HRESULT hr = D3DCompileFromFile(
        path, nullptr, nullptr, entry, target,
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0,
        blob.GetAddressOf(), err.GetAddressOf());
    if (FAILED(hr)) {
        std::wcout << L"[ResourceLoader] Shader compile failed (0x"
                   << std::hex << static_cast<unsigned>(hr) << std::dec
                   << L"): " << path << L"\n";
        if (err) std::cout << "  " << (const char*)err->GetBufferPointer() << '\n';
        return false;
    }
    return true;
}

// Компилирует шейдер + создаёт input layout (по пресету) и rasterizer.
bool BuildShader(ID3D11Device* device, const wchar_t* path, LayoutPreset preset, CachedShader& out) {
    std::wstring resolved = ResolveAssetPath(path);

    ComPtr<ID3DBlob> vsBlob, psBlob;
    if (!CompileShaderFromFile(resolved.c_str(), "VSMain", "vs_5_0", vsBlob)) return false;
    if (!CompileShaderFromFile(resolved.c_str(), "PSMain", "ps_5_0", psBlob)) return false;

    device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                               nullptr, out.vs.GetAddressOf());
    device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                              nullptr, out.ps.GetAddressOf());

    if (preset == LayoutPreset::POS_NORMAL_COLOR) {
        // Пресет POS_NORMAL_COLOR: POSITION0 (float4) + NORMAL0 (float3) + COLOR0 (float4).
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        device->CreateInputLayout(layout, 3, vsBlob->GetBufferPointer(),
                                  vsBlob->GetBufferSize(), out.layout.GetAddressOf());
    } else {
        // Пресет POS_COLOR: POSITION0 (float4) + COLOR0 (float4).
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(),
                                  vsBlob->GetBufferSize(), out.layout.GetAddressOf());
    }

    CD3D11_RASTERIZER_DESC rastDesc(D3D11_DEFAULT);
    rastDesc.CullMode = D3D11_CULL_NONE;
    device->CreateRasterizerState(&rastDesc, out.rast.GetAddressOf());

    return true;
}

// Phong-вариант: VS и PS из отдельных файлов (data/shaders/phong/), layout POS_NORMAL_COLOR.
bool BuildPhongShader(ID3D11Device* device, const wchar_t* vsPath, const wchar_t* psPath,
                      CachedShader& out) {
    std::wstring resolvedVS = ResolveAssetPath(vsPath);
    std::wstring resolvedPS = ResolveAssetPath(psPath);

    ComPtr<ID3DBlob> vsBlob, psBlob;
    if (!CompileShaderFromFile(resolvedVS.c_str(), "VSMain", "vs_5_0", vsBlob)) return false;
    if (!CompileShaderFromFile(resolvedPS.c_str(), "PSMain", "ps_5_0", psBlob)) return false;

    device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                               nullptr, out.vs.GetAddressOf());
    device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                              nullptr, out.ps.GetAddressOf());

    // Пресет POS_NORMAL_COLOR: POSITION0 (float4) + NORMAL0 (float3) + COLOR0 (float4).
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    device->CreateInputLayout(layout, 3, vsBlob->GetBufferPointer(),
                              vsBlob->GetBufferSize(), out.layout.GetAddressOf());

    CD3D11_RASTERIZER_DESC rastDesc(D3D11_DEFAULT);
    rastDesc.CullMode = D3D11_CULL_NONE;
    device->CreateRasterizerState(&rastDesc, out.rast.GetAddressOf());

    return true;
}

// Создаёт VB/IB для quad'а (центрированный прямоугольник width x height).
void BuildQuad(ID3D11Device* device, MeshComponent& mesh) {
    const float hw = mesh.quadWidth * 0.5f;
    const float hh = mesh.quadHeight * 0.5f;
    const XMFLOAT4& c = mesh.quadColor;

    VertexPosColor verts[4] = {
        { { -hw, -hh, 0.0f, 1.0f }, c },
        { {  hw, -hh, 0.0f, 1.0f }, c },
        { {  hw,  hh, 0.0f, 1.0f }, c },
        { { -hw,  hh, 0.0f, 1.0f }, c },
    };
    UINT indices[6] = { 0, 1, 2, 2, 3, 0 };

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.ByteWidth = sizeof(verts);
    D3D11_SUBRESOURCE_DATA vbData = { verts, 0, 0 };
    device->CreateBuffer(&vbDesc, &vbData, mesh.vertexBuffer.GetAddressOf());

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.ByteWidth = sizeof(indices);
    D3D11_SUBRESOURCE_DATA ibData = { indices, 0, 0 };
    device->CreateBuffer(&ibDesc, &ibData, mesh.indexBuffer.GetAddressOf());

    mesh.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    mesh.indexCount = 6;
    mesh.vertexStride = sizeof(VertexPosColor);
    mesh.indexFormat = DXGI_FORMAT_R32_UINT;
}

} // namespace

// --- Геометрия с нормалями (пресет POS_NORMAL_COLOR) ------------------------

namespace {

// Создаёт VB/IB для куба (вершины на ±size/2, нормали по граням).
void BuildCube(ID3D11Device* device, MeshComponent& mesh) {
    const float h = mesh.cubeSize * 0.5f;
    const XMFLOAT4& c = mesh.quadColor;

    // 6 граней x 4 вершины; нормаль перпендикулярна грани.
    const XMFLOAT3 normals[6] = {
        {  0.0f,  0.0f, -1.0f }, // -Z
        {  0.0f,  0.0f,  1.0f }, // +Z
        { -1.0f,  0.0f,  0.0f }, // -X
        {  1.0f,  0.0f,  0.0f }, // +X
        {  0.0f, -1.0f,  0.0f }, // -Y
        {  0.0f,  1.0f,  0.0f }, // +Y
    };

    VertexPosNormalColor verts[24];
    UINT indices[36];

    auto face = [&](int f, const XMFLOAT3& a, const XMFLOAT3& b,
                    const XMFLOAT3& cc, const XMFLOAT3& d) {
        const int v = f * 4;
        const XMFLOAT3 n = normals[f];
        verts[v + 0] = { { a.x, a.y, a.z, 1.0f }, n, c };
        verts[v + 1] = { { b.x, b.y, b.z, 1.0f }, n, c };
        verts[v + 2] = { { cc.x, cc.y, cc.z, 1.0f }, n, c };
        verts[v + 3] = { { d.x, d.y, d.z, 1.0f }, n, c };
        const int i = f * 6;
        indices[i + 0] = v + 0; indices[i + 1] = v + 1; indices[i + 2] = v + 2;
        indices[i + 3] = v + 0; indices[i + 4] = v + 2; indices[i + 5] = v + 3;
    };

    face(0, { -h,-h,-h }, { -h, h,-h }, {  h, h,-h }, {  h,-h,-h }); // -Z
    face(1, {  h,-h, h }, {  h, h, h }, { -h, h, h }, { -h,-h, h }); // +Z
    face(2, { -h,-h,-h }, { -h,-h, h }, { -h, h, h }, { -h, h,-h }); // -X
    face(3, {  h,-h, h }, {  h,-h,-h }, {  h, h,-h }, {  h, h, h }); // +X
    face(4, { -h,-h, h }, { -h,-h,-h }, {  h,-h,-h }, {  h,-h, h }); // -Y
    face(5, { -h, h,-h }, { -h, h, h }, {  h, h, h }, {  h, h,-h }); // +Y

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.ByteWidth = sizeof(verts);
    D3D11_SUBRESOURCE_DATA vbData = { verts, 0, 0 };
    device->CreateBuffer(&vbDesc, &vbData, mesh.vertexBuffer.GetAddressOf());

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.ByteWidth = sizeof(indices);
    D3D11_SUBRESOURCE_DATA ibData = { indices, 0, 0 };
    device->CreateBuffer(&ibDesc, &ibData, mesh.indexBuffer.GetAddressOf());

    mesh.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    mesh.indexCount = 36;
    mesh.vertexStride = sizeof(VertexPosNormalColor);
    mesh.indexFormat = DXGI_FORMAT_R32_UINT;
}

// Создаёт VB/IB для UV-сферы (радиус, slices по долготе, stacks по широте).
void BuildSphere(ID3D11Device* device, MeshComponent& mesh) {
    const float r = mesh.sphereRadius;
    const int slices = mesh.sphereSlices < 3 ? 3 : mesh.sphereSlices;
    const int stacks = mesh.sphereStacks < 2 ? 2 : mesh.sphereStacks;
    const XMFLOAT4& c = mesh.quadColor;

    const int ringCount = stacks - 1;                    // внутренних колец
    const int vertexCount = 2 + ringCount * slices;      // полюса + кольца
    std::vector<VertexPosNormalColor> verts(vertexCount);
    std::vector<UINT> indices;
    indices.reserve(static_cast<size_t>(slices) * (stacks - 2) * 6 + static_cast<size_t>(slices) * 6);

    const float phi = XM_PI / static_cast<float>(stacks);   // шаг по широте
    const float theta = XM_2PI / static_cast<float>(slices); // шаг по долготе

    // Полюса
    verts[0] = { { 0.0f,  r, 0.0f, 1.0f }, { 0.0f,  1.0f, 0.0f }, c };
    verts[1] = { { 0.0f, -r, 0.0f, 1.0f }, { 0.0f, -1.0f, 0.0f }, c };

    // Внутренние кольца
    int v = 2;
    for (int i = 1; i <= ringCount; ++i) {
        const float py = r * std::cos(phi * i);
        const float pr = r * std::sin(phi * i);
        for (int j = 0; j < slices; ++j) {
            const float px = pr * std::sin(theta * j);
            const float pz = pr * std::cos(theta * j);
            // Нормаль сферы = нормализованная позиция вершины.
            const float len = std::sqrt(px * px + py * py + pz * pz);
            const XMFLOAT3 n = { px / len, py / len, pz / len };
            verts[v++] = { { px, py, pz, 1.0f }, n, c };
        }
    }

    // Верхняя шапка (полюс + первое кольцо)
    for (int j = 0; j < slices; ++j) {
        const int j1 = (j + 1) % slices;
        indices.push_back(0);
        indices.push_back(2 + j1);
        indices.push_back(2 + j);
    }

    // Средняя часть (четырёхугольники между кольцами)
    for (int i = 0; i < ringCount - 1; ++i) {
        const int row0 = 2 + i * slices;
        const int row1 = row0 + slices;
        for (int j = 0; j < slices; ++j) {
            const int j1 = (j + 1) % slices;
            indices.push_back(row0 + j);  indices.push_back(row1 + j1); indices.push_back(row0 + j1);
            indices.push_back(row0 + j);  indices.push_back(row1 + j);  indices.push_back(row1 + j1);
        }
    }

    // Нижняя шапка (последнее кольцо + полюс)
    const int lastRow = 2 + (ringCount - 1) * slices;
    for (int j = 0; j < slices; ++j) {
        const int j1 = (j + 1) % slices;
        indices.push_back(1);
        indices.push_back(lastRow + j);
        indices.push_back(lastRow + j1);
    }

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.ByteWidth = static_cast<UINT>(sizeof(VertexPosNormalColor) * verts.size());
    D3D11_SUBRESOURCE_DATA vbData = { verts.data(), 0, 0 };
    device->CreateBuffer(&vbDesc, &vbData, mesh.vertexBuffer.GetAddressOf());

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.ByteWidth = static_cast<UINT>(sizeof(UINT) * indices.size());
    D3D11_SUBRESOURCE_DATA ibData = { indices.data(), 0, 0 };
    device->CreateBuffer(&ibDesc, &ibData, mesh.indexBuffer.GetAddressOf());

    mesh.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    mesh.indexCount = static_cast<UINT>(indices.size());
    mesh.vertexStride = sizeof(VertexPosNormalColor);
    mesh.indexFormat = DXGI_FORMAT_R32_UINT;
}

// Создаёт VB/IB для плоскости width x depth на XZ (Y = 0), нормали вверх.
void BuildPlane(ID3D11Device* device, MeshComponent& mesh) {
    const int segX = mesh.planeSegmentsX < 1 ? 1 : mesh.planeSegmentsX;
    const int segZ = mesh.planeSegmentsZ < 1 ? 1 : mesh.planeSegmentsZ;
    const float hw = mesh.planeWidth * 0.5f;
    const float hd = mesh.planeDepth * 0.5f;
    const XMFLOAT4& c = mesh.quadColor;

    const int cols = segX + 1;
    const int rows = segZ + 1;
    std::vector<VertexPosNormalColor> verts(static_cast<size_t>(cols) * rows);
    std::vector<UINT> indices(static_cast<size_t>(segX) * segZ * 6);

    const XMFLOAT3 up = { 0.0f, 1.0f, 0.0f };
    int v = 0;
    for (int z = 0; z < rows; ++z) {
        const float pz = -hd + mesh.planeDepth * static_cast<float>(z) / segZ;
        for (int x = 0; x < cols; ++x) {
            const float px = -hw + mesh.planeWidth * static_cast<float>(x) / segX;
            verts[v++] = { { px, 0.0f, pz, 1.0f }, up, c };
        }
    }

    int i = 0;
    for (int z = 0; z < segZ; ++z) {
        for (int x = 0; x < segX; ++x) {
            const int r0 = z * cols + x;
            const int r1 = (z + 1) * cols + x;
            indices[i++] = r0;     indices[i++] = r0 + 1; indices[i++] = r1 + 1;
            indices[i++] = r0;     indices[i++] = r1 + 1; indices[i++] = r1;
        }
    }

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.ByteWidth = static_cast<UINT>(sizeof(VertexPosNormalColor) * verts.size());
    D3D11_SUBRESOURCE_DATA vbData = { verts.data(), 0, 0 };
    device->CreateBuffer(&vbDesc, &vbData, mesh.vertexBuffer.GetAddressOf());

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.ByteWidth = static_cast<UINT>(sizeof(UINT) * indices.size());
    D3D11_SUBRESOURCE_DATA ibData = { indices.data(), 0, 0 };
    device->CreateBuffer(&ibDesc, &ibData, mesh.indexBuffer.GetAddressOf());

    mesh.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    mesh.indexCount = static_cast<UINT>(indices.size());
    mesh.vertexStride = sizeof(VertexPosNormalColor);
    mesh.indexFormat = DXGI_FORMAT_R32_UINT;
}

// Создаёт VB/IB по описанию примитива в MeshComponent.
void UploadMesh(ID3D11Device* device, MeshComponent& mesh) {
    switch (mesh.primitive) {
        case MeshComponent::Primitive::QUAD:   BuildQuad(device, mesh);   break;
        case MeshComponent::Primitive::CUBE:   BuildCube(device, mesh);   break;
        case MeshComponent::Primitive::SPHERE: BuildSphere(device, mesh); break;
        case MeshComponent::Primitive::PLANE:  BuildPlane(device, mesh);  break;
        case MeshComponent::Primitive::NONE:   break;
    }
    if (mesh.primitive != MeshComponent::Primitive::NONE) {
        mesh.uploaded = true;
    }
}

} // namespace

void ResourceLoader::UploadAll(World& world, ID3D11Device* device) {
    if (!device) return;

    // --- Меши: создаём геометрию из описания --------------------------------
    Query<MeshComponent> meshQ(world);
    meshQ.ForEach([&](Entity, MeshComponent& mesh) {
        if (mesh.uploaded) return;
        UploadMesh(device, mesh);
    });

    // --- Материалы: компилируем шейдеры (с кешем по пути) --------------------
    std::unordered_map<std::wstring, CachedShader> cache;

    Query<MaterialComponent> matQ(world);
    matQ.ForEach([&](Entity, MaterialComponent& mat) {
        if (mat.uploaded) return;

        std::wstring path = mat.shaderPath.empty() ? kDefaultShaderPath : mat.shaderPath;

        auto it = cache.find(path);
        if (it == cache.end()) {
            CachedShader cs;
            if (!BuildShader(device, path.c_str(), LayoutPreset::POS_COLOR, cs)) {
                return; // не помечаем uploaded, чтобы ошибка была заметна
            }
            it = cache.emplace(path, std::move(cs)).first;
        }

        const CachedShader& cs = it->second;
        mat.vertexShader = cs.vs;
        mat.pixelShader = cs.ps;
        mat.inputLayout = cs.layout;
        mat.rasterizerState = cs.rast;
        mat.uploaded = true;
    });

    // --- Phong-материалы: POS_NORMAL_COLOR, шейдеры data/shaders/phong/ ------
    Query<PhongMaterialComponent> phongMatQ(world);
    phongMatQ.ForEach([&](Entity, PhongMaterialComponent& mat) {
        if (mat.uploaded) return;

        // Пусто => дефолтные шейдеры Фонга фреймворка (VS + PS в одной папке).
        std::wstring vsPath = mat.shaderPath.empty() ? kDefaultPhongVSPath : mat.shaderPath;
        std::wstring psPath = mat.shaderPath.empty() ? kDefaultPhongPSPath : mat.shaderPath;

        // Кеш по паре путей (VS и PS лежат рядом, ключ — путь VS).
        auto it = cache.find(vsPath);
        if (it == cache.end()) {
            CachedShader cs;
            if (!BuildPhongShader(device, vsPath.c_str(), psPath.c_str(), cs)) {
                return; // не помечаем uploaded, чтобы ошибка была заметна
            }
            it = cache.emplace(vsPath, std::move(cs)).first;
        }

        const CachedShader& cs = it->second;
        mat.vertexShader = cs.vs;
        mat.pixelShader = cs.ps;
        mat.inputLayout = cs.layout;
        mat.rasterizerState = cs.rast;
        mat.uploaded = true;
    });
}
