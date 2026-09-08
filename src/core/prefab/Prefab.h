#pragma once

#include "core/ecs/EcsTypes.h"
#include "core/ecs/World.h"
#include <memory>
#include <vector>

class PackedScene;

class Prefab {
public:
    Prefab() = default;

    Prefab(Prefab&&) = default;
    Prefab& operator=(Prefab&&) = default;

    Prefab(const Prefab&) = delete;
    Prefab& operator=(const Prefab&) = delete;

    // Type-erased component applier. Shared with PackedScene: the
    // Prefab::ToPackedScene bridge (implemented in PackedScene.h) clones
    // appliers into a single-node PackedScene.
    struct IApplier {
        virtual void Apply(World&, Entity) const = 0;
        virtual std::shared_ptr<IApplier> Clone() const = 0;
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

        std::shared_ptr<IApplier> Clone() const override {
            return std::make_shared<Applier<T, F>>(*this);
        }
    };

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

    // Bridge: build a self-contained single-node PackedScene from this Prefab
    // (appliers are cloned — no references back into the Prefab object).
    // Defined in PackedScene.h (needs the complete PackedScene type).
    PackedScene ToPackedScene() const;

private:
    friend class PackedScene;
    friend class SceneBuilder;

    std::vector<std::unique_ptr<IApplier>> apps_;
};
