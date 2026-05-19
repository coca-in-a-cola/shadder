#pragma once

#include <cstdint>
#include <vector>
#include <memory>

namespace shadder {

class World;

enum class SystemPhase : uint32_t {
    UPDATE = 0,
    PRE_RENDER,
    RENDER,
    POST_RENDER,
    COUNT
};

class ISystem {
public:
    virtual ~ISystem() = default;
    virtual void OnUpdate(World& world, float deltaTime) = 0;
};

} // namespace shadder
