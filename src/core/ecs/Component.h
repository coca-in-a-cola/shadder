#pragma once

#include "EcsTypes.h"
#include <memory>

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
