#include "framework/modules/render/ObjLoader.h"

#include <windows.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <algorithm>
#include <cstdlib>
#include <cmath>

// -----------------------------------------------------------------------------
// ObjLoader — реализация. См. ObjLoader.h.
// -----------------------------------------------------------------------------

namespace {

using DirectX::XMFLOAT2;
using DirectX::XMFLOAT3;
using DirectX::XMFLOAT4;
using DirectX::XMVECTOR;
using DirectX::XMVectorSet;

// Хеш-ключ вершины OBJ: тройка индексов (v, vt, vn), 1-based, 0 = отсутствует.
struct ObjVertexKey {
    int v, t, n;
    bool operator==(const ObjVertexKey& o) const { return v == o.v && t == o.t && n == o.n; }
};
struct ObjVertexKeyHash {
    size_t operator()(const ObjVertexKey& k) const {
        // v/vt/vn лежат в диапазоне до ~миллионов — 21 бит на компонент хватает.
        size_t h = static_cast<size_t>(static_cast<uint32_t>(k.v)) * 73856093u;
        h ^= static_cast<size_t>(static_cast<uint32_t>(k.t)) * 19349663u;
        h ^= static_cast<size_t>(static_cast<uint32_t>(k.n)) * 83492791u;
        return h;
    }
};

inline float SafeParseFloat(const char* s, float fallback = 0.0f) {
    if (!s) return fallback;
    char* end = nullptr;
    const float val = std::strtof(s, &end);
    return (end == s) ? fallback : val;
}

inline int SafeParseInt(const char* s) {
    if (!s) return 0;
    char* end = nullptr;
    const long val = std::strtol(s, &end, 10);
    return (end == s) ? 0 : static_cast<int>(val);
}

// Нормаль треугольника (area-weighted, CCW).
XMFLOAT3 FaceNormal(const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& c) {
    const float ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
    const float vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
    XMFLOAT3 n = { uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx };
    const float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
    if (len > 1e-10f) {
        n.x /= len; n.y /= len; n.z /= len;
    } else {
        n = { 0.0f, 1.0f, 0.0f }; // вырожденный треугольник — нормаль вверх
    }
    return n;
}

} // namespace

bool ObjLoader::Load(const std::wstring& path, XMFLOAT4 color, ObjMeshData& out) {
    // std::ifstream не принимает wstring на mingw — конвертируем в узкую строку
    // (пути моделей ASCII, кириллица в именах моделей не ожидается).
    std::string narrow(path.begin(), path.end());
    std::ifstream file(narrow);
    if (!file.is_open()) {
        std::cout << "[ObjLoader] Failed to open: " << narrow << '\n';
        return false;
    }

    std::vector<XMFLOAT3> positions;
    std::vector<XMFLOAT3> normals;
    std::vector<ObjVertexKey> uniqueKeys;
    std::unordered_map<ObjVertexKey, uint32_t, ObjVertexKeyHash> uniqueMap;

    // Нормали граней для усреднения (вершина может не иметь vn в файле).
    std::vector<XMVECTOR> faceNormals;
    // Для каждой выходной вершины — список (нормаль грани, вес) для усреднения.
    // Индекс согласован с uniqueKeys.
    std::vector<std::vector<size_t>> vertexFaces;

    std::string line;
    while (std::getline(file, line)) {
        // Убираем \r (Windows-концы строк).
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;

        std::istringstream ss(line);
        std::string tag;
        ss >> tag;

        if (tag == "v") {
            float x = 0, y = 0, z = 0;
            ss >> x >> y >> z;
            positions.push_back({ x, y, z });
        } else if (tag == "vn") {
            float x = 0, y = 0, z = 0;
            ss >> x >> y >> z;
            normals.push_back({ x, y, z });
        } else if (tag == "f") {
            // Собираем вершины грани: "v", "v/vt", "v//vn", "v/vt/vn".
            std::vector<uint32_t> faceVerts;
            faceVerts.reserve(8);
            std::string corner;
            while (ss >> corner) {
                // Разбор corner: split по '/'.
                int vIdx = 0, tIdx = 0, nIdx = 0;
                {
                    size_t p1 = corner.find('/');
                    if (p1 == std::string::npos) {
                        vIdx = SafeParseInt(corner.c_str());
                    } else {
                        vIdx = SafeParseInt(corner.substr(0, p1).c_str());
                        size_t p2 = corner.find('/', p1 + 1);
                        if (p2 == std::string::npos) {
                            tIdx = SafeParseInt(corner.substr(p1 + 1).c_str());
                        } else {
                            // Между p1 и p2 может быть пусто (v//vn) или vt.
                            const std::string mid = corner.substr(p1 + 1, p2 - p1 - 1);
                            if (!mid.empty()) tIdx = SafeParseInt(mid.c_str());
                            nIdx = SafeParseInt(corner.substr(p2 + 1).c_str());
                        }
                    }
                }
                // Отрицательные индексы — относительные (с конца списка).
                auto resolve = [](int idx, size_t count) -> int {
                    if (idx > 0) return idx - 1;
                    if (idx < 0) return static_cast<int>(count) + idx;
                    return -1;
                };
                const int v = resolve(vIdx, positions.size());
                const int n = resolve(nIdx, normals.size());
                if (v < 0) continue; // битая вершина — пропускаем

                ObjVertexKey key{ v, 0, n };
                auto it = uniqueMap.find(key);
                uint32_t outIdx;
                if (it != uniqueMap.end()) {
                    outIdx = it->second;
                } else {
                    outIdx = static_cast<uint32_t>(uniqueKeys.size());
                    uniqueMap.emplace(key, outIdx);
                    uniqueKeys.push_back(key);
                }
                faceVerts.push_back(outIdx);
            }

            if (faceVerts.size() < 3) continue; // вырожденная грань

            // Позиции вершин грани — для нормали и триангуляции фаном.
            std::vector<XMFLOAT3> fp(faceVerts.size());
            for (size_t i = 0; i < faceVerts.size(); ++i) {
                fp[i] = positions[uniqueKeys[faceVerts[i]].v];
            }
            const size_t faceId = faceNormals.size();
            const XMFLOAT3 fn = FaceNormal(fp[0], fp[1], fp[2]);
            faceNormals.push_back(DirectX::XMLoadFloat3(&fn));

            for (uint32_t fv : faceVerts) {
                vertexFaces.resize(uniqueKeys.size());
                vertexFaces[fv].push_back(faceId);
            }

            // Триангуляция фаном: (0, i, i+1).
            for (size_t i = 1; i + 1 < faceVerts.size(); ++i) {
                out.indices.push_back(faceVerts[0]);
                out.indices.push_back(faceVerts[i]);
                out.indices.push_back(faceVerts[i + 1]);
            }
        }
        // Остальные теги (vt, mtllib, usemtl, g, s, o) игнорируем.
    }

    if (out.indices.empty()) {
        std::cout << "[ObjLoader] No faces in: " << narrow << '\n';
        return false;
    }

    // --- Сборка вершин: позиция + нормаль (из vn или усреднённая) ------------
    out.vertices.clear();
    out.vertices.reserve(uniqueKeys.size());
    for (size_t i = 0; i < uniqueKeys.size(); ++i) {
        const ObjVertexKey& key = uniqueKeys[i];
        XMFLOAT3 n = { 0.0f, 0.0f, 0.0f };
        if (key.n >= 0) {
            n = normals[key.n];
        } else {
            // Усредняем нормали граней, к которым принадлежит вершина.
            XMVECTOR acc = DirectX::XMVectorZero();
            for (size_t f : vertexFaces[i]) acc = DirectX::XMVectorAdd(acc, faceNormals[f]);
            DirectX::XMStoreFloat3(&n, DirectX::XMVector3Normalize(acc));
        }
        const XMFLOAT3& p = positions[key.v];
        out.vertices.push_back({ { p.x, p.y, p.z, 1.0f }, n, color });
    }

    // --- Bounding sphere: центр bbox, радиус = max расстояние до вершины ------
    XMFLOAT3 mn = { 1e30f, 1e30f, 1e30f }, mx = { -1e30f, -1e30f, -1e30f };
    for (const auto& p : positions) {
        mn.x = std::min(mn.x, p.x); mn.y = std::min(mn.y, p.y); mn.z = std::min(mn.z, p.z);
        mx.x = std::max(mx.x, p.x); mx.y = std::max(mx.y, p.y); mx.z = std::max(mx.z, p.z);
    }
    out.center = { (mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, (mn.z + mx.z) * 0.5f };
    out.radius = 0.0f;
    for (const auto& p : positions) {
        const float dx = p.x - out.center.x;
        const float dy = p.y - out.center.y;
        const float dz = p.z - out.center.z;
        out.radius = std::max(out.radius, std::sqrt(dx * dx + dy * dy + dz * dz));
    }

    std::cout << "[ObjLoader] Loaded " << narrow << ": "
              << out.vertices.size() << " verts, "
              << out.indices.size() / 3 << " tris, radius "
              << out.radius << '\n';
    return true;
}
