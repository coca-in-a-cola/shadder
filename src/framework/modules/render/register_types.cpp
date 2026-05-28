#include "register_types.h"
#include "ecs/Component.h"
#include "ecs/Storage.h"
#include "MeshComponent.h"
#include "MaterialComponent.h"

void initialize_render_module() {
    ComponentRegistry::Instance().Register({
        &MeshComponent::component_id,
        []() -> std::unique_ptr<IComponentStorage> { return CreateStorageFor<MeshComponent>(); },
        MeshComponent::ComponentName()
    });
    ComponentRegistry::Instance().Register({
        &MaterialComponent::component_id,
        []() -> std::unique_ptr<IComponentStorage> { return CreateStorageFor<MaterialComponent>(); },
        MaterialComponent::ComponentName()
    });
}

void uninitialize_render_module() {
    // No-op for now.
}
