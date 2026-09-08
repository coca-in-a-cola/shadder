#pragma once

#include "core/ecs/World.h"
#include "framework/modules/transform/Transform3D.h"
#include <DirectXMath.h>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>

// Forward declaration
class SceneTree;

// -----------------------------------------------------------------------------
// SceneNode — Godot-style scene tree node
// -----------------------------------------------------------------------------
// Each node wraps an ECS Entity and maintains a parent/children hierarchy.
// Transform propagation follows Godot: global = parent.global * local.
// SceneNode is the HIERARCHY layer on top of ECS (analog of Node3D in Godot):
// ECS remains the source of truth for components and systems; the node layer
// only owns the parent/child graph and transform inheritance.
// -----------------------------------------------------------------------------
class SceneNode : public std::enable_shared_from_this<SceneNode> {
public:
    using Ptr = std::shared_ptr<SceneNode>;
    using WeakPtr = std::weak_ptr<SceneNode>;

    // Create a root node (no entity — pure container, analog of the viewport root)
    static Ptr CreateRoot() {
        return std::make_shared<SceneNode>(Entity{INVALID_ENTITY_INDEX, INVALID_GENERATION});
    }

    // Create a node wrapping an existing entity
    explicit SceneNode(Entity entity) : entity_(entity) {}

    // Non-copyable
    SceneNode(const SceneNode&) = delete;
    SceneNode& operator=(const SceneNode&) = delete;

    // -------------------------------------------------------------------------
    // Hierarchy management
    // -------------------------------------------------------------------------
    void AddChild(Ptr child) {
        if (!child || child.get() == this) return;
        // Cycle guard: a node cannot become a descendant of its own subtree.
        for (Ptr ancestor = shared_from_this(); ancestor; ancestor = ancestor->GetParent()) {
            if (ancestor == child) return;
        }
        if (auto old = child->parent_.lock()) {
            old->RemoveChild(child);
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

    // Find a direct child by name (recursive=true searches the whole subtree)
    Ptr FindChild(const std::string& name, bool recursive = false) const {
        for (const auto& child : children_) {
            if (child->name_ == name) return child;
            if (recursive) {
                if (Ptr found = child->FindChild(name, true)) return found;
            }
        }
        return nullptr;
    }

    // -------------------------------------------------------------------------
    // Entity access — entity <-> node binding
    // -------------------------------------------------------------------------
    Entity GetEntity() const { return entity_; }
    void SetEntity(Entity e) { entity_ = e; }
    bool HasEntity() const { return entity_.IsValid(); }

    // -------------------------------------------------------------------------
    // Naming (Godot-style; used by FindChild)
    // -------------------------------------------------------------------------
    const std::string& GetName() const { return name_; }
    void SetName(const std::string& name) { name_ = name; }

    // -------------------------------------------------------------------------
    // Transform — local (relative to parent)
    // -------------------------------------------------------------------------
    void LocalPosition(const DirectX::XMFLOAT3& v) { local_.position = v; MarkTransformDirty(); }
    void LocalRotation(const DirectX::XMFLOAT4& q) { local_.rotation = q; MarkTransformDirty(); }
    void LocalScale(const DirectX::XMFLOAT3& v)    { local_.scale = v; MarkTransformDirty(); }

    const DirectX::XMFLOAT3& LocalPosition() const { return local_.position; }
    const DirectX::XMFLOAT4& LocalRotation() const { return local_.rotation; }
    const DirectX::XMFLOAT3& LocalScale()    const { return local_.scale; }

    // Local transform as Transform3D struct
    const Transform3D& GetLocalTransform() const { return local_; }
    void SetLocalTransform(const Transform3D& t) { local_ = t; MarkTransformDirty(); }

    // -------------------------------------------------------------------------
    // Global transform (world space) — computed from the parent chain
    // -------------------------------------------------------------------------
    // Global = Parent.Global * Local (like Godot global_transform)
    const Transform3D& GetGlobalTransform() const {
        if (transform_dirty_) {
            UpdateGlobalTransform();
        }
        return global_;
    }

    // Force recompute on next access (called automatically on hierarchy/transform changes)
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
    // Reparent to a new parent (removes from the old parent automatically)
    void Reparent(Ptr new_parent) {
        if (!new_parent || new_parent.get() == this) return;
        // Cycle guard: cannot reparent under own descendant
        for (Ptr ancestor = new_parent; ancestor; ancestor = ancestor->GetParent()) {
            if (ancestor.get() == this) return;
        }
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

    // Root of this tree
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
        Ptr current = std::const_pointer_cast<SceneNode>(shared_from_this());
        while (auto p = current->parent_.lock()) {
            ++depth;
            current = p;
        }
        return depth;
    }

    // -------------------------------------------------------------------------
    // Traversal (depth-first pre-order: node, then children left-to-right)
    // -------------------------------------------------------------------------
    template <typename Fn>
    void ForEachChild(Fn&& fn) {
        for (auto& child : children_) {
            fn(child);
            child->ForEachChild(fn);
        }
    }

    template <typename Fn>
    void ForEachChild(Fn&& fn) const {
        for (const auto& child : children_) {
            fn(child);
            child->ForEachChild(fn);
        }
    }

    // -------------------------------------------------------------------------
    // Sync with ECS: write global transform into the entity's Transform3D
    // component. Call before systems/render that expect world transforms.
    // ECS stays the runtime data plane; the node layer feeds it.
    // -------------------------------------------------------------------------
    void SyncToECS(World& world) const {
        if (!entity_.IsValid()) return;
        auto* tr = world.GetComponent<Transform3D>(entity_);
        if (tr) {
            *tr = GetGlobalTransform();
        }
    }

    // Sync the entire subtree (pre-order)
    void SyncSubtreeToECS(World& world) const {
        SyncToECS(world);
        for (const auto& child : children_) {
            child->SyncSubtreeToECS(world);
        }
    }

private:
    // Recalculate global transform from the parent chain (lazy, cached)
    void UpdateGlobalTransform() const {
        if (auto parent = parent_.lock()) {
            const Transform3D& parent_global = parent->GetGlobalTransform();
            global_ = MultiplyTransforms(local_, parent_global);
        } else {
            global_ = local_;
        }
        transform_dirty_ = false;
    }

    // Compose global = local * parent (DirectXMath row-vector convention:
    // v' = v * M, the leftmost factor applies first — local first, then parent).
    // Same convention as InstancedRenderSystem (world = S * R * T).
    static Transform3D MultiplyTransforms(const Transform3D& local, const Transform3D& parent) {
        using namespace DirectX;

        XMMATRIX l_mat = XMMatrixScalingFromVector(XMLoadFloat3(&local.scale)) *
                         XMMatrixRotationQuaternion(XMLoadFloat4(&local.rotation)) *
                         XMMatrixTranslationFromVector(XMLoadFloat3(&local.position));
        XMMATRIX p_mat = XMMatrixScalingFromVector(XMLoadFloat3(&parent.scale)) *
                         XMMatrixRotationQuaternion(XMLoadFloat4(&parent.rotation)) *
                         XMMatrixTranslationFromVector(XMLoadFloat3(&parent.position));
        XMMATRIX result = l_mat * p_mat;

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
    std::string name_;

    // Local transform (relative to parent)
    Transform3D local_{};

    // Cached global transform (world space), lazy evaluation
    mutable Transform3D global_{};
    mutable bool transform_dirty_ = true;
};
