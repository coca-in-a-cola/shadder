#include "register_types.h"
#include "ecs/World.h"
#include "CameraComponent.h"

void initialize_camera_module(World& world) {
    world.RegisterComponent<CameraComponent>();
}

void uninitialize_camera_module(World& /*world*/) {
}
