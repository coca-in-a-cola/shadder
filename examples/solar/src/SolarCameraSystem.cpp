// SolarCameraSystem — реализация. См. SolarCameraSystem.h.

#include "SolarCameraSystem.h"

#include "shadder.hpp"
#include "framework/game/Game.h"
#include "core/ecs/Query.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <iostream>

using namespace DirectX;

namespace {
    constexpr float kPitchLimit = 1.5f;       // не даём камере перевернуться
    constexpr float kFpsSpeed = 22.0f;        // мировых единиц/сек
    constexpr float kFpsBoost = 3.0f;         // Shift
    constexpr float kMouseSens = 0.0022f;
    constexpr float kOrbitSens = 0.0045f;
    constexpr float kZoomStep = 6.0f;         // колесо: единиц за щелчок
    constexpr float kZoomKey = 30.0f;         // R/колесо: плавный зум
    constexpr float kPanSpeed = 18.0f;

    const char* PresetName(SolarSettings::ProjectionPreset p) {
        switch (p) {
            case SolarSettings::ProjectionPreset::FOV45: return "PERSPECTIVE FOV 45deg";
            case SolarSettings::ProjectionPreset::FOV90: return "PERSPECTIVE FOV 90deg";
            case SolarSettings::ProjectionPreset::ORTHO: return "ORTHOGRAPHIC (world units)";
        }
        return "?";
    }
} // namespace

void SolarCameraSystem::ApplyPreset(World& world, SolarSettings& settings,
                                    SolarSettings::ProjectionPreset preset) {
    settings.projection = preset;
    Query<CameraComponent> q(world);
    q.ForEach([&](Entity, CameraComponent& c) {
        if (!c.active) return;
        switch (preset) {
            case SolarSettings::ProjectionPreset::FOV45:
                c.projection = CameraComponent::Projection::PERSPECTIVE;
                c.fovY = XM_PIDIV4;                    // 45°
                c.nearZ = 0.1f;
                c.farZ = 500.0f;
                break;
            case SolarSettings::ProjectionPreset::FOV90:
                c.projection = CameraComponent::Projection::PERSPECTIVE;
                c.fovY = XM_PIDIV2;                    // 90°
                c.nearZ = 0.1f;
                c.farZ = 500.0f;
                break;
            case SolarSettings::ProjectionPreset::ORTHO:
                c.projection = CameraComponent::Projection::ORTHO_WORLD;
                c.orthoHeight = 80.0f;                 // окно ~80 мировых единиц по высоте
                c.nearZ = -300.0f;                     // орто: near может быть отрицательным
                c.farZ = 300.0f;
                break;
        }
    });
    std::cout << "[Solar] Projection: " << PresetName(preset) << '\n';
}

void SolarCameraSystem::UpdateFPS(World& world, CameraComponent& cam, float dt) {
    auto* game = game_;
    auto* input = game ? game->GetInputDevice() : nullptr;
    if (!input) return;

    // Взгляд мышью (без захвата курсора — как в phong-примере, движение при ЛКМ
    // не требуется: FPS-камера смотрит мышью свободно).
    fpsYaw_ += mouseDX_ * kMouseSens;
    fpsPitch_ = std::max(-kPitchLimit, std::min(kPitchLimit, fpsPitch_ - mouseDY_ * kMouseSens));

    // Ориентация: LH, Y вверх. Forward = (sin(yaw)*cos(pitch), sin(pitch), cos(yaw)*cos(pitch)).
    const float cp = std::cos(fpsPitch_);
    XMFLOAT3 forward = { std::sin(fpsYaw_) * cp, std::sin(fpsPitch_), std::cos(fpsYaw_) * cp };
    XMFLOAT3 right = { std::cos(fpsYaw_), 0.0f, -std::sin(fpsYaw_) };

    XMFLOAT3 move = { 0.0f, 0.0f, 0.0f };
    auto addMove = [&move](const XMFLOAT3& v, float s) {
        move.x += v.x * s; move.y += v.y * s; move.z += v.z * s;
    };
    if (input->IsKeyDown(Keys::W)) addMove(forward, 1.0f);
    if (input->IsKeyDown(Keys::S)) addMove(forward, -1.0f);
    if (input->IsKeyDown(Keys::D)) addMove(right, 1.0f);
    if (input->IsKeyDown(Keys::A)) addMove(right, -1.0f);
    // Q/E — вниз/вверх (как вверх-вниз в Godot fly-режиме).
    if (input->IsKeyDown(Keys::E)) { move.y += 1.0f; }
    if (input->IsKeyDown(Keys::Q)) { move.y -= 1.0f; }

    const float lenSq = move.x * move.x + move.y * move.y + move.z * move.z;
    if (lenSq > 0.0f) {
        const float len = std::sqrt(lenSq);
        float speed = kFpsSpeed;
        if (input->IsKeyDown(Keys::LeftShift) || input->IsKeyDown(Keys::RightShift)) {
            speed *= kFpsBoost;
        }
        fpsPos_.x += move.x / len * speed * dt;
        fpsPos_.y += move.y / len * speed * dt;
        fpsPos_.z += move.z / len * speed * dt;
        // Не даём улететь далеко/под пол.
        fpsPos_.y = std::max(-50.0f, std::min(120.0f, fpsPos_.y));
    }

    cam.eye = fpsPos_;
    cam.target = { fpsPos_.x + forward.x, fpsPos_.y + forward.y, fpsPos_.z + forward.z };
    cam.up = { 0.0f, 1.0f, 0.0f };
}

void SolarCameraSystem::UpdateOrbit(World& world, CameraComponent& cam, float dt) {
    auto* input = game_ ? game_->GetInputDevice() : nullptr;
    if (!input) return;

    // RMB — взгляд (вращение вокруг цели), как в редакторе Godot.
    if (input->IsKeyDown(Keys::RightButton)) {
        orbitYaw_ += mouseDX_ * kOrbitSens;
        orbitPitch_ = std::max(-kPitchLimit, std::min(kPitchLimit, orbitPitch_ + mouseDY_ * kOrbitSens));
    }

    // Колесо — зум (приближение/отдаление к цели).
    orbitDistance_ = std::max(8.0f, std::min(260.0f,
        orbitDistance_ - static_cast<float>(wheelAccum_ * 0.01f) * kZoomStep));
    // W/S — плавный зум с клавиатуры (пока камера орбитальная).
    if (input->IsKeyDown(Keys::W)) orbitDistance_ = std::max(8.0f, orbitDistance_ - kZoomKey * dt);
    if (input->IsKeyDown(Keys::S)) orbitDistance_ = std::min(260.0f, orbitDistance_ + kZoomKey * dt);

    // MMB — панорама (сдвиг цели в плоскости камеры).
    if (input->IsKeyDown(Keys::MiddleButton)) {
        const XMFLOAT3 right = { std::cos(orbitYaw_), 0.0f, -std::sin(orbitYaw_) };
        // "Вверх" в плоскости экрана: пересечение плоскости, перпендикулярной взгляду.
        const float cp = std::cos(orbitPitch_);
        const float sp = std::sin(orbitPitch_);
        const float sy = std::sin(orbitYaw_);
        const float cy = std::cos(orbitYaw_);
        const XMFLOAT3 up = { -sp * sy, cp, -sp * cy };

        const float pan = kPanSpeed * dt * (orbitDistance_ / 95.0f);
        if (mouseDX_ != 0.0f) {
            orbitTarget_.x += right.x * mouseDX_ * pan * 0.05f;
            orbitTarget_.z += right.z * mouseDX_ * pan * 0.05f;
        }
        if (mouseDY_ != 0.0f) {
            orbitTarget_.x += up.x * mouseDY_ * pan * 0.05f;
            orbitTarget_.y += up.y * mouseDY_ * pan * 0.05f;
            orbitTarget_.z += up.z * mouseDY_ * pan * 0.05f;
        }
    }

    // Позиция на орбите (LH: Z вперёд — камера стоит на -Z при yaw=0).
    const float cp = std::cos(orbitPitch_);
    const XMFLOAT3 eye = {
        orbitTarget_.x + orbitDistance_ * cp * std::sin(orbitYaw_),
        orbitTarget_.y + orbitDistance_ * std::sin(orbitPitch_),
        orbitTarget_.z - orbitDistance_ * cp * std::cos(orbitYaw_),
    };

    cam.eye = eye;
    cam.target = orbitTarget_;
    cam.up = { 0.0f, 1.0f, 0.0f };
}

void SolarCameraSystem::OnUpdate(World& world, float deltaTime) {
    auto* input = game_ ? game_->GetInputDevice() : nullptr;
    if (!input) return;

    // --- Копим дельты мыши за кадр (события Raw Input приходят между кадрами).
    mouseDX_ += static_cast<float>(input->MouseOffset.x);
    mouseDY_ += static_cast<float>(input->MouseOffset.y);
    wheelAccum_ += input->MouseWheelDelta;
    input->MouseOffset.x = 0.0f;
    input->MouseOffset.y = 0.0f;
    input->MouseWheelDelta = 0;

    // --- Переключение режима камеры: TAB (по нажатию) -----------------------
    const bool tabNow = input->IsKeyDown(Keys::Tab);
    if (tabNow && !tabDown_) {
        mode_ = (mode_ == Mode::FPS) ? Mode::ORBIT : Mode::FPS;
        std::cout << "[Solar] Camera: " << (mode_ == Mode::FPS ? "FPS (WASD+mouse, Q/E down/up, Shift boost)"
                                                              : "ORBIT (RMB look, wheel zoom, MMB pan)")
                  << '\n';
    }
    tabDown_ = tabNow;

    // --- Настройки сцены: пресет проекции -----------------------------------
    SolarSettings* settings = nullptr;
    Query<SolarSettings> setQ(world);
    setQ.ForEach([&](Entity, SolarSettings& s) { settings = &s; });
    if (!settings) return;

    const auto edge = [input](Keys k, bool& wasDown) {
        const bool down = input->IsKeyDown(k);
        const bool pressed = down && !wasDown;
        wasDown = down;
        return pressed;
    };

    if (edge(Keys::D1, k1Down_)) ApplyPreset(world, *settings, SolarSettings::ProjectionPreset::FOV45);
    if (edge(Keys::D2, k2Down_)) ApplyPreset(world, *settings, SolarSettings::ProjectionPreset::FOV90);
    if (edge(Keys::D3, k3Down_)) ApplyPreset(world, *settings, SolarSettings::ProjectionPreset::ORTHO);
    if (edge(Keys::P, pDown_)) {
        const int next = (static_cast<int>(settings->projection) + 1) % 3;
        ApplyPreset(world, *settings, static_cast<SolarSettings::ProjectionPreset>(next));
    }

    // --- Активная камера ------------------------------------------------------
    CameraComponent* cam = nullptr;
    Query<CameraComponent> camQ(world);
    camQ.ForEach([&](Entity, CameraComponent& c) {
        if (c.active && !cam) cam = &c;
    });
    if (!cam) return;

    if (mode_ == Mode::FPS) {
        UpdateFPS(world, *cam, deltaTime);
    } else {
        UpdateOrbit(world, *cam, deltaTime);
    }
}
