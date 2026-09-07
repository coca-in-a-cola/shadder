#pragma once

#include "core/ecs/EcsTypes.h"
#include "core/ecs/World.h"
#include <memory>
#include <vector>

class Prefab {
public:
    Prefab() = default;

    Prefab(Prefab&&) = default;
    Prefab& operator=(Prefab&&) = default;

    Prefab(const Prefab&) = delete;
    Prefab& operator=(const Prefab&) = delete;

    template <class T, class F>
    Prefab& With(F&& init) {
        apps_.push_back(std::make_unique<Applier<T, std::decay_t<F>>>(
            std::forward<F>(init)));
        return *this;
    }

    Entity Instantiate(World& world) const {
        Entity e = world.CreateEntity();
        for (auto& a : apps_) {
            a->Apply(world, e);
        }
        return e;
    }

private:
    struct IApplier {
        virtual void Apply(World&, Entity) const = 0;
        virtual ~IApplier() = default;
    };

    template <class T, class F>
    struct Applier : IApplier {
        F init;

        explicit Applier(F&& f) : init(std::forward<F>(f)) {}

        void Apply(World& w, Entity e) const override {
            w.RegisterComponent<T>();
            auto& c = w.AddComponent<T>(e);
            init(c);
        }
    };

    std::vector<std::unique_ptr<IApplier>> apps_;
};
