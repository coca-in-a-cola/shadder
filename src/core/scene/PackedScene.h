#pragma once

#include "core/ecs/World.h"
#include "core/ecs/EcsTypes.h"
#include "core/prefab/Prefab.h"
#include "core/scene/SceneNode.h"
#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <unordered_set>

// -----------------------------------------------------------------------------
// PackedScene — Godot-style serializable scene description
// -----------------------------------------------------------------------------
// A PackedScene is a serializable scene structure that can be instantiated
// into a World multiple times. It stores the hierarchy and component data
// needed to recreate the scene.
//
// In Godot: PackedScene is the serialized form; Scene is the instanced version.
// Here: PackedScene is the template; Instantiate() creates nodes/entities in World.
//
// Nesting (analog of Godot instanced scenes): a node can reference another
// PackedScene as a child. Instantiate() recursively unpacks nested scenes —
// the referenced scene's root node BECOMES the child node (its name/transform
// can be overridden per reference, like editing an instanced scene in Godot).
// A Prefab is just the degenerate case: a single-node PackedScene (see
// Prefab::ToPackedScene and SceneBuilder::WithPrefab).
//
// Description is built in code (builder style); a file format is a later step.
// -----------------------------------------------------------------------------
class PackedScene {
public:
    // Component initializer function type
    using ComponentInitFn = std::function<void(World&, Entity)>;

    struct NodeData {
        std::string name; // optional name for finding nodes
        Transform3D local_transform{};
        std::vector<ComponentInitFn> component_inits;
        std::vector<NodeData> children;

        // Parent index in the flat array (-1 for root)
        int parent_index = -1;

        // Nested scene referenced as a child of this node (Godot instanced
        // scene). At Instantiate() the referenced scene's root node becomes
        // this node: its name/local here act as overrides on that root.
        // SHARED BY POINTER — this scene does not own it. The referenced
        // scene must outlive every Instantiate() of this scene (like a
        // Prefab object captured by a component init lambda).
        const PackedScene* nested = nullptr;

        bool IsNestedScene() const { return nested != nullptr; }
    };

    PackedScene() = default;
    PackedScene(const PackedScene&) = default;
    PackedScene& operator=(const PackedScene&) = default;
    PackedScene(PackedScene&&) = default;
    PackedScene& operator=(PackedScene&&) = default;

    // -------------------------------------------------------------------------
    // Build API (Godot-style: build in code, not from file)
    // -------------------------------------------------------------------------

    // Add a root node (or child of parent_index)
    int AddNode(const Transform3D& local = Transform3D{},
                int parent_index = -1,
                const char* name = nullptr) {
        NodeData node;
        node.local_transform = local;
        node.parent_index = parent_index;
        if (name) node.name = name;

        int index = static_cast<int>(nodes_.size());
        nodes_.push_back(std::move(node));
        return index;
    }

    // Add a fully-formed node (splice/serialization support)
    int AddNodeData(NodeData nd) {
        int index = static_cast<int>(nodes_.size());
        nodes_.push_back(std::move(nd));
        return index;
    }

    // Add a component initializer to a node
    template <class Component, class Fn>
    void AddComponent(int node_index, Fn&& init_fn) {
        if (node_index < 0 || node_index >= static_cast<int>(nodes_.size())) return;
        nodes_[node_index].component_inits.push_back(
            [fn = std::forward<Fn>(init_fn)](World& w, Entity e) mutable {
                w.RegisterComponent<Component>();
                auto& c = w.AddComponent<Component>(e);
                fn(c);
            }
        );
    }

    // Add a raw component initializer (already in (World&, Entity) shape).
    // Used by Prefab::ToPackedScene to bridge type-erased appliers.
    void AddInit(int node_index, ComponentInitFn fn) {
        if (node_index < 0 || node_index >= static_cast<int>(nodes_.size())) return;
        nodes_[node_index].component_inits.push_back(std::move(fn));
    }

    // Convenience: add component from Prefab's With<T> pattern
    template <class Component, class Fn>
    void With(int node_index, Fn&& init_fn) {
        AddComponent<Component>(node_index, std::forward<Fn>(init_fn));
    }

    // -------------------------------------------------------------------------
    // Nesting: reference another PackedScene as a child node (Godot instanced
    // scene). Instantiate() unpacks it recursively: the referenced scene's
    // root node becomes the child of `parent_index`, and `local`/`name` act
    // as per-instance overrides on that root (like editing an instanced
    // scene node in the Godot editor).
    //
    // Lifetime: the referenced scene is SHARED, not copied — it must outlive
    // every Instantiate() of this scene.
    // A self/cycle reference is rejected at Add time (Instantiate would never
    // terminate otherwise).
    // -------------------------------------------------------------------------
    int AddNestedScene(const PackedScene& child_scene,
                       const Transform3D& local = Transform3D{},
                       int parent_index = -1,
                       const char* name = nullptr) {
        if (&child_scene == this) return -1; // trivial self reference
        if (CreatesCycle(child_scene)) return -1;

        int index = AddNode(local, parent_index, name);
        if (index >= 0) {
            nodes_[index].nested = &child_scene;
        }
        return index;
    }

    // -------------------------------------------------------------------------
    // Splice support: merge another scene's nodes as children of a node in
    // THIS scene (used by SceneBuilder::WithPrefab and by the
    // Prefab::FromPackedScene bridge — see ToPackedScene below).
    // parent_index < 0 attaches the other scene's roots to this scene's roots.
    // The source is COPIED (self-contained: appliers are cloned), so no
    // lifetime requirements on `other`.
    // -------------------------------------------------------------------------
    void SpliceInto(const PackedScene& other, int parent_index = -1) {
        const int n = static_cast<int>(other.nodes_.size());
        const int base = static_cast<int>(nodes_.size());
        for (int i = 0; i < n; ++i) {
            NodeData nd = other.nodes_[i]; // copy (appliers are shared/immutable)
            const int oldp = nd.parent_index;
            const bool was_root = (oldp < 0 || oldp >= n);
            nd.parent_index = was_root ? parent_index : base + oldp;
            nodes_.push_back(std::move(nd));
        }
    }

    // -------------------------------------------------------------------------
    // Instantiate into World — creates entities and hierarchy
    // -------------------------------------------------------------------------
    // Returns the root entity of the instantiated scene (or invalid if empty)
    // The optional output_root_node receives the SceneNode tree mirroring the instance
    // -------------------------------------------------------------------------
    Entity Instantiate(World& world, SceneNode::Ptr* output_root_node = nullptr) const {
        if (nodes_.empty()) {
            return Entity{INVALID_ENTITY_INDEX, INVALID_GENERATION};
        }

        SceneNode::Ptr root_node;
        for (int i = 0; i < static_cast<int>(nodes_.size()); ++i) {
            if (!IsRootIndex(i)) continue;
            SceneNode::Ptr node = InstantiateNode(*this, i, nullptr, world);
            if (node && !root_node) {
                root_node = node;
            }
        }

        if (output_root_node) {
            *output_root_node = root_node;
        }

        return root_node ? root_node->GetEntity() : Entity{INVALID_ENTITY_INDEX, INVALID_GENERATION};
    }

    // Instantiate as child of existing SceneNode
    Entity InstantiateAsChild(World& world, SceneNode::Ptr parent) const {
        if (nodes_.empty() || !parent) {
            return Entity{INVALID_ENTITY_INDEX, INVALID_GENERATION};
        }

        SceneNode::Ptr first;
        for (int i = 0; i < static_cast<int>(nodes_.size()); ++i) {
            if (!IsRootIndex(i)) continue;
            SceneNode::Ptr node = InstantiateNode(*this, i, parent.get(), world);
            if (node && !first) {
                first = node;
            }
        }

        return first ? first->GetEntity() : Entity{INVALID_ENTITY_INDEX, INVALID_GENERATION};
    }

    // Get node count
    size_t GetNodeCount() const { return nodes_.size(); }

    // Access node data for inspection
    const NodeData& GetNode(size_t index) const { return nodes_[index]; }

    // Mutable access (used by SceneBuilder during construction)
    NodeData& MutableNode(size_t index) { return nodes_[index]; }

    // Default-constructed Transform3D check (position 0, identity rotation, scale 1).
    // Exact float compare against the defaults — used to detect "no override".
    static bool IsIdentityTransform(const Transform3D& t) {
        const Transform3D d{};
        return t.position.x == d.position.x && t.position.y == d.position.y && t.position.z == d.position.z &&
               t.rotation.x == d.rotation.x && t.rotation.y == d.rotation.y &&
               t.rotation.z == d.rotation.z && t.rotation.w == d.rotation.w &&
               t.scale.x == d.scale.x && t.scale.y == d.scale.y && t.scale.z == d.scale.z;
    }

private:
    std::vector<NodeData> nodes_;

    // Root = no valid parent inside this scene (matches legacy semantics:
    // an out-of-range parent_index degrades to root instead of dropping the node)
    bool IsRootIndex(int i) const {
        int p = nodes_[i].parent_index;
        return p < 0 || p >= static_cast<int>(nodes_.size());
    }

    // Cycle guard for AddNestedScene: the new child_scene must not (transitively)
    // reference this scene back.
    bool CreatesCycle(const PackedScene& child_scene) const {
        const PackedScene* target = this;
        std::vector<const PackedScene*> stack;
        std::unordered_set<const PackedScene*> visited;
        stack.push_back(&child_scene);
        while (!stack.empty()) {
            const PackedScene* s = stack.back();
            stack.pop_back();
            if (s == target) return true;
            if (!visited.insert(s).second) continue;
            for (const auto& n : s->nodes_) {
                if (n.nested) {
                    stack.push_back(n.nested);
                }
            }
        }
        return false;
    }

    // -------------------------------------------------------------------------
    // Recursive instantiation core
    // -------------------------------------------------------------------------
    // Instantiates packed node `index` of `scene` as a child of `parent`
    // (nullptr = tree root) and recurses into its packed children and nested
    // scenes. Returns the created node.
    // -------------------------------------------------------------------------
    static SceneNode::Ptr InstantiateNode(const PackedScene& scene, int index,
                                          SceneNode* parent, World& world) {
        if (index < 0 || index >= static_cast<int>(scene.nodes_.size())) return nullptr;
        const NodeData& data = scene.nodes_[index];

        // Nested scene reference: the referenced scene's root node BECOMES
        // this node (Godot instanced scene). The reference's name/local act
        // as per-instance overrides on that root; component initializers
        // declared on the reference run on the root's entity (avoid
        // AddComponent<Transform3D> there — use the transform override).
        if (data.nested) {
            const PackedScene& nested = *data.nested;
            SceneNode::Ptr first;
            for (int i = 0; i < static_cast<int>(nested.nodes_.size()); ++i) {
                if (!nested.IsRootIndex(i)) continue;
                SceneNode::Ptr node = InstantiateNode(nested, i, parent, world);
                if (!node) continue;
                if (!first) {
                    first = node;
                    if (!data.name.empty()) {
                        node->SetName(data.name);
                    }
                    if (!IsIdentityTransform(data.local_transform)) {
                        node->SetLocalTransform(data.local_transform);
                        // Keep the ECS plane in sync with the override
                        if (Transform3D* tr = world.GetComponent<Transform3D>(node->GetEntity())) {
                            *tr = data.local_transform;
                        }
                    }
                    for (const auto& init : data.component_inits) {
                        init(world, node->GetEntity());
                    }
                }
            }
            if (first) {
                // Packed children declared on the reference node attach under the root
                for (int i = 0; i < static_cast<int>(scene.nodes_.size()); ++i) {
                    if (scene.nodes_[i].parent_index == index) {
                        InstantiateNode(scene, i, first.get(), world);
                    }
                }
            }
            return first;
        }

        Entity e = world.CreateEntity();
        for (const auto& init : data.component_inits) {
            init(world, e);
        }
        // Ensure the entity carries a Transform3D reflecting its local transform
        // (skip if a component init already added one — ECS is the source of truth).
        if (!world.HasComponent<Transform3D>(e)) {
            world.AddComponent<Transform3D>(e) = data.local_transform;
        }

        auto node = std::make_shared<SceneNode>(e);
        node->SetName(data.name);
        // Mirror the ECS transform when a component init already added one
        // (possibly different from the packed local); otherwise keep the packed local.
        if (const Transform3D* tr = world.GetComponent<Transform3D>(e)) {
            node->SetLocalTransform(*tr);
        } else {
            node->SetLocalTransform(data.local_transform);
        }
        if (parent) {
            parent->AddChild(node);
        }

        // Packed children (declared with this node as parent_index)
        for (int i = 0; i < static_cast<int>(scene.nodes_.size()); ++i) {
            if (scene.nodes_[i].parent_index == index) {
                InstantiateNode(scene, i, node.get(), world);
            }
        }

        return node;
    }
};

// -----------------------------------------------------------------------------
// Prefab bridge — Prefab (type-erased appliers, single node) <-> PackedScene.
// Defined here because it needs the complete PackedScene type.
// -----------------------------------------------------------------------------
inline PackedScene Prefab::ToPackedScene() const {
    PackedScene s;
    for (const auto& a : apps_) {
        // Scene-backed appliers (FromPackedScene bridge) are SPLICES, not
        // component bundles — wrapping them in an AddInit would break any
        // later splice/node walk (the inner scene must contribute its nodes).
        if (const PackedScene* scene = a->AsScene()) {
            s.SpliceInto(*scene);
            continue;
        }
        // Clone the applier so the returned scene is self-contained
        // (no dangling references back into this Prefab).
        std::shared_ptr<IApplier> clone = a->Clone();
        s.AddInit(s.AddNode(), [clone](World& w, Entity e) { clone->Apply(w, e); });
    }
    return s;
}

// Scene-backed Prefab: Instantiate() = instantiate the wrapped scene and
// return its root entity. Plain appliers (mixed in via With<T>) still run on
// a seed entity, which is discarded when a scene is present.
inline Entity Prefab::Instantiate(World& world) const {
    for (const auto& a : apps_) {
        if (a->AsScene()) {
            // Scene-backed: skip the seed-entity dance, unpack the hierarchy.
            for (const auto& b : apps_) {
                if (const PackedScene* scene = b->AsScene()) {
                    return scene->Instantiate(world);
                }
            }
        }
    }
    // Plain component prefab: legacy semantics (one entity, appliers in order).
    Entity e = world.CreateEntity();
    for (auto& a : apps_) {
        a->Apply(world, e);
    }
    return e;
}

inline Prefab Prefab::FromPackedScene(const PackedScene& scene) {
    Prefab p;
    p.apps_.push_back(std::make_unique<SceneApplier>(&scene));
    return p;
}

// Scene-backed applier: unpack the whole hierarchy (nested scenes included).
// The seed entity created by Prefab::Instantiate is discarded — the scene
// root's entity is the meaningful result.
inline Prefab::SceneApplier::SceneApplier(const PackedScene* s) : scene(s) {}

inline void Prefab::SceneApplier::Apply(World& w, Entity seed) const {
    w.DestroyEntity(seed);
    scene->Instantiate(w);
}

inline std::shared_ptr<Prefab::IApplier> Prefab::SceneApplier::Clone() const {
    return std::make_shared<SceneApplier>(scene);
}

// -----------------------------------------------------------------------------
// SceneBuilder — Fluent API for constructing PackedScene (Godot-like)
// -----------------------------------------------------------------------------
// Usage:
//   PackedScene scene = SceneBuilder()
//       .Node("Root")
//           .With<Transform3D>([](auto& t) { t.position = {0,0,0}; })
//           .Node("Child")
//               .With<MeshComponent>([](auto& m) { m.primitive = MeshComponent::QUAD; })
//           .End()
//       .End()
//       .Build();
//
//   // Godot instanced scenes: unpack another PackedScene as a child node
//   PackedScene nested = ...;
//   SceneBuilder()
//       .Node("Root")
//           .Inline(nested, "Instance1", local_override) // shared reference
//           .WithPrefab(quad_prefab, "Quad")             // single-node prefab, spliced copy
//       .Build();
// -----------------------------------------------------------------------------
class SceneBuilder {
public:
    SceneBuilder() : current_parent_(-1) {
        scene_ = std::make_unique<PackedScene>();
    }

    SceneBuilder& Node(const char* name = nullptr, const Transform3D& local = Transform3D{}) {
        int idx = scene_->AddNode(local, current_parent_, name);
        parent_stack_.push_back(current_parent_);
        current_parent_ = idx;
        return *this;
    }

    SceneBuilder& End() {
        if (!parent_stack_.empty()) {
            current_parent_ = parent_stack_.back();
            parent_stack_.pop_back();
        }
        return *this;
    }

    template <class Component, class Fn>
    SceneBuilder& With(Fn&& fn) {
        if (current_parent_ >= 0) {
            scene_->AddComponent<Component>(current_parent_, std::forward<Fn>(fn));
        }
        return *this;
    }

    // Convenience: set transform on current node
    SceneBuilder& Transform(const Transform3D& t) {
        if (current_parent_ >= 0 && current_parent_ < static_cast<int>(scene_->GetNodeCount())) {
            scene_->MutableNode(current_parent_).local_transform = t;
        }
        return *this;
    }

    // Nesting: reference another PackedScene as a child of the current node.
    // Instantiate() unpacks it recursively (analog of Godot instanced scenes).
    // The nested scene is SHARED — it must outlive every Instantiate() of the
    // built scene. `name`/`local` override the nested root's name/transform.
    SceneBuilder& Inline(const PackedScene& nested,
                         const char* name = nullptr,
                         const Transform3D& local = Transform3D{}) {
        scene_->AddNestedScene(nested, local, current_parent_, name);
        return *this;
    }

    // Nesting: unpack a Prefab as a child of the current node (prefab =
    // single-node PackedScene). The prefab's content is COPIED (spliced) into
    // the built scene — no lifetime requirements. The first spliced root gets
    // `name`/`local` overrides when provided.
    // A scene-backed prefab (FromPackedScene) splices its WHOLE scene, so
    // multi-node prefabs land here intact.
    SceneBuilder& WithPrefab(const Prefab& prefab,
                             const char* name = nullptr,
                             const Transform3D& local = Transform3D{}) {
        const int n = static_cast<int>(scene_->GetNodeCount());
        SpliceSceneOntoCurrent(prefab.ToPackedScene());
        const int spliced = static_cast<int>(scene_->GetNodeCount()) - n;
        if (spliced > 0 && name) {
            // The first spliced root is the first node whose parent points at
            // current_parent_ (it was a root in the source scene).
            for (int i = n; i < static_cast<int>(scene_->GetNodeCount()); ++i) {
                const int p = scene_->GetNode(i).parent_index;
                if (p == current_parent_ || p < 0 || p >= static_cast<int>(scene_->GetNodeCount())) {
                    scene_->MutableNode(i).name = name;
                    if (!PackedScene::IsIdentityTransform(local)) {
                        scene_->MutableNode(i).local_transform = local;
                    }
                    break;
                }
            }
        }
        return *this;
    }

    // Nesting: splice another PackedScene directly as a child of the current
    // node (sibling of WithPrefab — takes the scene instead of a Prefab).
    // The content is COPIED — no lifetime requirements on `nested`.
    SceneBuilder& WithScene(const PackedScene& nested,
                            const char* name = nullptr,
                            const Transform3D& local = Transform3D{}) {
        const int n = static_cast<int>(scene_->GetNodeCount());
        scene_->SpliceInto(nested, current_parent_);
        const int spliced = static_cast<int>(scene_->GetNodeCount()) - n;
        if (spliced > 0 && name) {
            for (int i = n; i < static_cast<int>(scene_->GetNodeCount()); ++i) {
                const int p = scene_->GetNode(i).parent_index;
                if (p == current_parent_) {
                    scene_->MutableNode(i).name = name;
                    if (!PackedScene::IsIdentityTransform(local)) {
                        scene_->MutableNode(i).local_transform = local;
                    }
                    break;
                }
            }
        }
        return *this;
    }

    PackedScene Build() {
        PackedScene result = std::move(*scene_);
        scene_ = std::make_unique<PackedScene>();
        current_parent_ = -1;
        parent_stack_.clear();
        return result;
    }

private:
    std::unique_ptr<PackedScene> scene_;
    int current_parent_;
    std::vector<int> parent_stack_;

    // Splice `other` under the current node, normalizing out-of-range parent
    // indices of its roots (an out-of-range parent degrades to root inside the
    // SOURCE scene — here they must attach to current_parent_, or -1 when the
    // builder is at top level).
    void SpliceSceneOntoCurrent(const PackedScene& other) {
        const int n = static_cast<int>(other.GetNodeCount());
        const int base = static_cast<int>(scene_->GetNodeCount());
        for (int i = 0; i < n; ++i) {
            PackedScene::NodeData nd = other.GetNode(i); // copy
            const int oldp = nd.parent_index;
            const bool was_root = (oldp < 0 || oldp >= n);
            nd.parent_index = was_root ? current_parent_ : base + oldp;
            scene_->AddNodeData(std::move(nd));
        }
    }
};
