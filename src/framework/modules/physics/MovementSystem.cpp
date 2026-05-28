#include "framework/modules/physics/MovementSystem.h"
#include "ecs/Query.h"
#include "framework/modules/transform/Transform3D.h"
#include "framework/modules/physics/VelocityComponent.h"
#include <DirectXMath.h>

void MovementSystem::OnUpdate(World& world, float deltaTime) {
    Query<Transform3D, VelocityComponent> q(world);
    q.ForEach([deltaTime](Entity, Transform3D& tr, VelocityComponent& vel) {
        using namespace DirectX;
        XMVECTOR pos = XMLoadFloat3(&tr.position);
        XMVECTOR velocity = XMLoadFloat3(&vel.velocity);
        pos = XMVectorAdd(pos, XMVectorScale(velocity, deltaTime));
        XMStoreFloat3(&tr.position, pos);
    });
}
