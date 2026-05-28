#include "register_types.h"
#include "ecs/Component.h"
#include "ecs/Storage.h"
#include "Transform3D.h"

void initialize_transform_module() {
    ComponentRegistry::Instance().Register({
        &Transform3D::component_id,
        []() -> std::unique_ptr<IComponentStorage> { return CreateStorageFor<Transform3D>(); },
        Transform3D::ComponentName()
    });
}

void uninitialize_transform_module() {
    // No-op for now.
}
