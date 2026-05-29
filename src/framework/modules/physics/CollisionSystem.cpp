#include "framework/modules/physics/CollisionSystem.h"
#include "ecs/Query.h"
#include "framework/modules/transform/Transform3D.h"
#include "framework/modules/physics/ColliderComponent.h"
#include <vector>

void CollisionSystem::OnUpdate(World& world, float) {
    std::vector<Entity> entities;
    Query<Transform3D, ColliderComponent> q(world);
    q.ForEach([&](Entity e, Transform3D&, ColliderComponent&) {
        entities.push_back(e);
    });

    for (size_t i = 0; i < entities.size(); ++i) {
        for (size_t j = i + 1; j < entities.size(); ++j) {
            Entity a = entities[i], b = entities[j];
            auto* trA = world.GetComponent<Transform3D>(a);
            auto* collA = world.GetComponent<ColliderComponent>(a);
            auto* trB = world.GetComponent<Transform3D>(b);
            auto* collB = world.GetComponent<ColliderComponent>(b);
            if (!trA || !collA || !trB || !collB) continue;

            float aMinX = trA->position.x - collA->halfExtents.x;
            float aMaxX = trA->position.x + collA->halfExtents.x;
            float aMinY = trA->position.y - collA->halfExtents.y;
            float aMaxY = trA->position.y + collA->halfExtents.y;

            float bMinX = trB->position.x - collB->halfExtents.x;
            float bMaxX = trB->position.x + collB->halfExtents.x;
            float bMinY = trB->position.y - collB->halfExtents.y;
            float bMaxY = trB->position.y + collB->halfExtents.y;

            if (aMinX <= bMaxX && aMaxX >= bMinX &&
                aMinY <= bMaxY && aMaxY >= bMinY) {
                OnCollision.emit(a, b);
            }
        }
    }
}
