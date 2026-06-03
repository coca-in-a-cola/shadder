#include "register_types.h"
#include "core/ecs/World.h"
#include "VelocityComponent.h"
#include "ColliderComponent.h"

void initialize_physics_module(World& world) {
    world.RegisterComponent<VelocityComponent>();
    world.RegisterComponent<ColliderComponent>();
}

void uninitialize_physics_module(World& /*world*/) {
    // No-op for now.
}
