#pragma once

// -----------------------------------------------------------------------------
// KatamariComponents — компоненты примера SH-S2 (Katamari Damacy).
//
// Godot-подход: данные — в компонентах, поведение — в системах.
//   KatamariBallComponent  — катящийся шар игрока (радиус, скорость, рост);
//   KatamariPickupComponent— объект на поле (радиус из геометрии, подобран?);
//   KatamariStatsComponent — глобальное состояние игры (цель, прогресс);
// Позиция/ориентация — в Transform3D (framework/modules/transform).
//
// Bounding sphere (ОБЯЗАТЕЛЬНО по спеке SH-S2 п.5): у каждого объекта сфера
// из геометрии — KatamariPickupComponent::radius заполняется из
// ObjMeshData::radius при создании, у шара — KatamariBallComponent::radius.
// Проверка подбора = пересечение сфер.
// -----------------------------------------------------------------------------

#include "shadder.hpp"

#include "core/ecs/Component.h"
#include "framework/modules/render/ObjLoader.h"
#include <DirectXMath.h>
#include <vector>

// Катящийся шар (игрок).
struct KatamariBallComponent final : public shadder::ComponentBase {
    float radius = 0.5f;        // текущий радиус шара (мировые единицы)
    float moveSpeed = 6.0f;     // линейная скорость (растёт с размером)
    float growPerPickup = 0.9f; // прирост радиуса на объект (масштабируется от размера)
    DirectX::XMFLOAT2 moveInput = { 0.0f, 0.0f }; // (-1..1) x=вправо, y=вперёд
};

// Нестандартная геометрия пикапа: OBJ/fallback-данные в пресете POS_NORMAL_COLOR.
// ResourceLoader умеет только примитивы, поэтому эти буферы заливает
// KatamariUploadSystem (см. KatamariUpload.h) один раз перед игровым циклом.
struct KatamariCustomMesh final : public shadder::ComponentBase {
    std::vector<ObjMeshData::Vertex> vertices;
    std::vector<uint32_t> indices;
    bool uploaded = false;
};

// Объект, который шар может подобрать.
struct KatamariPickupComponent final : public shadder::ComponentBase {
    float radius = 0.3f;    // радиус bounding sphere из геометрии * scale
    bool pickedUp = false;  // подобран?
    float stuckAngle = 0.0f;// угол на поверхности шара (куда прилип)
    float stuckHeight = 0.0f; // высота на поверхности шара
};

// Глобальное состояние игры.
struct KatamariStatsComponent final : public shadder::ComponentBase {
    shadder::Entity ballEntity{}; // ссылка на шар
    int collected = 0;            // сколько подобрано
    int total = 0;                // всего объектов на поле
    bool won = false;             // цель: собрать всё (или почти всё)
};
