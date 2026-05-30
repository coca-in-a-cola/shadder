#pragma once

#include "ecs/Component.h"
#include "ecs/EcsTypes.h"
#include <DirectXMath.h>

// -----------------------------------------------------------------------------
// InstanceTag
// -----------------------------------------------------------------------------
// Помечает сущность как один экземпляр (instance), который должен попасть в
// instance-буфер указанного рендер-батча (InstancedMeshComponent).
//
// batch — индекс сущности-батча (InstancedMeshComponent живёт на ней).
// color — per-instance цвет, уходит в instance-буфер вместе с трансформом.
// -----------------------------------------------------------------------------
struct InstanceTag final : public ComponentBase {
public:
    EntityIndex batch = INVALID_ENTITY_INDEX;
    DirectX::XMFLOAT4 color{1.0f, 1.0f, 1.0f, 1.0f};

    InstanceTag() = default;
    explicit InstanceTag(EntityIndex batchEntity) : batch(batchEntity) {}
    InstanceTag(EntityIndex batchEntity, const DirectX::XMFLOAT4& col)
        : batch(batchEntity), color(col) {}
};
