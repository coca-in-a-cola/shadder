#pragma once

#include "core/ecs/World.h"
#include "core/ecs/EcsTypes.h"
#include "core/prefab/Prefab.h"
#include "core/scene/SceneNode.h"
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>

// -----------------------------------------------------------------------------
// PackedScene — Godot-style serializable scene description
// -----------------------------------------------------------------------------
// A PackedScene is a serializable scene structure that can be instantiated
// into a World multiple times. It stores the hierarchy and component data
// needed to recreate the scene.
// 
// In Godot: PackedScene is the serialized form; Scene is the instanced version.
// Here: PackedScene is the template; Instantiate() creates nodes/entities in World.
// -----------------------------------------------------------------------------
class PackedScene {
public:
    // Component initializer function type
    using ComponentInitFn = std::function<void(World&, Entity)>;

    struct NodeData {
        Entity entity{INVALID_ENTITY_INDEX, INVALID_GENERATION}; // placeholder, assigned on instantiate
        std::string name; // optional name for finding nodes
        Transform3D local_transform{};
        std::vector<ComponentInitFn> component_inits;
        std::vector<NodeData> children;
        
        // Parent index in the flat array (-1 for root)
        int parent_index = -1;
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
        
        // Update parent's children indices if needed (for traversal)
        if (parent_index >= 0 && parent_index < index) {
            // Children are tracked implicitly via parent_index
        }
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

    // Convenience: add component from Prefab's With<T> pattern
    template <class Component, class Fn>
    void With(int node_index, Fn&& init_fn) {
        AddComponent<Component>(node_index, std::forward<Fn>(init_fn));
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

        // First pass: create all entities
        std::vector<Entity> entities(nodes_.size());
        for (size_t i = 0; i < nodes_.size(); ++i) {
            entities[i] = world.CreateEntity();
            nodes_[i].entity = entities[i]; // store for child refs (not serialized)
        }

        // Second pass: add components
        for (size_t i = 0; i < nodes_.size(); ++i) {
            Entity e = entities[i];
            for (const auto& init : nodes_[i].component_inits) {
                init(world, e);
            }
            // Apply local transform to entity's Transform3D
            world.RegisterComponent<Transform3D>();
            auto& tr = world.AddComponent<Transform3D>(e);
            tr = nodes_[i].local_transform;
        }

        // Third pass: build SceneNode hierarchy mirroring the instance
        std::vector<SceneNode::Ptr> scene_nodes(nodes_.size());
        for (size_t i = 0; i < nodes_.size(); ++i) {
            scene_nodes[i] = std::make_shared<SceneNode>(entities[i]);
            scene_nodes[i]->SetLocalTransform(nodes_[i].local_transform);
        }

        // Link hierarchy
        SceneNode::Ptr root_node = nullptr;
        for (size_t i = 0; i < nodes_.size(); ++i) {
            int parent_idx = nodes_[i].parent_index;
            if (parent_idx >= 0 && parent_idx < static_cast<int>(nodes_.size())) {
                scene_nodes[parent_idx]->AddChild(scene_nodes[i]);
            } else {
                root_node = scene_nodes[i];
            }
        }

        if (output_root_node) {
            *output_root_node = root_node;
        }

        return root_node ? root_node->GetEntity() : Entity{INVALID_ENTITY_INDEX, INVALID_GENERATION};
    }

    // Instantiate as child of existing SceneNode
    Entity InstantiateAsChild(World& world, SceneNode::Ptr parent) const {
        if (nodes_.empty()) {
            return Entity{INVALID_ENTITY_INDEX, INVALID_GENERATION};
        }

        // Create entities and components (same as above)
        std::vector<Entity> entities(nodes_.size());
        for (size_t i = 0; i < nodes_.size(); ++i) {
            entities[i] = world.CreateEntity();
            for (const auto& init : nodes_[i].component_inits) {
                init(world, entities[i]);
            }
            world.RegisterComponent<Transform3D>();
            auto& tr = world.AddComponent<Transform3D>(entities[i]);
            tr = nodes_[i].local_transform;
        }

        // Build SceneNode hierarchy
        std::vector<SceneNode::Ptr> scene_nodes(nodes_.size());
        for (size_t i = 0; i < nodes_.size(); ++i) {
            scene_nodes[i] = std::make_shared<SceneNode>(entities[i]);
            scene_nodes[i]->SetLocalTransform(nodes_[i].local_transform);
        }

        SceneNode::Ptr first_child = nullptr;
        for (size_t i = 0; i < nodes_.size(); ++i) {
            int parent_idx = nodes_[i].parent_index;
            if (parent_idx >= 0 && parent_idx < static_cast<int>(nodes_.size())) {
                scene_nodes[parent_idx]->AddChild(scene_nodes[i]);
            } else {
                // This is a root of the packed scene - attach to given parent
                parent->AddChild(scene_nodes[i]);
                if (!first_child) first_child = scene_nodes[i];
            }
        }

        return first_child ? first_child->GetEntity() : Entity{INVALID_ENTITY_INDEX, INVALID_GENERATION};
    }

    // Get node count
    size_t GetNodeCount() const { return nodes_.size(); }

    // Access node data for inspection
    const NodeData& GetNode(size_t index) const { return nodes_[index]; }

private:
    std::vector<NodeData> nodes_;
};

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
            // Can't modify after creation easily, would need mutable access
            // For now, set via With<Transform3D>
            scene_->nodes_[current_parent_].local_transform = t;
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
};