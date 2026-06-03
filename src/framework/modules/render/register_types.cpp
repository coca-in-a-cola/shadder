#include "register_types.h"
#include "core/ecs/World.h"
#include "MeshComponent.h"
#include "MaterialComponent.h"
#include "InstancedMeshComponent.h"
#include "InstanceTag.h"

void initialize_render_module(World& world) {
    world.RegisterComponent<MeshComponent>();
    world.RegisterComponent<MaterialComponent>();
    world.RegisterComponent<InstancedMeshComponent>();
    world.RegisterComponent<InstanceTag>();
}

void uninitialize_render_module(World& /*world*/) {
    // No-op for now.
}
