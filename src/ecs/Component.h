#pragma once

#include "EcsTypes.h"
#include <memory>
#include <vector>
#include <functional>
#include <string_view>

// Forward declaration of storage class template
template <class T>
class SparseSetStorage;

// Base class for all components (marker only)
class ComponentBase {};

// Interface for component storage (type‑erased)
class IComponentStorage {
public:
    virtual ~IComponentStorage() = default;
    virtual void RemoveByEntity(EntityIndex e) = 0;
    virtual bool HasEntity(EntityIndex e) const = 0;
    virtual size_t Size() const = 0;
    virtual EntityIndex EntityAtDense(size_t idx) const = 0;
};

// Registry entry – holds a pointer to the component's static ID field and a factory that creates its storage.
struct ComponentRegistryEntry {
    ComponentTypeID* id_ptr; // points to the static component_id inside the component class
    std::function<std::unique_ptr<IComponentStorage>()> factory; // creates SparseSetStorage<T>
    std::string_view name;   // for debugging / logs
};

// Global singleton registry – filled by the macro in .cpp files.
class ComponentRegistry {
public:
    static ComponentRegistry& Instance() {
        static ComponentRegistry instance;
        return instance;
    }
    void Register(ComponentRegistryEntry entry) {
        entries_.push_back(std::move(entry));
    }
    const std::vector<ComponentRegistryEntry>& Entries() const { return entries_; }
private:
    std::vector<ComponentRegistryEntry> entries_;
};

// Helper to create storage for a concrete component type.
// Returns a pointer typed as IComponentStorage (base) so it can be stored in the World.
template <class T>
std::unique_ptr<IComponentStorage> CreateStorageFor() {
    // Directly allocate the concrete storage and up‑cast the unique_ptr.
    return std::unique_ptr<IComponentStorage>(new SparseSetStorage<T>());
}
