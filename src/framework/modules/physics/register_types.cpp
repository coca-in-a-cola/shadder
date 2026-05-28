#include "register_types.h"
#include "ecs/Component.h"
#include "ecs/Storage.h"
#include "VelocityComponent.h"
#include "ColliderComponent.h"

void initialize_physics_module() {
    ComponentRegistry::Instance().Register({
        &VelocityComponent::component_id,
        []() -> std::unique_ptr<IComponentStorage> { return CreateStorageFor<VelocityComponent>(); },
        VelocityComponent::ComponentName()
    });
    ComponentRegistry::Instance().Register({
        &ColliderComponent::component_id,
        []() -> std::unique_ptr<IComponentStorage> { return CreateStorageFor<ColliderComponent>(); },
        ColliderComponent::ComponentName()
    });
}

void uninitialize_physics_module() {
    // No-op for now.
}
