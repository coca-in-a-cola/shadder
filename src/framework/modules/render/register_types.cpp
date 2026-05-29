#include "register_types.h"
#include "ecs/World.h"
#include "MeshComponent.h"
#include "MaterialComponent.h"

void initialize_render_module(World& world) {
    world.RegisterComponent<MeshComponent>();
    world.RegisterComponent<MaterialComponent>();
}

void uninitialize_render_module(World& /*world*/) {
    // No-op for now.
}
