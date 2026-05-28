#pragma once

#include "ecs/Component.h"
#include <vector>
#include <cstddef>
#include <algorithm>

namespace shadder {

// -----------------------------------------------------------------------------
// SparseSetStorage – dense array of components + sparse array mapping entity index -> dense index.
// -----------------------------------------------------------------------------

template <class T>
class SparseSetStorage final : public IComponentStorage {
public:
    using value_type = T;

    // Emplace component for an entity (asserts if already present).
    template <class... Args>
    T& Emplace(EntityIndex entity, Args&&... args) {
        // Ensure capacity of sparse array
        if (entity >= sparse_.size())
            sparse_.resize(entity + 1, INVALID_INDEX);
        // Entity must not already have a component.
        assert(!HasEntity(entity) && "Component already exists for entity");
        // Place component at the end of dense storage.
        dense_.emplace_back(std::forward<Args>(args)...);
        dense_entities_.push_back(entity);
        // Update sparse map.
        sparse_[entity] = static_cast<uint32_t>(dense_.size() - 1);
        return dense_.back();
    }

    void RemoveByEntity(EntityIndex entity) override {
        if (!HasEntity(entity))
            return; // no‑op
        uint32_t dense_idx = sparse_[entity];
        uint32_t last_idx = static_cast<uint32_t>(dense_.size() - 1);
        // Swap with the last element to keep dense packed.
        if (dense_idx != last_idx) {
            dense_[dense_idx] = std::move(dense_.back());
            EntityIndex moved_entity = dense_entities_[last_idx];
            dense_entities_[dense_idx] = moved_entity;
            sparse_[moved_entity] = dense_idx;
        }
        dense_.pop_back();
        dense_entities_.pop_back();
        sparse_[entity] = INVALID_INDEX;
    }

    bool HasEntity(EntityIndex entity) const override {
        return entity < sparse_.size() && sparse_[entity] != INVALID_INDEX;
    }

    // Accessors – nullptr if component not present.
    T* Get(EntityIndex entity) {
        if (!HasEntity(entity))
            return nullptr;
        return &dense_[sparse_[entity]];
    }
    const T* Get(EntityIndex entity) const {
        if (!HasEntity(entity))
            return nullptr;
        return &dense_[sparse_[entity]];
    }

    // Iteration helpers – used by World::ForEach.
    size_t Size() const override { return dense_.size(); }
    T& AtDense(size_t idx) { return dense_[idx]; }
    const T& AtDense(size_t idx) const { return dense_[idx]; }
    EntityIndex EntityAtDense(size_t idx) const { return dense_entities_[idx]; }

private:
    static constexpr uint32_t INVALID_INDEX = static_cast<uint32_t>(-1);
    std::vector<T> dense_;                 // component data
    std::vector<EntityIndex> dense_entities_; // parallel array of entity indices
    std::vector<uint32_t> sparse_;         // sparse mapping entity -> dense index (or INVALID_INDEX)
};

} // namespace shadder
