#pragma once

// -----------------------------------------------------------------------------
// ObjLoader — минимальный загрузчик Wavefront OBJ (framework/modules/render).
//
// Поддержка: v (позиции), vn (нормали), f (треугольные/многоугольные грани —
// триангулируются фан-методом), индексы вида v, v//vn, v/vt/vn, v/vt.
// НЕ поддерживается (и не нужно для примеров): vt-данные, материалы (mtl),
// группы/сглаживание (g/s/o читаются и игнорируются).
//
// Результат — «сырые» данные геометрии в пресете POS_NORMAL_COLOR фреймворка
// (позиция float4 + нормаль float3 + цвет float4). Цвет общий на модель:
// OBJ не несёт цветов, окраску задаёт пользователь ObjLoader::Load(..., color).
// Нормали: если у вершины нет vn — усредняем нормали граней (smooth shading).
//
// Бounding sphere считается из геометрии (радиус = max расстояние вершин от
// центра bbox), т.к. требование SH-S2: подбор по пересечению сфер из геометрии.
// -----------------------------------------------------------------------------

#include <DirectXMath.h>
#include <cstdint>
#include <string>
#include <vector>

struct ObjMeshData {
    // Пресет POS_NORMAL_COLOR (совпадает с VertexPosNormalColor в ResourceLoader):
    // POSITION0 (float4) + NORMAL0 (float3) + COLOR0 (float4).
    struct Vertex {
        DirectX::XMFLOAT4 pos;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT4 color;
    };

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Bounding sphere из геометрии (в локальных координатах модели).
    DirectX::XMFLOAT3 center = { 0.0f, 0.0f, 0.0f };
    float radius = 0.0f;

    // Пользовательский цвет (вшивается во все вершины при загрузке).
    void SetColor(const DirectX::XMFLOAT4& c) {
        for (auto& v : vertices) v.color = c;
    }
};

class ObjLoader {
public:
    // Загружает .obj по пути (файл ищется как есть). При ошибке возвращает false
    // и оставляет out нетронутым; причина печатается в stdout.
    static bool Load(const std::wstring& path, DirectX::XMFLOAT4 color, ObjMeshData& out);
};
