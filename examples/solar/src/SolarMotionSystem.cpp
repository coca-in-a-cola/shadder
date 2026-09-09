// SolarMotionSystem — механика солнечной системы (SH-S1 п.3).
//
// Фазы UPDATE: каждый кадр
//   1) планеты ходят по орбитам вокруг солнца (SolarOrbit),
//   2) луны — вокруг своих планет (SolarOrbit с parentEntity = планета),
//   3) все тела вращаются вокруг собственной оси (SolarSpin -> кватернион).
//
// Порядок: луны читают позицию планеты, поэтому орбиты считаем до спинов,
// и в одном проходе сначала орбиты, потом спины (два ForEach внутри фазы).

#include "SolarMotionSystem.h"
#include "SolarComponents.h"

#include "core/ecs/Query.h"
#include "framework/modules/transform/Transform3D.h"
#include <DirectXMath.h>

void SolarMotionSystem::OnUpdate(World& world, float dt) {
    using namespace DirectX;

    // --- Орбиты: x = r*cos(a), z = -r*sin(a) (LH: Z вперёд), + наклон по Y ---
    Query<SolarOrbit, Transform3D> orbitQ(world);
    orbitQ.ForEach([&](Entity, SolarOrbit& orb, Transform3D& tr) {
        orb.angle += orb.speed * dt;
        if (orb.angle > XM_2PI) orb.angle -= XM_2PI;
        if (orb.angle < 0.0f)   orb.angle += XM_2PI;

        // Позиция центра орбиты: родитель (планета/солнце) или центр мира.
        XMFLOAT3 center = { 0.0f, 0.0f, 0.0f };
        if (orb.parentEntity.IsValid()) {
            if (const Transform3D* parentTr = world.GetComponent<Transform3D>(orb.parentEntity)) {
                center = parentTr->position;
            }
        }
        tr.position.x = center.x + orb.radius * std::cos(orb.angle);
        tr.position.z = center.z - orb.radius * std::sin(orb.angle);
        tr.position.y = center.y + orb.height;
    });

    // --- Спины: вращение вокруг собственной оси кватернионом ----------------
    Query<SolarSpin, Transform3D> spinQ(world);
    spinQ.ForEach([&](Entity, SolarSpin& spin, Transform3D& tr) {
        spin.angle += spin.speed * dt;
        if (spin.angle > XM_2PI) spin.angle -= XM_2PI;

        const XMVECTOR axis = XMVector3Normalize(XMLoadFloat3(&spin.axis));
        const XMVECTOR q = XMQuaternionRotationAxis(axis, spin.angle);
        XMStoreFloat4(&tr.rotation, q);
    });
}
