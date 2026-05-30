#include "framework/modules/render/ResourceLoader.h"
#include "framework/modules/render/MeshComponent.h"
#include "framework/modules/render/MaterialComponent.h"
#include "ecs/Query.h"
#include <windows.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <wrl.h>
#include <iostream>
#include <string>
#include <unordered_map>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace {

// Путь к дефолтному шейдеру фреймворка (используется, если shaderPath пуст).
const wchar_t* kDefaultShaderPath = L"data/Default2D.hlsl";

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

// Скомпилированный материал-шейдер, кешируемый по пути.
struct CachedShader {
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11RasterizerState> rast;
};

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

// Компилирует шейдер + создаёт input layout (пресет POS_COLOR) и rasterizer.
bool BuildShader(ID3D11Device* device, const wchar_t* path, CachedShader& out) {
    std::wstring resolved = ResolveAssetPath(path);

    ComPtr<ID3DBlob> vsBlob, psBlob;
    if (!CompileShaderFromFile(resolved.c_str(), "VSMain", "vs_5_0", vsBlob)) return false;
    if (!CompileShaderFromFile(resolved.c_str(), "PSMain", "ps_5_0", psBlob)) return false;

    device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                               nullptr, out.vs.GetAddressOf());
    device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                              nullptr, out.ps.GetAddressOf());

    // Пресет POS_COLOR: POSITION0 (float4) + COLOR0 (float4).
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(),
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

void ResourceLoader::UploadAll(World& world, ID3D11Device* device) {
    if (!device) return;

    // --- Меши: создаём геометрию из описания --------------------------------
    Query<MeshComponent> meshQ(world);
    meshQ.ForEach([&](Entity, MeshComponent& mesh) {
        if (mesh.uploaded) return;
        if (mesh.primitive == MeshComponent::Primitive::QUAD) {
            BuildQuad(device, mesh);
            mesh.uploaded = true;
        }
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
            if (!BuildShader(device, path.c_str(), cs)) {
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
}
