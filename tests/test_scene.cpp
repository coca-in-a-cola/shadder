// =============================================================================
// test_scene.cpp — native (host) test for the SceneNode hierarchy layer.
//
// Runs on the Linux host with g++ + DirectXMath headers (no D3D needed):
//   g++ -std=c++17 -isystem /opt/directxmath -isystem test/sal_shim \
//       -I src tests/test_scene.cpp -o /tmp/test_scene && /tmp/test_scene
//
// Covers (task SH-P / SceneNode):
//   1. parent/child links (AddChild / RemoveChild / Reparent / Detach)
//   2. pre-order traversal order
//   3. global transform = parent.global * local (Godot-style inheritance)
//   4. dirty-flag invalidation propagates to descendants
//   5. SceneTree helpers (CreateNode, FindByEntity, ForEach, node count)
//   6. SyncToECS writes the global transform into the entity's Transform3D
//   7. cycle guard (adding an ancestor as a child is a no-op)
// =============================================================================
#include "core/scene/SceneTree.h"
#include "core/scene/PackedScene.h"

#include <DirectXMath.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>

static int g_failures = 0;

#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
            ++g_failures;                                                           \
        }                                                                           \
    } while (0)

static bool Near(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) <= eps;
}

static bool PosNear(const DirectX::XMFLOAT3& p, float x, float y, float z, float eps = 1e-4f) {
    return Near(p.x, x, eps) && Near(p.y, y, eps) && Near(p.z, z, eps);
}

int main() {
    using namespace DirectX;

    World world;
    world.RegisterComponent<Transform3D>();

    // -------------------------------------------------------------------------
    // 1) Hierarchy: root -> parent -> (childA, childB)
    // -------------------------------------------------------------------------
    SceneNode::Ptr root = SceneNode::CreateRoot();
    root->SetName("Root");

    Entity eParent = world.CreateEntity();
    SceneNode::Ptr parent = std::make_shared<SceneNode>(eParent);
    parent->SetName("Parent");
    root->AddChild(parent);

    Entity eChildA = world.CreateEntity();
    Entity eChildB = world.CreateEntity();
    world.AddComponent<Transform3D>(eChildA); // used by the SyncToECS check below
    world.AddComponent<Transform3D>(eChildB);
    SceneNode::Ptr childA = std::make_shared<SceneNode>(eChildA);
    childA->SetName("ChildA");
    SceneNode::Ptr childB = std::make_shared<SceneNode>(eChildB);
    childB->SetName("ChildB");
    parent->AddChild(childA);
    parent->AddChild(childB);

    CHECK(parent->GetParent() == root);
    CHECK(childA->GetParent() == parent);
    CHECK(parent->GetChildCount() == 2);
    CHECK(parent->GetChild(0) == childA);
    CHECK(parent->GetChild(1) == childB);
    CHECK(parent->GetChild(5) == nullptr);
    CHECK(root->GetDepth() == 0);
    CHECK(parent->GetDepth() == 1);
    CHECK(childA->GetDepth() == 2);
    CHECK(childA->GetRoot() == root);
    CHECK(root->FindChild("ChildB") == nullptr);          // direct search: not a direct child
    CHECK(root->FindChild("ChildB", true) == childB);     // recursive search finds it
    CHECK(root->FindChild("Nope", true) == nullptr);

    // -------------------------------------------------------------------------
    // 2) Traversal: depth-first pre-order
    // -------------------------------------------------------------------------
    std::vector<std::string> order;
    root->ForEachChild([&](const SceneNode::Ptr& n) { order.push_back(n->GetName()); });
    CHECK(order.size() == 3);
    CHECK(order[0] == "Parent" && order[1] == "ChildA" && order[2] == "ChildB");

    // -------------------------------------------------------------------------
    // 3) Global transform = parent.global * local
    //    parent: translate (10, 20, 0); childA: translate (1, 2, 0) + rotZ 90deg
    //    => childA global pos (11, 22, 0)
    // -------------------------------------------------------------------------
    Transform3D pt;
    pt.position = {10.0f, 20.0f, 0.0f};
    parent->SetLocalTransform(pt);

    Transform3D ct;
    ct.position = {1.0f, 2.0f, 0.0f};
    XMStoreFloat4(&ct.rotation, XMQuaternionRotationRollPitchYaw(0.0f, 0.0f, 1.5707963f));
    childA->SetLocalTransform(ct);

    const Transform3D& pg = parent->GetGlobalTransform();
    CHECK(PosNear(pg.position, 10.0f, 20.0f, 0.0f)); // root is identity container

    const Transform3D& cg = childA->GetGlobalTransform();
    CHECK(PosNear(cg.position, 11.0f, 22.0f, 0.0f));

    // Rotation inheritance: rotate global X axis (1,0,0) by child global rot -> (0,1,0)
    XMVECTOR rotated = XMVector3Transform(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f),
                                          XMMatrixRotationQuaternion(XMLoadFloat4(&cg.rotation)));
    XMFLOAT3 rr;
    XMStoreFloat3(&rr, rotated);
    CHECK(PosNear(rr, 0.0f, 1.0f, 0.0f));

    // -------------------------------------------------------------------------
    // 4) Dirty-flag propagation: move the parent, children must follow
    // -------------------------------------------------------------------------
    pt.position = {100.0f, 200.0f, 0.0f};
    parent->SetLocalTransform(pt);
    const Transform3D& cg2 = childA->GetGlobalTransform();
    CHECK(PosNear(cg2.position, 101.0f, 202.0f, 0.0f));

    // -------------------------------------------------------------------------
    // 5) SceneTree helpers
    // -------------------------------------------------------------------------
    SceneTree tree;
    SceneNode::Ptr tParent = tree.CreateNode(eParent);
    SceneNode::Ptr tChild = tree.CreateNode(eChildA, tParent);
    tChild->SetName("TChild");
    CHECK(tree.GetNodeCount() == 2);
    CHECK(tree.FindByEntity(eChildA) == tChild);
    CHECK(tree.FindByEntity(eChildB) == nullptr);

    size_t visited = 0;
    tree.ForEach([&](const SceneNode::Ptr&) { ++visited; });
    CHECK(visited == 2);

    // -------------------------------------------------------------------------
    // 6) SyncToECS: node global transform lands in the entity's Transform3D
    // -------------------------------------------------------------------------
    tParent->SetLocalTransform(Transform3D{pt});
    Transform3D& ecsTr = *world.GetComponent<Transform3D>(eChildA);
    ecsTr.position = {0.0f, 0.0f, 0.0f}; // simulate stale ECS data
    tChild->SyncSubtreeToECS(world);
    // tChild local is identity (never set), tParent global == (100,200,0)
    CHECK(PosNear(ecsTr.position, 100.0f, 200.0f, 0.0f));

    // -------------------------------------------------------------------------
    // 7) Cycle guard: adding an ancestor as a child must be a no-op
    // -------------------------------------------------------------------------
    size_t before = parent->GetChildCount();
    parent->AddChild(root);       // root is parent's ancestor
    CHECK(parent->GetChildCount() == before);
    childA->Reparent(childB->GetParent()); // no-op (same parent), must not corrupt
    CHECK(childA->GetParent() == parent);

    // Reparent childB under root
    childB->Reparent(root);
    CHECK(childB->GetParent() == root);
    CHECK(parent->GetChildCount() == 1);
    root->RemoveChild(childB);
    CHECK(childB->GetParent() == nullptr);

    // -------------------------------------------------------------------------
    // 8) PackedScene: build -> instantiate -> hierarchy + transforms in World
    // -------------------------------------------------------------------------
    PackedScene packed = SceneBuilder()
                             .Node("PRoot")
                             .Node("PChild")
                             .With<Transform3D>([](Transform3D& t) { t.position = {5.0f, 0.0f, 0.0f}; })
                             .End()
                             .Build();
    // Give the root a transform via the builder's Transform()
    // (instantiate and check instead of relying on builder chaining quirks)
    CHECK(packed.GetNodeCount() == 2);

    SceneNode::Ptr instRoot = nullptr;
    Entity rootEntity = packed.Instantiate(world, &instRoot);
    CHECK(rootEntity == instRoot->GetEntity());
    CHECK(instRoot->GetName() == "PRoot");
    CHECK(instRoot->GetChildCount() == 1);
    CHECK(world.IsAlive(instRoot->GetEntity()));
    CHECK(world.HasComponent<Transform3D>(instRoot->GetEntity()));
    CHECK(instRoot->FindChild("PChild", true) != nullptr);

    // PChild local pos (5,0,0) under identity root -> global (5,0,0)
    SceneNode::Ptr instChild = instRoot->GetChild(0);
    CHECK(PosNear(instChild->GetGlobalTransform().position, 5.0f, 0.0f, 0.0f));

    // -------------------------------------------------------------------------
    if (g_failures == 0) {
        std::printf("ALL SCENE TESTS PASSED\n");
        return 0;
    }
    std::printf("%d CHECK(S) FAILED\n", g_failures);
    return 1;
}
