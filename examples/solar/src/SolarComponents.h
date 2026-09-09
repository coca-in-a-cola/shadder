#pragma once

// -----------------------------------------------------------------------------
// Solar demo components (SH-S1).
//
// Godot-подход: данные — в компонентах, поведение — в системах.
//   SolarOrbit   — орбита вокруг родителя (радиус, скорость, начальная фаза);
//   SolarSpin    — вращение вокруг собственной оси (ось, скорость);
//   SolarSettings— глобальное состояние сцены (активный пресет проекции).
// Позиция хранится в Transform3D; системы читают/пишут только её.
// -----------------------------------------------------------------------------

#include "shadder.hpp"

#include <DirectXMath.h>

// Орбитальное движение: entity ходит по окружности радиуса radius вокруг
// своего «родителя» (позиция берётся из parentEntity, см. SolarMotionSystem).
// Тригонометрия: x = radius * cos(angle), z = -radius * sin(angle)
// (LH-система фреймворка: Z вперёд, Y вверх).
struct SolarOrbit final : public shadder::ComponentBase {
    shadder::Entity parentEntity{};      // вокруг кого ходим (INVALID = центр мира)
    float radius = 10.0f;                // радиус орбиты (мировые единицы)
    float speed = 1.0f;                  // рад/сек (знак = направление)
    float angle = 0.0f;                  // текущая фаза (рад)
    float height = 0.0f;                 // наклон плоскости орбиты: смещение по Y
};

// Вращение вокруг собственной оси (кватернионом в Transform3D.rotation).
struct SolarSpin final : public shadder::ComponentBase {
    DirectX::XMFLOAT3 axis = { 0.0f, 1.0f, 0.0f }; // локальная ось вращения
    float speed = 1.0f;                  // рад/сек
    float angle = 0.0f;                  // накопленный угол (рад)
};

// Глобальные настройки демо (одна сущность, «сцена»).
// Пресеты проекции — требование SH-S1 п.5: 45° FOV / 90° FOV / ортогональная.
struct SolarSettings final : public shadder::ComponentBase {
    enum class ProjectionPreset { FOV45, FOV90, ORTHO };
    ProjectionPreset projection = ProjectionPreset::FOV45;
    shadder::Entity cameraEntity{};      // сущность с CameraComponent
    bool cameraFPS = true;               // активный режим камеры (для вывода в stdout)
};
