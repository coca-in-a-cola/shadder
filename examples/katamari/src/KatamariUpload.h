#pragma once

// KatamariUploadSystem — одноразовая заливка KatamariCustomMesh (OBJ-геометрия
// пикапов) в GPU-буферы + привязка к Phong-шейдерам фреймворка.
//
// Почему своя загрузка: ResourceLoader::UploadAll умеет только примитивы
// (QUAD/CUBE/SPHERE/PLANE). ObjMeshData уже в пресете POS_NORMAL_COLOR
// (та же раскладка вершин, что у phong-шейдеров фреймворка), поэтому просто
// создаём VB/IB и проставляем шейдеры как у остальных Phong-материалов.
//
// Вызывается один раз после ResourceLoader::UploadAll и до game.Run()
// (паттерн как у ResourceLoader — см. SH-P).

#include "core/ecs/System.h"
#include <d3d11.h>

class Game;

class KatamariUploadSystem : public ISystem {
public:
    KatamariUploadSystem(Game* game, ID3D11Device* device)
        : game_(game), device_(device) {}
    void OnUpdate(World& world, float deltaTime) override;

private:
    Game* game_;
    ID3D11Device* device_;
};
