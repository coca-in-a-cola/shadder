// OrbitCameraSystem — орбитальная камера для демо Phong.
// ЛКМ + движение мыши = вращение вокруг цели; колесо = зум; WASD/QE = высота.
// Позиция/цель пишутся в CameraComponent (eye/target), матрицы строит CameraSystem.

#pragma once

#include "shadder.hpp"

#include <windows.h>
#include <DirectXMath.h>

class OrbitCameraSystem : public ISystem {
public:
    OrbitCameraSystem(Game* game, DirectX::XMFLOAT3 target, float distance)
        : game_(game), target_(target), distance_(distance) {}

    void OnUpdate(World& world, float dt) override {
        using namespace DirectX;

        auto* input = game_ ? game_->GetInputDevice() : nullptr;
        auto* display = game_ ? game_->GetDisplay() : nullptr;
        if (!input || !display) return;

        // --- Вращение: ЛКМ + мышь -----------------------------------------
        if (input->IsKeyDown(Keys::LeftButton)) {
            yaw_   += static_cast<float>(input->MouseOffset.x) * 0.005f;
            pitch_ += static_cast<float>(input->MouseOffset.y) * 0.005f;
            // Ограничение по вертикали (не даём уйти через полюс)
            pitch_ = std::max(-1.5f, std::min(1.5f, pitch_));
        }

        // --- Зум колесом ----------------------------------------------------
        distance_ += static_cast<float>(input->MouseWheelDelta) * -0.005f;
        distance_ = std::max(2.0f, std::min(30.0f, distance_));

        // --- Высота цели: W/S (вперёд/назад по вертикали) -------------------
        if (input->IsKeyDown(Keys::W)) target_.y += 2.0f * dt;
        if (input->IsKeyDown(Keys::S)) target_.y -= 2.0f * dt;
        target_.y = std::max(0.0f, std::min(5.0f, target_.y));

        // --- Позиция камеры на орбите (LH: Z вперёд от камеры к цели) -------
        const float cp = std::cos(pitch_);
        XMFLOAT3 eye = {
            target_.x + distance_ * cp * std::sin(yaw_),
            target_.y + distance_ * std::sin(pitch_),
            target_.z - distance_ * cp * std::cos(yaw_),
        };

        // Пишем в первую активную камеру (CameraSystem построит View/Projection).
        Query<CameraComponent> q(world);
        q.ForEach([&](Entity, CameraComponent& c) {
            if (c.active && c.projection == CameraComponent::Projection::PERSPECTIVE) {
                c.eye = eye;
                c.target = target_;
                c.up = { 0.0f, 1.0f, 0.0f };
            }
        });
    }

private:
    Game* game_;
    DirectX::XMFLOAT3 target_;
    float distance_;
    float yaw_ = 0.6f;    // стартовый азимут
    float pitch_ = 0.35f; // стартовый угол возвышения
};
