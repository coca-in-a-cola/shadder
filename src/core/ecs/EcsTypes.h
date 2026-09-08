#pragma once

#include <cstdint>


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
  constexpr bool operator==(const Entity &other) const noexcept {
    return index == other.index && generation == other.generation;
  }
  constexpr bool operator!=(const Entity &other) const noexcept {
    return !(*this == other);
  }
};

using ComponentTypeID = uint32_t;
constexpr ComponentTypeID INVALID_COMPONENT_TYPE = static_cast<ComponentTypeID>(-1);


