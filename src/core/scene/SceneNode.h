#pragma once

#include "core/ecs/World.h"
#include "framework/modules/transform/Transform3D.h"
#include <DirectXMath.h>
#include <vector>
#include <memory>
#include <algorithm>

// Forward declaration
class SceneTree;

// -----------------------------------------------------------------------------
// SceneNode — Godot-style scene tree node
// -----------------------------------------------------------------------------
// Each node wraps an ECS Entity and maintains a parent/children hierarchy.
// Transform propagation follows Godot: global_transform = parent.global * local
// SceneNode is the HIERARCHY layer on top of ECS (Node3D over nodes in Godot terms).
// ECS remains the source of truth for components and systems.
// -----------------------------------------------------------------------------
class SceneNode : public std::enable_shared_from_this<SceneNode> {
public:
    using Ptr = std::shared_ptr<SceneNode>;
    using WeakPtr = std::weak_ptr<SceneNode>;

    // Create a root node (no entity, just a container)
    static Ptr CreateRoot() {
        return std::make_shared<SceneNode>(Entity{INVALID_ENTITY_INDEX, INVALID_GENERATION});
    }

    // Create a node with an entity
    explicit SceneNode(Entity entity) : entity_(entity) {}

    // Non-copyable, movable
    SceneNode(const SceneNode&) = delete;
    SceneNode& operator=(const SceneNode&) = delete;
    SceneNode(SceneNode&&) = default;
    SceneNode& operator=(SceneNode&&) = default;

    // -------------------------------------------------------------------------
    // Hierarchy management
    // -------------------------------------------------------------------------
    void AddChild(Ptr child) {
        if (!child) return;
        if (child->parent_.lock()) {
            child->parent_.lock()->RemoveChild(child);
        }
        child->parent_ = shared_from_this();
        children_.push_back(child);
        child->MarkTransformDirty();
    }

    void RemoveChild(Ptr child) {
        auto it = std::find(children_.begin(), children_.end(), child);
        if (it != children_.end()) {
            (*it)->parent_.reset();
            children_.erase(it);
            child->MarkTransformDirty();
        }
    }

    Ptr GetParent() const { return parent_.lock(); }
    const std::vector<Ptr>& GetChildren() const { return children_; }
    size_t GetChildCount() const { return children_.size(); }
    Ptr GetChild(size_t index) const { return (index < children_.size()) ? children_[index] : nullptr; }

    // Find child by name (if entity has NameComponent - future extension)
    Ptr FindChild(const char* /*name*/) const { return nullptr; }

    // -------------------------------------------------------------------------
    // Entity access
    // -------------------------------------------------------------------------
    Entity GetEntity() const { return entity_; }
    void SetEntity(Entity e) { entity_ = e; }
    bool HasEntity() const { return entity_.IsValid(); }

    // -------------------------------------------------------------------------
    // Transform — local (relative to parent)
    // -------------------------------------------------------------------------
    DirectX::XMFLOAT3& LocalPosition() { MarkTransformDirty(); return local_.position; }
    DirectX::XMFLOAT4& LocalRotation() { MarkTransformDirty(); return local_.rotation; }
    DirectX::XMFLOAT3& LocalScale()    { MarkTransformDirty(); return local_.scale; }

    const DirectX::XMFLOAT3& LocalPosition() const { return local_.position; }
    const DirectX::XMFLOAT4& LocalRotation() const { return local_.rotation; }
    const DirectX::XMFLOAT3& LocalScale()    const { return local_.scale; }

    // Local transform as Transform3D struct (for convenience)
    Transform3D GetLocalTransform() const { return local_; }
    void SetLocalTransform(const Transform3D& t) { local_ = t; MarkTransformDirty(); }

    // -------------------------------------------------------------------------
    // Global transform (world space) — computed from hierarchy
    // -------------------------------------------------------------------------
    // Global = Parent.Global * Local (like Godot)
    const Transform3D& GetGlobalTransform() const {
        if (transform_dirty_) {
            UpdateGlobalTransform();
        }
        return global_;
    }

    // Force recompute (call after hierarchy changes)
    void MarkTransformDirty() {
        if (!transform_dirty_) {
            transform_dirty_ = true;
            for (auto& child : children_) {
                child->MarkTransformDirty();
            }
        }
    }

    // -------------------------------------------------------------------------
    // Tree operations
    // -------------------------------------------------------------------------
    // Reparent to new parent (removes from old parent automatically)
    void Reparent(Ptr new_parent) {
        if (new_parent == shared_from_this()) return;
        if (auto old = parent_.lock()) {
            old->RemoveChild(shared_from_this());
        }
        new_parent->AddChild(shared_from_this());
    }

    // Detach from parent (becomes root of its own subtree)
    void Detach() {
        if (auto p = parent_.lock()) {
            p->RemoveChild(shared_from_this());
        }
    }

    // Get root of this tree
    Ptr GetRoot() {
        Ptr current = shared_from_this();
        while (auto p = current->parent_.lock()) {
            current = p;
        }
        return current;
    }

    // Depth in tree (root = 0)
    int GetDepth() const {
        int depth = 0;
        Ptr current = shared_from_this();
        while (auto p = current->parent_.lock()) {
            ++depth;
            current = p;
        }
        return depth;
    }

    // -------------------------------------------------------------------------
    // Traversal (Godot-style: depth-first, pre-order)
    // -------------------------------------------------------------------------
    template <typename Fn>
    void ForEachChild(Fn&& fn) {
        for (auto& child : children_) {
            fn(child);
            child->ForEachChild(std::forward<Fn>(fn));
        }
    }

    template <typename Fn>
    void ForEachChild(Fn&& fn) const {
        for (const auto& child : children_) {
            fn(child);
            child->ForEachChild(std::forward<Fn>(fn));
        }
    }

    // -------------------------------------------------------------------------
    // Sync with ECS: write global transform to entity's Transform3D component
    // Call this before rendering/systems that need world transforms
    // -------------------------------------------------------------------------
    void SyncToECS(World& world) const {
        if (!entity_.IsValid()) return;
        auto* tr = world.GetComponent<Transform3D>(entity_);
        if (tr) {
            *tr = GetGlobalTransform();
        }
    }

    // Sync entire subtree
    void SyncSubtreeToECS(World& world) const {
        SyncToECS(world);
        for (const auto& child : children_) {
            child->SyncSubtreeToECS(world);
        }
    }

private:
    // Recalculate global transform from parent
    void UpdateGlobalTransform() const {
        if (auto parent = parent_.lock()) {
            // Global = Parent.Global * Local
            const Transform3D& parent_global = parent->GetGlobalTransform();
            global_ = MultiplyTransforms(parent_global, local_);
        } else {
            global_ = local_;
        }
        transform_dirty_ = false;
    }

    // Matrix multiplication: parent * child (like Godot)
    static Transform3D MultiplyTransforms(const Transform3D& parent, const Transform3D& child) {
        using namespace DirectX;

        // Parent matrix
        XMMATRIX p_scale = XMMatrixScalingFromVector(XMLoadFloat3(&parent.scale));
        XMMATRIX p_rot = XMMatrixRotationQuaternion(XMLoadFloat4(&parent.rotation));
        XMMATRIX p_trans = XMMatrixTranslationFromVector(XMLoadFloat3(&parent.position));
        XMMATRIX p_mat = p_scale * p_rot * p_trans;

        // Child matrix
        XMMATRIX c_scale = XMMatrixScalingFromVector(XMLoadFloat3(&child.scale));
        XMMATRIX c_rot = XMMatrixRotationQuaternion(XMLoadFloat4(&child.rotation));
        XMMATRIX c_trans = XMMatrixTranslationFromVector(XMLoadFloat3(&child.position));
        XMMATRIX c_mat = c_scale * c_rot * c_trans;

        // Combined: parent * child
        XMMATRIX result = p_mat * c_mat;

        // Decompose back to TRS
        Transform3D out;
        XMVECTOR scale, rot, trans;
        XMMatrixDecompose(&scale, &rot, &trans, result);
        XMStoreFloat3(&out.scale, scale);
        XMStoreFloat4(&out.rotation, rot);
        XMStoreFloat3(&out.position, trans);
        return out;
    }

    Entity entity_{INVALID_ENTITY_INDEX, INVALID_GENERATION};
    WeakPtr parent_;
    std::vector<Ptr> children_;

    // Local transform (relative to parent)
    Transform3D local_{};

    // Cached global transform (world space)
    mutable Transform3D global_{};
    mutable bool transform_dirty_ = true;
};