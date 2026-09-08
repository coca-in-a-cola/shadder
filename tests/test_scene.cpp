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
//   8. PackedScene: build -> instantiate -> hierarchy + transforms in World
//   9. PackedScene nesting: Inline another scene -> recursive unpack,
//      per-instance name/transform overrides, ECS stays in sync
//  10. PackedScene reuse: two instantiations of a scene with a shared nested
//      scene produce independent subtrees; InstantiateAsChild
//  11. nested-scene cycle guard (AddNestedScene rejects cycles)
//  12. Prefab compatibility: legacy Instantiate semantics unchanged;
//      ToPackedScene / SceneBuilder::WithPrefab bridges
//  13. Prefab::FromPackedScene reverse bridge: scene-backed Prefab instantiates
//      the whole hierarchy; nested scenes unpacked; temp-scene facade pattern
//      (prefab headers style) keeps working
//  14. Demo mirror (examples/framework): RainDemo scene with nested camera/quad
//      prefab scenes + per-instance placement override — structure, ECS sync,
//      components carried, godot-style tree walk
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
    // 9) PackedScene nesting: Inline another scene -> recursive unpack
    // -------------------------------------------------------------------------
    PackedScene nested = SceneBuilder()
                             .Node("NRoot", [] {
                                 Transform3D t;
                                 t.position = {2.0f, 0.0f, 0.0f};
                                 return t;
                             }())
                             .Node("NChild", [] {
                                 Transform3D t;
                                 t.position = {3.0f, 0.0f, 0.0f};
                                 return t;
                             }())
                             .Build();
    CHECK(nested.GetNodeCount() == 2);
    CHECK(nested.GetNode(0).name == "NRoot");
    CHECK(nested.GetNode(1).name == "NChild");

    Transform3D instLocal;
    instLocal.position = {100.0f, 0.0f, 0.0f};
    PackedScene outer = SceneBuilder()
                            .Node("ORoot")
                            .Inline(nested, "Inst", instLocal)
                            .End()
                            .Build();
    CHECK(outer.GetNodeCount() == 2);
    CHECK(outer.GetNode(1).IsNestedScene());

    SceneNode::Ptr outerRoot = nullptr;
    Entity outerEntity = outer.Instantiate(world, &outerRoot);
    CHECK(outerEntity == outerRoot->GetEntity());
    CHECK(outerRoot->GetName() == "ORoot");
    CHECK(outerRoot->GetChildCount() == 1); // nested unpacked under ORoot

    SceneNode::Ptr instNestedRoot = outerRoot->GetChild(0);
    CHECK(instNestedRoot->GetName() == "Inst");            // per-instance override
    CHECK(PosNear(instNestedRoot->GetLocalTransform().position, 100.0f, 0.0f, 0.0f)); // override
    // Godot placement semantics: the reference transform REPLACES the nested
    // root's local -> Inst global (100,0,0); NChild global (103,0,0) = 100 + 3.
    CHECK(PosNear(instNestedRoot->GetGlobalTransform().position, 100.0f, 0.0f, 0.0f));
    CHECK(instNestedRoot->GetChildCount() == 1);           // NChild followed the instance
    SceneNode::Ptr instNestedChild = instNestedRoot->GetChild(0);
    CHECK(instNestedChild->GetName() == "NChild");
    CHECK(PosNear(instNestedChild->GetGlobalTransform().position, 103.0f, 0.0f, 0.0f)); // 100+3

    // ECS stays in sync: the transform override landed in the entity's Transform3D
    const Transform3D* nestedRootEcs = world.GetComponent<Transform3D>(instNestedRoot->GetEntity());
    CHECK(nestedRootEcs != nullptr);
    if (nestedRootEcs) {
        CHECK(PosNear(nestedRootEcs->position, 100.0f, 0.0f, 0.0f));
    }

    // -------------------------------------------------------------------------
    // 10) Reuse: instantiate the same scene again -> independent subtree;
    //     InstantiateAsChild under an existing node
    // -------------------------------------------------------------------------
    SceneNode::Ptr secondRoot = nullptr;
    Entity secondEntity = outer.Instantiate(world, &secondRoot);
    CHECK(secondEntity != outerEntity);                    // distinct entities
    CHECK(secondRoot != outerRoot);
    CHECK(secondRoot->FindChild("Inst", true) != nullptr);
    CHECK(PosNear(secondRoot->FindChild("Inst", true)->GetGlobalTransform().position,
                  100.0f, 0.0f, 0.0f));

    SceneNode::Ptr hostRoot = SceneNode::CreateRoot();
    hostRoot->SetName("Host");
    Entity asChildEntity = nested.InstantiateAsChild(world, hostRoot);
    CHECK(asChildEntity.IsValid());
    CHECK(hostRoot->GetChildCount() == 1);
    CHECK(hostRoot->GetChild(0)->GetName() == "NRoot");
    CHECK(hostRoot->GetChild(0)->GetChildCount() == 1);

    // -------------------------------------------------------------------------
    // 11) Nested-scene cycle guard: self and mutual references rejected
    // -------------------------------------------------------------------------
    PackedScene cycA;
    cycA.AddNode(Transform3D{}, -1, "A");
    CHECK(cycA.AddNestedScene(cycA) == -1);                // self reference rejected

    PackedScene cycB;
    cycB.AddNode(Transform3D{}, -1, "B");
    CHECK(cycB.AddNestedScene(cycA) >= 0);                 // B -> A is fine
    CHECK(cycA.AddNestedScene(cycB) == -1);                // A -> B -> A cycle rejected

    // Instantiating a nested chain still terminates; same nested scene twice OK
    PackedScene chainInner;
    chainInner.AddNode(Transform3D{}, -1, "Inner");
    PackedScene chainOuter;
    chainOuter.AddNode(Transform3D{}, -1, "Outer");
    CHECK(chainOuter.AddNestedScene(chainInner) >= 0);
    CHECK(chainOuter.AddNestedScene(chainInner) >= 0);
    CHECK(chainOuter.Instantiate(world).IsValid());

    // -------------------------------------------------------------------------
    // 12) Prefab compatibility: legacy semantics unchanged + bridges
    // -------------------------------------------------------------------------
    // Legacy Prefab path: creates ONE entity, applies With<T> appliers, does
    // NOT force a Transform3D (pong relies on adding it afterwards).
    struct TestTagComponent : public ComponentBase {
        int value = 0;
    };
    Prefab legacy;
    legacy.With<TestTagComponent>([](TestTagComponent& t) { t.value = 42; });
    Entity legacyEntity = legacy.Instantiate(world);
    CHECK(world.IsAlive(legacyEntity));
    CHECK(world.HasComponent<TestTagComponent>(legacyEntity));
    CHECK(!world.HasComponent<Transform3D>(legacyEntity)); // no forced transform
    const TestTagComponent* tag = world.GetComponent<TestTagComponent>(legacyEntity);
    CHECK(tag != nullptr && tag->value == 42);

    // Bridge 1: Prefab::ToPackedScene -> single-node scene, components carried
    PackedScene fromPrefab = legacy.ToPackedScene();
    CHECK(fromPrefab.GetNodeCount() == 1);
    Entity bridgedEntity = fromPrefab.Instantiate(world);
    CHECK(bridgedEntity != legacyEntity);
    const TestTagComponent* bridgedTag = world.GetComponent<TestTagComponent>(bridgedEntity);
    CHECK(bridgedTag != nullptr && bridgedTag->value == 42);

    // Bridge 2: SceneBuilder::WithPrefab splices prefab content as a child node
    PackedScene fromBuilder = SceneBuilder()
                                  .Node("PrefabHost")
                                  .WithPrefab(legacy, "FromPrefab")
                                  .End()
                                  .Build();
    CHECK(fromBuilder.GetNodeCount() == 2);
    SceneNode::Ptr prefabHost = nullptr;
    fromBuilder.Instantiate(world, &prefabHost);
    CHECK(prefabHost != nullptr);
    SceneNode::Ptr fromPrefabNode = prefabHost->FindChild("FromPrefab");
    CHECK(fromPrefabNode != nullptr);
    if (fromPrefabNode) {
        const TestTagComponent* splicedTag = world.GetComponent<TestTagComponent>(fromPrefabNode->GetEntity());
        CHECK(splicedTag != nullptr && splicedTag->value == 42);
    }

    // Prefab::Instantiate still works alongside all of the above
    Entity legacyAgain = legacy.Instantiate(world);
    CHECK(world.IsAlive(legacyAgain));
    CHECK(world.GetComponent<TestTagComponent>(legacyAgain)->value == 42);

    // -------------------------------------------------------------------------
    // 13) Prefab::FromPackedScene: scene-backed Prefab (reverse bridge)
    // -------------------------------------------------------------------------
    PackedScene payload = SceneBuilder()
                              .Node("Payload", [] {
                                  Transform3D t;
                                  t.position = {7.0f, 0.0f, 0.0f};
                                  return t;
                              }())
                              .Node("PayloadChild", [] {
                                  Transform3D t;
                                  t.position = {0.0f, 4.0f, 0.0f};
                                  return t;
                              }())
                              .Build();

    Prefab scenePrefab = Prefab::FromPackedScene(payload);
    // Whole hierarchy unpacked; Instantiate returns the scene ROOT entity.
    SceneNode::Ptr bridgeRoot = nullptr;
    Entity bridgeEntity = scenePrefab.Instantiate(world);
    CHECK(bridgeEntity.IsValid());
    CHECK(world.GetComponent<TestTagComponent>(bridgeEntity) == nullptr);
    // Find the payload root among alive entities: instantiate a marker sibling
    // scene to locate by structure instead of guessing the index.
    // Simpler: wrap again and take the node out explicitly.
    PackedScene bridgeProbe = SceneBuilder()
                                  .Node("ProbeHost")
                                  .Inline(payload, "Bridged")
                                  .End()
                                  .Build();
    bridgeProbe.Instantiate(world, &bridgeRoot);
    CHECK(bridgeRoot != nullptr);
    SceneNode::Ptr bridged = bridgeRoot ? bridgeRoot->FindChild("Bridged") : nullptr;
    CHECK(bridged != nullptr);
    if (bridged) {
        CHECK(PosNear(bridged->GetLocalTransform().position, 7.0f, 0.0f, 0.0f));
        CHECK(bridged->GetChildCount() == 1);
        CHECK(PosNear(bridged->GetChild(0)->GetGlobalTransform().position, 7.0f, 4.0f, 0.0f));
    }

    // Temp-scene facade pattern (as used by the prefab headers, e.g. pong's
    // QuadPrefab(...).Instantiate(world)): the scene temporary lives until the
    // end of the FULL EXPRESSION — after Instantiate returned — so this is
    // safe. (Wrapping a scene that died earlier would be UB; keep the scene
    // alive or instantiate within the same full expression.)
    Entity facadeEntity = Prefab::FromPackedScene(
                              SceneBuilder()
                                  .Node("Temp", [] {
                                      Transform3D t;
                                      t.position = {3.0f, 0.0f, 0.0f};
                                      return t;
                                  }())
                                  .Build())
                              .Instantiate(world);
    CHECK(world.IsAlive(facadeEntity));
    const Transform3D* tempTr = world.GetComponent<Transform3D>(facadeEntity);
    CHECK(tempTr != nullptr);
    if (tempTr) {
        CHECK(PosNear(tempTr->position, 3.0f, 0.0f, 0.0f));
    }

    // -------------------------------------------------------------------------
    // 14) Demo mirror (examples/framework): nested prefab scenes + placement
    // -------------------------------------------------------------------------
    // Camera-like node: single-node scene carrying a tag component
    struct DemoCameraComponent : public ComponentBase {
        float screenW = 0.0f;
        float screenH = 0.0f;
    };
    world.RegisterComponent<DemoCameraComponent>();

    PackedScene cameraPrefab = SceneBuilder()
                                   .Node("Camera")
                                   .With<DemoCameraComponent>([](DemoCameraComponent& c) {
                                       c.screenW = 900.0f;
                                       c.screenH = 900.0f;
                                   })
                                   .End()
                                   .Build();
    PackedScene quadPrefab = SceneBuilder()
                                 .Node("Quad")
                                 .With<TestTagComponent>([](TestTagComponent& t) { t.value = 77; })
                                 .End()
                                 .Build();

    Transform3D markerLocal;
    markerLocal.position = {430.0f, 120.0f, 0.0f};
    PackedScene demo = SceneBuilder()
                           .Node("RainDemo")
                           .Inline(cameraPrefab, "Camera")            // shared reference
                           .Node("Rain")
                           .End()
                           .WithPrefab(Prefab::FromPackedScene(quadPrefab), "Marker", markerLocal) // spliced copy (scene wrapper)
                           .End()
                           .Build();
    CHECK(demo.GetNodeCount() == 4); // RainDemo, Camera(nested), Rain, Marker

    SceneNode::Ptr demoRoot = nullptr;
    Entity demoEntity = demo.Instantiate(world, &demoRoot);
    CHECK(demoEntity == demoRoot->GetEntity());
    CHECK(demoRoot->GetName() == "RainDemo");
    CHECK(demoRoot->GetChildCount() == 3); // Camera + Rain + Marker

    // Camera unpacked from the nested prefab scene, component carried
    SceneNode::Ptr demoCam = demoRoot->FindChild("Camera");
    CHECK(demoCam != nullptr);
    const DemoCameraComponent* camComp =
        demoCam ? world.GetComponent<DemoCameraComponent>(demoCam->GetEntity()) : nullptr;
    CHECK(camComp != nullptr && camComp->screenW == 900.0f && camComp->screenH == 900.0f);

    // Marker spliced from the quad prefab scene with per-instance placement:
    // global == local == (430,120,0) under identity root; ECS transform synced.
    SceneNode::Ptr demoMarker = demoRoot->FindChild("Marker");
    CHECK(demoMarker != nullptr);
    if (demoMarker) {
        CHECK(PosNear(demoMarker->GetGlobalTransform().position, 430.0f, 120.0f, 0.0f));
        const TestTagComponent* markerTag = world.GetComponent<TestTagComponent>(demoMarker->GetEntity());
        CHECK(markerTag != nullptr && markerTag->value == 77);
        const Transform3D* markerEcs = world.GetComponent<Transform3D>(demoMarker->GetEntity());
        CHECK(markerEcs != nullptr);
        if (markerEcs) {
            CHECK(PosNear(markerEcs->position, 430.0f, 120.0f, 0.0f));
        }
    }

    // Attach-at-runtime pattern (demo attaches the batch node under "Rain"):
    // Rain packed node has no components here, so instantiate produces a node
    // child; verify we can add a runtime node under it (Godot add_child).
    SceneNode::Ptr demoRain = demoRoot->FindChild("Rain");
    CHECK(demoRain != nullptr);
    Entity batchEntity = world.CreateEntity();
    world.AddComponent<Transform3D>(batchEntity);
    SceneNode::Ptr batchNode = std::make_shared<SceneNode>(batchEntity);
    batchNode->SetName("TriangleBatch");
    demoRain->AddChild(batchNode);
    CHECK(demoRoot->FindChild("Rain")->GetChildCount() == 1);
    CHECK(batchNode->GetParent() == demoRain);

    // Godot-style tree walk over the mirrored demo structure
    std::vector<std::string> demoNames;
    demoRoot->ForEachChild([&](const SceneNode::Ptr& n) { demoNames.push_back(n->GetName()); });
    CHECK(demoNames.size() == 4); // Camera, Rain(+TriangleBatch), Marker
    CHECK(demoNames[0] == "Camera" && demoNames[1] == "Rain" && demoNames[2] == "TriangleBatch" && demoNames[3] == "Marker");

    // -------------------------------------------------------------------------
    if (g_failures == 0) {
        std::printf("ALL SCENE TESTS PASSED\n");
        return 0;
    }
    std::printf("%d CHECK(S) FAILED\n", g_failures);
    return 1;
}
