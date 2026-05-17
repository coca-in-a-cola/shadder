#pragma once

#include "EcsTypes.h"
#include "Component.h"
#include "Storage.h"
#include "System.h"
#include <vector>
#include <memory>
#include <cassert>
#include <array>

namespace shadder::ecs {

class World {
public:
    World();
    ~World();

    // -----------------------------------------------------------------
    // Entity lifecycle
    // -----------------------------------------------------------------
    Entity CreateEntity();
    void DestroyEntity(const Entity& e);
    bool IsAlive(const Entity& e) const;
    EntityGeneration GetGeneration(EntityIndex idx) const;

    // -----------------------------------------------------------------
    // Component registration (automatically called for each component via registry)
    // -----------------------------------------------------------------
    void AutoRegisterFromRegistry(); // called once after construction

    // -----------------------------------------------------------------
    // Component operations (templates)
    // -----------------------------------------------------------------
    template<class C, class... Args>
    C& AddComponent(const Entity& e, Args&&... args);

    template<class C>
    void RemoveComponent(const Entity& e);

    template<class C>
    C* GetComponent(const Entity& e);

    template<class C>
    const C* GetComponent(const Entity& e) const;

    template<class C>
    bool HasComponent(const Entity& e) const;

    // -----------------------------------------------------------------
    // Simple iteration – for this initial step we only support one-type loops.
    // -----------------------------------------------------------------
    template<class C, class F>
    void ForEach(F&& fn);

    // Future placeholder – called from Game::Update (currently no-op)
    void Update(float /*deltaTime*/) {}

    // -----------------------------------------------------------------
    // System registration and execution
    // -----------------------------------------------------------------
    void RegisterSystem(SystemPhase phase, std::unique_ptr<ISystem> system);

    template<class S, class... Args>
    void RegisterSystem(SystemPhase phase, Args&&... args) {
        static_assert(std::is_base_of_v<ISystem, S>, "S must derive from ISystem");
        RegisterSystem(phase, std::make_unique<S>(std::forward<Args>(args)...));
    }

    void UpdateSystems(SystemPhase phase, float dt);

    // -----------------------------------------------------------------
    // Make Query a friend so it can access GetStorage
    // -----------------------------------------------------------------
    template<class... Cs>
    friend class Query;

private:
    // -----------------------------------------------------------------
    // Internals – entity allocator
    // -----------------------------------------------------------------
    std::vector<EntityGeneration> generations_; // index -> generation
    std::vector<EntityIndex> free_list_;       // recycled indices

    // -----------------------------------------------------------------
    // Internals – component storages, indexed by ComponentTypeID
    // -----------------------------------------------------------------
    std::vector<std::unique_ptr<IComponentStorage>> storages_;

    // -----------------------------------------------------------------
    // Internals – systems per phase
    // -----------------------------------------------------------------
    std::array<std::vector<std::unique_ptr<ISystem>>, static_cast<size_t>(SystemPhase::COUNT)> systems_;

    // Helper to retrieve the concrete storage for a component type.
    template<class C>
    SparseSetStorage<C>* GetStorage();
};

// =========================== Implementation ===============================

inline World::World() = default;
inline World::~World() = default;

inline Entity World::CreateEntity() {
    EntityIndex idx;
    if (!free_list_.empty()) {
        idx = free_list_.back();
        free_list_.pop_back();
    } else {
        idx = static_cast<EntityIndex>(generations_.size());
        generations_.push_back(0);
    }
    // Increment generation to invalidate stale handles.
    ++generations_[idx];
    return Entity{idx, generations_[idx]};
}

inline void World::DestroyEntity(const Entity& e) {
    if (!IsAlive(e)) return;
    // Remove all components belonging to this entity.
    for (auto& storage : storages_) {
        if (storage) storage->RemoveByEntity(e.index);
    }
    // Add index back to free list for reuse.
    free_list_.push_back(e.index);
    // Increment generation to poison old handles.
    ++generations_[e.index];
}

inline bool World::IsAlive(const Entity& e) const {
    return e.index < generations_.size() && generations_[e.index] == e.generation && e.index != INVALID_ENTITY_INDEX;
}

inline EntityGeneration World::GetGeneration(EntityIndex idx) const {
    if (idx < generations_.size()) return generations_[idx];
    return INVALID_GENERATION;
}

inline void World::AutoRegisterFromRegistry() {
    const auto& entries = ComponentRegistry::Instance().Entries();
    for (const auto& entry : entries) {
        // Ensure the storage vector can hold this component type.
        ComponentTypeID type_id = static_cast<ComponentTypeID>(storages_.size());
        // Assign the id back to the component's static field.
        if (entry.id_ptr) *entry.id_ptr = type_id;
        // Create storage via factory and store it.
        storages_.push_back(entry.factory());
    }
}

// -----------------------------------------------------------------
// System helpers
// -----------------------------------------------------------------

inline void World::RegisterSystem(SystemPhase phase, std::unique_ptr<ISystem> system) {
    systems_[static_cast<size_t>(phase)].push_back(std::move(system));
}

inline void World::UpdateSystems(SystemPhase phase, float dt) {
    for (auto& sys : systems_[static_cast<size_t>(phase)]) {
        sys->OnUpdate(*this, dt);
    }
}

// ----------------------- Component helpers ----------------------------

template<class C>
inline SparseSetStorage<C>* World::GetStorage() {
    static_assert(std::is_base_of_v<ComponentBase, C>, "C must derive from ComponentBase");
    ComponentTypeID type_id = C::component_id;
    assert(type_id != INVALID_COMPONENT_TYPE && "Component type not registered. Did you forget AutoRegisterFromRegistry()?" );
    // The storage vector must already contain an entry at type_id.
    assert(type_id < storages_.size());
    return static_cast<SparseSetStorage<C>*>(storages_[type_id].get());
}

template<class C, class... Args>
inline C& World::AddComponent(const Entity& e, Args&&... args) {
    assert(IsAlive(e) && "AddComponent on dead entity");
    auto* storage = GetStorage<C>();
    return storage->Emplace(e.index, std::forward<Args>(args)...);
}

template<class C>
inline void World::RemoveComponent(const Entity& e) {
    if (!IsAlive(e)) return;
    auto* storage = GetStorage<C>();
    storage->RemoveByEntity(e.index);
}

template<class C>
inline C* World::GetComponent(const Entity& e) {
    if (!IsAlive(e)) return nullptr;
    auto* storage = GetStorage<C>();
    return storage->Get(e.index);
}

template<class C>
inline const C* World::GetComponent(const Entity& e) const {
    if (!IsAlive(e)) return nullptr;
    auto* storage = const_cast<World*>(this)->GetStorage<C>();
    return storage->Get(e.index);
}

template<class C>
inline bool World::HasComponent(const Entity& e) const {
    if (!IsAlive(e)) return false;
    auto* storage = const_cast<World*>(this)->GetStorage<C>();
    return storage->HasEntity(e.index);
}

// ----------------------- Simple iteration ----------------------------

template<class C, class F>
inline void World::ForEach(F&& fn) {
    auto* storage = GetStorage<C>();
    size_t sz = storage->Size();
    for (size_t i = 0; i < sz; ++i) {
        Entity entity{ storage->EntityAtDense(i), generations_[storage->EntityAtDense(i)] };
        fn(entity, storage->AtDense(i));
    }
}

} // namespace shadder::ecs
