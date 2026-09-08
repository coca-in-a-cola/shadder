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
        // If this applier wraps a whole PackedScene (the FromPackedScene
        // bridge), return it — ToPackedScene/WithPrefab then SPLICE the scene
        // instead of wrapping the apply call. Default: not scene-backed.
        virtual const class PackedScene* AsScene() const { return nullptr; }
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

    // Instantiate into World.
    // Plain component prefab: creates ONE entity and applies With<T> appliers
    // in order (no forced Transform3D — callers may add their own).
    // Scene-backed prefab (FromPackedScene): instantiates the WHOLE scene
    // hierarchy (nested scenes included) and returns the scene root's entity.
    // Defined in PackedScene.h (needs the complete PackedScene type).
    Entity Instantiate(World& world) const;

    // Bridge: build a self-contained single-node PackedScene from this Prefab
    // (appliers are cloned — no references back into the Prefab object).
    // Defined in PackedScene.h (needs the complete PackedScene type).
    PackedScene ToPackedScene() const;

    // Reverse bridge: wrap an existing PackedScene as a Prefab. Instantiate()
    // then behaves like scene.Instantiate(): the whole hierarchy is unpacked
    // (nested scenes included) and the scene root's entity is returned.
    // The scene is captured BY POINTER (shared reference, Godot instanced-
    // scene semantics) — it must outlive every Instantiate() of this Prefab.
    // Defined in PackedScene.h.
    static Prefab FromPackedScene(const PackedScene& scene);

private:
    friend class PackedScene;
    friend class SceneBuilder;

    // Scene-backed applier — declared here, methods defined in PackedScene.h
    // (Apply needs the complete PackedScene type).
    struct SceneApplier : IApplier {
        const PackedScene* scene;
        explicit SceneApplier(const PackedScene* s);
        void Apply(World& w, Entity seed) const override;
        std::shared_ptr<IApplier> Clone() const override;
        const PackedScene* AsScene() const override { return scene; }
    };

    std::vector<std::unique_ptr<IApplier>> apps_;
};
