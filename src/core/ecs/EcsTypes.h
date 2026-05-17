#pragma once

#include <cstdint>

namespace shadder::ecs {

using EntityIndex = uint32_t;
using EntityGeneration = uint32_t;
constexpr EntityIndex INVALID_ENTITY_INDEX = static_cast<EntityIndex>(-1);
constexpr EntityGeneration INVALID_GENERATION = 0;

struct Entity {
    EntityIndex index{INVALID_ENTITY_INDEX};
    EntityGeneration generation{INVALID_GENERATION};
    constexpr bool IsValid() const noexcept {
        return index != INVALID_ENTITY_INDEX && generation != INVALID_GENERATION;
    }
};

using ComponentTypeID = uint32_t;
constexpr ComponentTypeID INVALID_COMPONENT_TYPE = static_cast<ComponentTypeID>(-1);

} // namespace shadder::ecs
