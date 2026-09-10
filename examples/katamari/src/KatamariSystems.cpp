// KatamariSystems — реализация систем примера SH-S2. См. KatamariSystems.h.

#include "KatamariSystems.h"
#include "KatamariComponents.h"

#include "shadder.hpp"
#include "framework/game/Game.h"
#include "framework/modules/transform/Transform3D.h"
#include "core/ecs/Query.h"
#include <DirectXMath.h>
#include <cmath>
#include <iostream>

using namespace DirectX;

// -----------------------------------------------------------------------------
// KatamariBallSystem
// -----------------------------------------------------------------------------
// Движение шара в горизонтальной плоскости (2.5D: Y = radius, спека п.4).
// Шар «катится»: вращение вокруг оси, перпендикулярной направлению движения
// (как реальный шар). Направление WASD — относительно камеры (как в Katamari).
// -----------------------------------------------------------------------------

void KatamariBallSystem::OnUpdate(World& world, float dt) {
    auto* input = game_ ? game_->GetInputDevice() : nullptr;
    if (!input) return;

    // Ввод: WASD в мировых осях, повернутых yaw камеры. W = «от камеры».
    float ix = 0.0f, iz = 0.0f;
    if (input->IsKeyDown(Keys::W)) iz += 1.0f;
    if (input->IsKeyDown(Keys::S)) iz -= 1.0f;
    if (input->IsKeyDown(Keys::D)) ix += 1.0f;
    if (input->IsKeyDown(Keys::A)) ix -= 1.0f;

    // Камера смотрит с фиксированным yaw_ = 0 (стоит на -Z): W = +Z, D = +X.
    XMFLOAT2 dir = { ix, iz };
    const float lenSq = dir.x * dir.x + dir.y * dir.y;
    if (lenSq > 1e-6f) {
        const float inv = 1.0f / std::sqrt(lenSq);
        dir.x *= inv; dir.y *= inv;
    }

    Query<KatamariBallComponent, Transform3D> q(world);
    q.ForEach([&](Entity, KatamariBallComponent& ball, Transform3D& tr) {
        // Скорость слегка растёт с размером (большой шар катится бодрее).
        const float speed = ball.moveSpeed * (0.8f + ball.radius * 0.15f);

        // Движение по плоскости; Y жёстко = радиус (2.5D).
        tr.position.x += dir.x * speed * dt;
        tr.position.z += dir.y * speed * dt;
        tr.position.y = ball.radius;

        // Визуальное вращение шара: ось = (up × moveDir), угол = путь / радиус.
        if (lenSq > 1e-6f) {
            const float path = speed * dt;
            const float angle = path / std::max(0.05f, ball.radius);
            // Ось вращения: перпендикуляр движению, горизонтальная.
            // moveDir = (dx, 0, dz) => ось = (dz, 0, -dx) для LH-системы (катится вперёд).
            XMVECTOR axis = XMVectorSet(dir.y, 0.0f, -dir.x, 0.0f);
            XMVECTOR dq = XMQuaternionRotationAxis(axis, angle);
            XMVECTOR q0 = XMLoadFloat4(&tr.rotation);
            XMStoreFloat4(&tr.rotation, XMQuaternionNormalize(XMQuaternionMultiply(dq, q0)));
        }
    });
}

// -----------------------------------------------------------------------------
// KatamariPickupSystem
// -----------------------------------------------------------------------------
// Подбор (спека п.4-5):
//   1. у каждого объекта bounding sphere из геометрии (radius = ObjMeshData
//      radius * scale, центр = position);
//   2. подбор = пересечение сфер (dist < rBall + rObj);
//   3. подбираются ТОЛЬКО объекты меньше шара; больше — не подбираются;
//   4. подобранный объект прилипает к поверхности шара (вращается вместе с ним,
//      т.к. делаем его дочерним по трансформу через хранение локального офсета);
//   5. шар растёт.
// -----------------------------------------------------------------------------

void KatamariPickupSystem::OnUpdate(World& world, float) {
    // --- Данные шара ----------------------------------------------------------
    XMFLOAT3 ballPos = { 0.0f, 0.0f, 0.0f };
    XMFLOAT4 ballRot = { 0.0f, 0.0f, 0.0f, 1.0f };
    float ballRadius = 0.0f;
    bool hasBall = false;

    {
        Query<KatamariBallComponent, Transform3D> bq(world);
        bq.ForEach([&](Entity, KatamariBallComponent& ball, Transform3D& tr) {
            ballPos = tr.position;
            ballRot = tr.rotation;
            ballRadius = ball.radius;
            hasBall = true;
        });
    }
    if (!hasBall) return;

    // --- Подбор: пересечение сфер ---------------------------------------------
    Query<KatamariPickupComponent, Transform3D> pq(world);
    pq.ForEach([&](Entity, KatamariPickupComponent& p, Transform3D& tr) {
        if (p.pickedUp) return;

        const float dx = tr.position.x - ballPos.x;
        const float dy = tr.position.y - ballPos.y;
        const float dz = tr.position.z - ballPos.z;
        const float distSq = dx * dx + dy * dy + dz * dz;
        const float sum = ballRadius + p.radius;

        // Пересечение сфер + объект меньше шара.
        if (distSq < sum * sum && p.radius < ballRadius) {
            p.pickedUp = true;

            // Направление от центра шара к объекту (локально: до поворота шара).
            XMVECTOR toObj = XMVector3Normalize(XMVectorSet(dx, dy, dz, 0.0f));
            XMFLOAT3 dirLocal;
            XMStoreFloat3(&dirLocal, toObj);

            // Сферические координаты точки прилипания (в системе шара).
            const float theta = std::atan2(dirLocal.z, dirLocal.x); // вокруг Y
            const float phi = std::acos(std::max(-1.0f, std::min(1.0f, dirLocal.y)));

            p.stuckAngle = theta;
            p.stuckHeight = std::cos(phi); // = dirLocal.y

            // Рост шара: пропорционально объёму подобранного (кубический корень),
            // с минимумом, чтобы мелочь тоже двигала прогресс.
            // (Демо-упрощение: растём на growPerPickup * rObj / rBall.)
            Query<KatamariBallComponent> ballQ(world);
            ballQ.ForEach([&](Entity, KatamariBallComponent& ball) {
                ball.radius += ball.growPerPickup * p.radius / std::max(0.05f, ball.radius);
                ball.radius = std::min(ball.radius, 12.0f); // потолок
            });
        }
    });

    // --- Прилипшие объекты вращаются вместе с шаром ---------------------------
    // Обновляем позицию подобранных: точка на сфере шара, повёрнутая кватернионом
    // шара (шар вращается в KatamariBallSystem, объекты «застревают» на нём).
    XMFLOAT4 ballRotNow = ballRot;
    XMFLOAT3 ballPosNow = ballPos;
    float ballRadiusNow = ballRadius;
    {
        Query<KatamariBallComponent, Transform3D> bq2(world);
        bq2.ForEach([&](Entity, KatamariBallComponent& ball, Transform3D& tr) {
            ballRotNow = tr.rotation;
            ballPosNow = tr.position;
            ballRadiusNow = ball.radius;
            tr.scale = { ball.radius / 0.5f, ball.radius / 0.5f, ball.radius / 0.5f };
        });
    }

    Query<KatamariPickupComponent, Transform3D> pq2(world);
    pq2.ForEach([&](Entity, KatamariPickupComponent& p, Transform3D& tr) {
        if (!p.pickedUp) return;

        // Локальная точка на поверхности: сферические (stuckAngle, stuckHeight).
        const float r = ballRadiusNow + p.radius * 0.4f; // чуть утоплены в шар
        const float cy = p.stuckHeight;                  // cos(phi)
        const float sy = std::sqrt(std::max(0.0f, 1.0f - cy * cy));
        XMVECTOR local = XMVectorSet(r * sy * std::cos(p.stuckAngle),
                                     r * cy,
                                     r * sy * std::sin(p.stuckAngle), 0.0f);

        // Поворот вместе с шаром + перевод в мир.
        XMVECTOR world = XMVector3Rotate(local, XMLoadFloat4(&ballRotNow));
        XMFLOAT3 wf;
        XMStoreFloat3(&wf, world);
        tr.position = { ballPosNow.x + wf.x, ballPosNow.y + wf.y, ballPosNow.z + wf.z };
    });
}

// -----------------------------------------------------------------------------
// KatamariCameraSystem
// -----------------------------------------------------------------------------
// Камера за спиной (спека п.6, «как Katamari»): НЕ зависит от вращения шара
// (orbit-независимая). Фиксированный yaw, высота и дистанция (зум колесом).
// Пишет eye/target/up в CameraComponent; матрицы строит CameraSystem (PRE_RENDER).
// -----------------------------------------------------------------------------

void KatamariCameraSystem::OnUpdate(World& world, float dt) {
    auto* input = game_ ? game_->GetInputDevice() : nullptr;
    auto* display = game_ ? game_->GetDisplay() : nullptr;

    // Зум колесом мыши.
    if (input) {
        wheelAccum_ += input->MouseWheelDelta;
        input->MouseWheelDelta = 0;
        if (wheelAccum_ != 0) {
            distance_ = std::max(4.0f, std::min(40.0f,
                distance_ - static_cast<float>(wheelAccum_) * 0.01f * 4.0f));
            wheelAccum_ = 0;
        }
    }

    // Позиция шара.
    XMFLOAT3 ballPos = { 0.0f, 0.0f, 0.0f };
    float ballRadius = 0.5f;
    bool hasBall = false;
    {
        Query<KatamariBallComponent, Transform3D> bq(world);
        bq.ForEach([&](Entity, KatamariBallComponent& ball, Transform3D& tr) {
            ballPos = tr.position;
            ballRadius = ball.radius;
            hasBall = true;
        });
    }
    if (!hasBall) return;

    // Дистанция/высота масштабируются с ростом шара.
    const float dist = distance_ + ballRadius * 2.0f;
    const float height = height_ + ballRadius * 1.5f;

    // Камера стоит на -Z от шара (yaw фиксирован), смотрит на шар.
    XMFLOAT3 eye = { ballPos.x, ballPos.y + height, ballPos.z - dist };
    XMFLOAT3 target = { ballPos.x, ballPos.y + ballRadius * 0.5f, ballPos.z };
    XMFLOAT3 up = { 0.0f, 1.0f, 0.0f };

    Query<CameraComponent> cq(world);
    cq.ForEach([&](Entity, CameraComponent& c) {
        if (!c.active) return;
        c.eye = eye;
        c.target = target;
        c.up = up;
        // farZ растёт вместе с шаром, чтобы мир не исчезал в клиппинге.
        c.farZ = std::max(c.farZ, ballRadius * 40.0f + 150.0f);
    });
}

// -----------------------------------------------------------------------------
// KatamariStatsSystem — раз в 3 секунды печатает прогресс в stdout.
// -----------------------------------------------------------------------------

void KatamariStatsSystem::OnUpdate(World& world, float dt) {
    sincePrint_ += dt;
    if (sincePrint_ < 3.0f) return;
    sincePrint_ = 0.0f;

    int collected = 0, total = 0;
    float radius = 0.0f;
    Query<KatamariStatsComponent> sq(world);
    sq.ForEach([&](Entity, KatamariStatsComponent& s) {
        collected = s.collected;
        total = s.total;
    });
    Query<KatamariBallComponent> bq(world);
    bq.ForEach([&](Entity, KatamariBallComponent& b) { radius = b.radius; });

    std::cout << "[Katamari] radius: " << radius
              << " | collected: " << collected << "/" << total << '\n';
}
