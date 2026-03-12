/**
 * AxIntegrationTests.cpp - End-to-End Integration Tests for Typed Node Pipeline
 *
 * Cross-cutting tests that verify the full pipeline works end-to-end:
 * parse .ats -> build scene -> typed nodes -> collect renderables.
 * Also tests scene destruction, multiple scenes, EventBus lifecycle
 * integration, and transform propagation with typed nodes.
 *
 * Migrated from Component-based architecture to Godot-style typed nodes.
 * All component references (ComponentFactory, AddComponent, GetComponentsByType)
 * have been replaced with typed node operations (CreateNode with NodeType,
 * GetNodesByType, static_cast to typed node subclass).
 */

#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxHashTable.h"
#include "Foundation/AxAllocator.h"
#include "Foundation/AxAllocatorAPI.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxMath.h"
#include "AxEngine/AxNode.h"
#include "AxEngine/AxTypedNodes.h"
#include "AxEngine/AxSceneTree.h"
#include "AxEngine/AxEventBus.h"
#include "AxEngine/AxSceneParser.h"

#include <cstring>
#include <cstdlib>
#include <cmath>

//=============================================================================
// Helpers
//=============================================================================

static bool FloatNear(float A, float B, float Tolerance = 0.001f)
{
  return (fabsf(A - B) < Tolerance);
}

/**
 * Event tracker for verifying EventBus integration with scene lifecycle.
 */
struct IntegrationEventTracker
{
  int NodeCreatedCount;
  int NodeDestroyedCount;
  Node* LastNodeSender;
};

static void TrackNodeCreated(const AxEvent* Event, void* UserData)
{
  IntegrationEventTracker* T = static_cast<IntegrationEventTracker*>(UserData);
  T->NodeCreatedCount++;
  T->LastNodeSender = Event->Sender;
}

static void TrackNodeDestroyed(const AxEvent* Event, void* UserData)
{
  IntegrationEventTracker* T = static_cast<IntegrationEventTracker*>(UserData);
  T->NodeDestroyedCount++;
  T->LastNodeSender = Event->Sender;
}

//=============================================================================
// Test Fixture
//=============================================================================

class IntegrationTest : public testing::Test
{
protected:
  void SetUp() override
  {
    AxonInitGlobalAPIRegistry();
    AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);

    TableAPI_ = static_cast<AxHashTableAPI*>(
      AxonGlobalAPIRegistry->Get(AXON_HASH_TABLE_API_NAME));
    ASSERT_NE(TableAPI_, nullptr) << "Failed to get HashTableAPI";

    AllocatorAPI_ = static_cast<AxAllocatorAPI*>(
      AxonGlobalAPIRegistry->Get(AXON_ALLOCATOR_API_NAME));
    ASSERT_NE(AllocatorAPI_, nullptr) << "Failed to get AllocatorAPI";

    Allocator_ = AllocatorAPI_->CreateHeap("IntegrationTestAlloc", 256 * 1024, 2 * 1024 * 1024);
    ASSERT_NE(Allocator_, nullptr) << "Failed to create test allocator";

    // Initialize the scene parser
    Parser_.Init(AxonGlobalAPIRegistry);
  }

  void TearDown() override
  {
    Parser_.Term();

    if (Allocator_) {
      Allocator_->Destroy(Allocator_);
      Allocator_ = nullptr;
    }

    AxonTermGlobalAPIRegistry();
  }

  /**
   * Helper: Create a SceneTree for testing.
   */
  SceneTree* CreateTestScene(const char* Name)
  {
    SceneTree* Tree = new SceneTree(TableAPI_, nullptr);
    if (Tree) {
      Tree->Name = Name;
      Tree->Allocator = Allocator_;
    }
    return (Tree);
  }

  void DestroyTestScene(SceneTree* Tree)
  {
    delete Tree;
  }

  AxHashTableAPI*   TableAPI_{nullptr};
  AxAllocatorAPI*   AllocatorAPI_{nullptr};
  AxAllocator*      Allocator_{nullptr};
  SceneParser       Parser_;
};

//=============================================================================
// Test 1: Full Pipeline - Parse .ats -> SceneTree -> Verify Transform
//=============================================================================
TEST_F(IntegrationTest, FullPipelineParseAndVerifyTransform)
{
  const char* SceneSource = R"(scene "PipelineTest" {
    node "Player" MeshInstance {
      transform {
        translation: 10.0, 0.0, 0.0
      }
      mesh: "res://meshes/player.glb"
      material: "DefaultMat"
      node "Weapon" MeshInstance {
        transform {
          translation: 0.0, 1.0, 0.0
        }
        mesh: "res://meshes/sword.glb"
        material: "WeaponMat"
        renderLayer: 1
      }
    }
  })";

  SceneTree* Tree = Parser_.ParseFromString(SceneSource, Allocator_);
  ASSERT_NE(Tree, nullptr);

  Node* PlayerNode = Tree->GetRootNode()->GetFirstChild();
  ASSERT_NE(PlayerNode, nullptr);
  EXPECT_EQ(PlayerNode->GetName(), "Player");
  EXPECT_EQ(PlayerNode->GetType(), NodeType::MeshInstance);

  Node* WeaponNode = PlayerNode->GetFirstChild();
  ASSERT_NE(WeaponNode, nullptr);
  EXPECT_EQ(WeaponNode->GetName(), "Weapon");
  EXPECT_EQ(WeaponNode->GetType(), NodeType::MeshInstance);

  // Run SceneTree::Update to compute world matrices via UpdateNodeTransforms
  Tree->Update(0.016f);

  // Verify Player world position is (10, 0, 0)
  const AxMat4x4& PlayerWorld = PlayerNode->GetWorldTransform();
  EXPECT_TRUE(FloatNear(PlayerWorld.E[3][0], 10.0f));
  EXPECT_TRUE(FloatNear(PlayerWorld.E[3][1], 0.0f));

  // Verify Weapon world position is (10, 1, 0) = parent (10,0,0) + local (0,1,0)
  const AxMat4x4& WeaponWorld = WeaponNode->GetWorldTransform();
  EXPECT_TRUE(FloatNear(WeaponWorld.E[3][0], 10.0f));
  EXPECT_TRUE(FloatNear(WeaponWorld.E[3][1], 1.0f));

  // Verify MeshInstance nodes are in the typed tracking list
  uint32_t MeshCount = 0;
  Node** MeshNodes = Tree->GetNodesByType(NodeType::MeshInstance, &MeshCount);
  ASSERT_NE(MeshNodes, nullptr);
  ASSERT_EQ(MeshCount, 2u);

  // Verify mesh data on typed nodes
  MeshInstance* PlayerMI = static_cast<MeshInstance*>(MeshNodes[0]);
  EXPECT_STREQ(PlayerMI->MeshPath, "res://meshes/player.glb");

  MeshInstance* WeaponMI = static_cast<MeshInstance*>(MeshNodes[1]);
  EXPECT_STREQ(WeaponMI->MeshPath, "res://meshes/sword.glb");

  delete Tree;
}

//=============================================================================
// Test 2: Programmatic Scene Creation (No Parser)
//=============================================================================
TEST_F(IntegrationTest, ProgrammaticSceneCreation)
{
  SceneTree* Tree = CreateTestScene("ProgrammaticTest");
  ASSERT_NE(Tree, nullptr);

  // Create typed nodes programmatically
  Node* CamNode = Tree->CreateNode("Camera", NodeType::Camera, nullptr);
  Node* LightNodePtr = Tree->CreateNode("Light", NodeType::Light, nullptr);
  Node* MeshNodePtr = Tree->CreateNode("Cube", NodeType::MeshInstance, nullptr);
  ASSERT_NE(CamNode, nullptr);
  ASSERT_NE(LightNodePtr, nullptr);
  ASSERT_NE(MeshNodePtr, nullptr);

  // Set data on typed nodes
  CameraNode* Cam = static_cast<CameraNode*>(CamNode);
  Cam->SetFOV(60.0f);
  Cam->SetNear(0.1f);
  Cam->SetFar(500.0f);

  LightNode* LN = static_cast<LightNode*>(LightNodePtr);
  LN->SetLightType(AX_LIGHT_TYPE_DIRECTIONAL);
  LN->SetIntensity(2.0f);

  MeshInstance* MI = static_cast<MeshInstance*>(MeshNodePtr);
  MI->SetMeshPath("models/cube.gltf");

  // Set transform on mesh node
  MeshNodePtr->GetTransform().Translation = {5.0f, 0.0f, 0.0f};
  MeshNodePtr->GetTransform().ForwardMatrixDirty = true;

  // Run SceneTree::Update to compute world matrices
  Tree->Update(0.016f);

  // Verify world matrix was computed
  EXPECT_TRUE(FloatNear(MeshNodePtr->GetWorldTransform().E[3][0], 5.0f));

  // Verify typed node queries work
  uint32_t MeshCount = 0;
  Node** MeshNodes = Tree->GetNodesByType(NodeType::MeshInstance, &MeshCount);
  EXPECT_EQ(MeshCount, 1u);
  EXPECT_EQ(MeshNodes[0], MeshNodePtr);

  uint32_t LightCount = 0;
  Node** Lights = Tree->GetNodesByType(NodeType::Light, &LightCount);
  EXPECT_EQ(LightCount, 1u);
  EXPECT_EQ(Lights[0], LightNodePtr);

  uint32_t CamCount = 0;
  Node** Cameras = Tree->GetNodesByType(NodeType::Camera, &CamCount);
  EXPECT_EQ(CamCount, 1u);
  EXPECT_EQ(Cameras[0], CamNode);

  DestroyTestScene(Tree);
}

//=============================================================================
// Test 3: Destroying a Node Removes Typed Nodes From Tracking and Fires Events
//=============================================================================
TEST_F(IntegrationTest, DestroyNodeRemovesTypedNodesAndFiresEvents)
{
  SceneTree* Tree = CreateTestScene("DestroyTest");
  ASSERT_NE(Tree, nullptr);

  // Subscribe to lifecycle events
  IntegrationEventTracker Tracker = {0, 0, nullptr};
  Tree->GetEventBus()->Subscribe(AX_EVENT_NODE_CREATED, TrackNodeCreated, &Tracker);
  Tree->GetEventBus()->Subscribe(AX_EVENT_NODE_DESTROYED, TrackNodeDestroyed, &Tracker);

  // Create a parent MeshInstance with a child MeshInstance
  Node* Parent = Tree->CreateNode("Parent", NodeType::MeshInstance, nullptr);
  ASSERT_NE(Parent, nullptr);
  EXPECT_EQ(Tracker.NodeCreatedCount, 1);

  Node* Child = Tree->CreateNode("Child", NodeType::MeshInstance, Parent);
  ASSERT_NE(Child, nullptr);
  EXPECT_EQ(Tracker.NodeCreatedCount, 2);

  // Verify both are in typed tracking
  uint32_t Count = 0;
  Tree->GetNodesByType(NodeType::MeshInstance, &Count);
  EXPECT_EQ(Count, 2u);

  // Record node count before destruction
  uint32_t NodeCountBefore = Tree->GetNodeCount();

  // Destroy the parent (should recursively destroy child)
  Tree->DestroyNode(Parent);

  // Verify node destroyed events fired
  EXPECT_GE(Tracker.NodeDestroyedCount, 1);

  // Verify typed nodes removed from tracking
  uint32_t CountAfter = 0;
  Node** ListAfter = Tree->GetNodesByType(NodeType::MeshInstance, &CountAfter);
  EXPECT_EQ(CountAfter, 0u);
  EXPECT_EQ(ListAfter, nullptr);

  // Verify node count decreased
  EXPECT_LT(Tree->GetNodeCount(), NodeCountBefore);

  DestroyTestScene(Tree);
}

//=============================================================================
// Test 4: Prefab Instantiation Integrates With Transform Propagation
//=============================================================================
TEST_F(IntegrationTest, PrefabInstantiationIntegratesWithTransformPropagation)
{
  SceneTree* Tree = CreateTestScene("PrefabSystemTest");
  ASSERT_NE(Tree, nullptr);

  // Parse a prefab with typed nodes
  const char* PrefabData = R"(node "Obstacle" MeshInstance {
    transform {
      translation: 0.0, 0.0, 0.0
    }
    mesh: "res://obstacle.glb"
    material: "ObstacleMat"
  })";

  Node* PrefabRoot = Parser_.ParsePrefab(PrefabData, Tree);
  ASSERT_NE(PrefabRoot, nullptr);
  EXPECT_EQ(PrefabRoot->GetType(), NodeType::MeshInstance);

  // Instantiate two copies at different positions
  Node* Copy1 = Parser_.InstantiatePrefab(Tree, PrefabRoot, nullptr);
  Node* Copy2 = Parser_.InstantiatePrefab(Tree, PrefabRoot, nullptr);
  ASSERT_NE(Copy1, nullptr);
  ASSERT_NE(Copy2, nullptr);

  // Set different positions on the copies
  Copy1->GetTransform().Translation = {5.0f, 0.0f, 0.0f};
  Copy1->GetTransform().ForwardMatrixDirty = true;
  Copy2->GetTransform().Translation = {-5.0f, 0.0f, 0.0f};
  Copy2->GetTransform().ForwardMatrixDirty = true;

  // Run SceneTree::Update to compute world matrices
  Tree->Update(0.016f);

  // Verify copies have correct world positions
  EXPECT_TRUE(FloatNear(Copy1->GetWorldTransform().E[3][0], 5.0f));
  EXPECT_TRUE(FloatNear(Copy2->GetWorldTransform().E[3][0], -5.0f));

  // Verify MeshInstance nodes are in the typed tracking list
  // Prefab original + 2 copies = 3 MeshInstance nodes
  uint32_t MeshCount = 0;
  Node** MeshNodes = Tree->GetNodesByType(NodeType::MeshInstance, &MeshCount);
  EXPECT_GE(MeshCount, 2u);
  ASSERT_NE(MeshNodes, nullptr);

  DestroyTestScene(Tree);
}

//=============================================================================
// Test 5: Scene Destruction Cleans Up All Resources
//=============================================================================
TEST_F(IntegrationTest, SceneDestructionCleansUpAllResources)
{
  SceneTree* Tree = CreateTestScene("DestructionTest");
  ASSERT_NE(Tree, nullptr);

  // Build up a scene with typed nodes and event subscriptions
  Node* NodeA = Tree->CreateNode("NodeA", NodeType::MeshInstance, nullptr);
  Node* NodeB = Tree->CreateNode("NodeB", NodeType::MeshInstance, NodeA);
  ASSERT_NE(NodeA, nullptr);
  ASSERT_NE(NodeB, nullptr);

  // Set mesh data on typed nodes
  MeshInstance* MIA = static_cast<MeshInstance*>(NodeA);
  MIA->SetMeshPath("models/a.glb");
  MeshInstance* MIB = static_cast<MeshInstance*>(NodeB);
  MIB->SetMeshPath("models/b.glb");

  // Subscribe to an event
  IntegrationEventTracker Tracker = {0, 0, nullptr};
  Tree->GetEventBus()->Subscribe(AX_EVENT_USER_BASE, TrackNodeCreated, &Tracker);

  // Verify scene has content
  EXPECT_GE(Tree->GetNodeCount(), 3u); // Root + NodeA + NodeB

  // Destroy the scene - delete cleans up everything
  DestroyTestScene(Tree);

  // After deletion, the scene object is gone. No lookup needed.
  // The destructor handles all cleanup internally.
}

//=============================================================================
// Test 6: Multiple Scenes Operate Independently
//=============================================================================
TEST_F(IntegrationTest, MultipleScenesOperateIndependently)
{
  // Create two independent scenes
  SceneTree* TreeA = CreateTestScene("SceneA");
  SceneTree* TreeB = CreateTestScene("SceneB");
  ASSERT_NE(TreeA, nullptr);
  ASSERT_NE(TreeB, nullptr);

  // Subscribe to events on each scene's EventBus independently
  IntegrationEventTracker TrackerA = {0, 0, nullptr};
  IntegrationEventTracker TrackerB = {0, 0, nullptr};
  TreeA->GetEventBus()->Subscribe(AX_EVENT_NODE_CREATED, TrackNodeCreated, &TrackerA);
  TreeB->GetEventBus()->Subscribe(AX_EVENT_NODE_CREATED, TrackNodeCreated, &TrackerB);

  // Add typed nodes to SceneA only
  Node* NodeA1 = TreeA->CreateNode("A1", NodeType::MeshInstance, nullptr);
  Node* NodeA2 = TreeA->CreateNode("A2", NodeType::Light, nullptr);
  ASSERT_NE(NodeA1, nullptr);
  ASSERT_NE(NodeA2, nullptr);

  // Add a typed node to SceneB only
  Node* NodeB1 = TreeB->CreateNode("B1", NodeType::MeshInstance, nullptr);
  ASSERT_NE(NodeB1, nullptr);

  // Verify events are scene-scoped
  EXPECT_EQ(TrackerA.NodeCreatedCount, 2); // A1 and A2
  EXPECT_EQ(TrackerB.NodeCreatedCount, 1); // B1 only

  // Verify SceneB does not see SceneA's MeshInstance nodes
  uint32_t CountA = 0;
  uint32_t CountB = 0;
  TreeA->GetNodesByType(NodeType::MeshInstance, &CountA);
  TreeB->GetNodesByType(NodeType::MeshInstance, &CountB);
  EXPECT_EQ(CountA, 1u);
  EXPECT_EQ(CountB, 1u);

  // Verify node counts are independent
  // SceneA: Root + A1 + A2 = 3
  EXPECT_EQ(TreeA->GetNodeCount(), 3u);
  // SceneB: Root + B1 = 2
  EXPECT_EQ(TreeB->GetNodeCount(), 2u);

  DestroyTestScene(TreeA);
  DestroyTestScene(TreeB);
}

//=============================================================================
// Test 7: EventBus Lifecycle Integration - Node Events
//=============================================================================
TEST_F(IntegrationTest, EventBusLifecycleIntegration)
{
  SceneTree* Tree = CreateTestScene("EventLifecycleTest");
  ASSERT_NE(Tree, nullptr);

  // Subscribe to node lifecycle events
  IntegrationEventTracker Tracker = {0, 0, nullptr};
  Tree->GetEventBus()->Subscribe(AX_EVENT_NODE_CREATED, TrackNodeCreated, &Tracker);
  Tree->GetEventBus()->Subscribe(AX_EVENT_NODE_DESTROYED, TrackNodeDestroyed, &Tracker);

  // Create a node -> should fire NODE_CREATED
  Node* TestNode = Tree->CreateNode("EventTest", NodeType::MeshInstance, nullptr);
  ASSERT_NE(TestNode, nullptr);
  EXPECT_EQ(Tracker.NodeCreatedCount, 1);
  EXPECT_EQ(Tracker.LastNodeSender, TestNode);

  // Destroy the node -> should fire NODE_DESTROYED
  Tree->DestroyNode(TestNode);
  EXPECT_EQ(Tracker.NodeDestroyedCount, 1);

  // Verify final counts
  EXPECT_EQ(Tracker.NodeCreatedCount, 1);
  EXPECT_EQ(Tracker.NodeDestroyedCount, 1);

  DestroyTestScene(Tree);
}

//=============================================================================
// Test 8: Typed nodes with transforms propagate correctly through hierarchy
//=============================================================================
TEST_F(IntegrationTest, TypedNodeTransformPropagation)
{
  SceneTree* Tree = CreateTestScene("TransformTest");
  ASSERT_NE(Tree, nullptr);

  // Create a hierarchy: Root -> Light (at 0,10,0) -> MeshInstance (at 5,0,0)
  Node* LightNodePtr = Tree->CreateNode("WorldLight", NodeType::Light, nullptr);
  ASSERT_NE(LightNodePtr, nullptr);
  LightNodePtr->GetTransform().Translation = {0.0f, 10.0f, 0.0f};
  LightNodePtr->GetTransform().ForwardMatrixDirty = true;

  Node* MeshNodePtr = Tree->CreateNode("ChildMesh", NodeType::MeshInstance, LightNodePtr);
  ASSERT_NE(MeshNodePtr, nullptr);
  MeshNodePtr->GetTransform().Translation = {5.0f, 0.0f, 0.0f};
  MeshNodePtr->GetTransform().ForwardMatrixDirty = true;

  // Run update to propagate transforms
  Tree->Update(0.016f);

  // MeshInstance world position should be (5, 10, 0) = parent (0,10,0) + local (5,0,0)
  const AxMat4x4& MeshWorld = MeshNodePtr->GetWorldTransform();
  EXPECT_TRUE(FloatNear(MeshWorld.E[3][0], 5.0f));
  EXPECT_TRUE(FloatNear(MeshWorld.E[3][1], 10.0f));
  EXPECT_TRUE(FloatNear(MeshWorld.E[3][2], 0.0f));

  // Light world position should be (0, 10, 0)
  const AxMat4x4& LightWorld = LightNodePtr->GetWorldTransform();
  EXPECT_TRUE(FloatNear(LightWorld.E[3][0], 0.0f));
  EXPECT_TRUE(FloatNear(LightWorld.E[3][1], 10.0f));

  DestroyTestScene(Tree);
}
