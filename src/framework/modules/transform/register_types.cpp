#include "register_types.h"
#include "ecs/World.h"
#include "Transform3D.h"

void initialize_transform_module(World& world) {
    world.RegisterComponent<Transform3D>();
}

void uninitialize_transform_module(World& /*world*/) {
    // No-op for now.
}
