#pragma once

#include "SceneNode.h"
#include "core/ecs/World.h"
#include <vector>
#include <memory>

// -----------------------------------------------------------------------------
// SceneTree — root holder for a scene hierarchy
// -----------------------------------------------------------------------------
// Manages the root SceneNode and provides convenient API for tree operations.
// -----------------------------------------------------------------------------
class SceneTree {
public:
    SceneTree() : root_(SceneNode::CreateRoot()) {}

    SceneNode::Ptr GetRoot() const { return root_; }

    // Create a new node and add it to root (or specified parent)
    SceneNode::Ptr CreateNode(Entity entity = Entity{INVALID_ENTITY_INDEX, INVALID_GENERATION},
                               SceneNode::Ptr parent = nullptr) {
        auto node = std::make_shared<SceneNode>(entity);
        if (parent) {
            parent->AddChild(node);
        } else {
            root_->AddChild(node);
        }
        return node;
    }

    // Remove a node and all its children from the tree
    void RemoveNode(SceneNode::Ptr node) {
        node->Detach();
    }

    // Sync entire tree transforms to ECS
    void SyncToECS(World& world) const {
        root_->SyncSubtreeToECS(world);
    }

    // Traverse all nodes (depth-first pre-order)
    template <typename Fn>
    void ForEach(Fn&& fn) const {
        root_->ForEachChild([&](SceneNode::Ptr child) {
            fn(child);
        });
    }

    // Find node by entity
    SceneNode::Ptr FindByEntity(Entity entity) const {
        SceneNode::Ptr result = nullptr;
        ForEach([&](SceneNode::Ptr node) {
            if (node->GetEntity() == entity) {
                result = node;
            }
        });
        return result;
    }

    // Get total node count
    size_t GetNodeCount() const {
        size_t count = 0;
        ForEach([&](SceneNode::Ptr) { ++count; });
        return count;
    }

private:
    SceneNode::Ptr root_;
};