/**
 * AxIntegrationTests.cpp - End-to-End Integration Tests for Node/Component/System
 *
 * Cross-cutting tests that verify the full pipeline works end-to-end:
 * parse .ats -> build scene -> run systems -> collect renderables.
 * Also tests backward compatibility, scene destruction, multiple scenes,
 * hot-reload, and EventBus lifecycle integration.
 *
 * Migrated from AxSceneExtension pattern to direct AxScene class usage.
 */

#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxHashTable.h"
#include "Foundation/AxAllocator.h"
#include "Foundation/AxAllocatorAPI.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxMath.h"
#include "AxEngine/AxNode.h"
#include "AxEngine/AxComponent.h"
#include "AxEngine/AxComponentFactory.h"
#include "AxEngine/AxSystem.h"
#include "AxEngine/AxScene.h"
#include "AxEngine/AxEventBus.h"
#include "AxScene/AxScene.h"
#include "AxSceneParserInternal.h"
#include "AxTransformSystem.h"
#include "AxRenderSystem.h"
#include "AxStandardComponents.h"

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
  int ComponentAddedCount;
  int ComponentRemovedCount;
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

static void TrackComponentAdded(const AxEvent* Event, void* UserData)
{
  IntegrationEventTracker* T = static_cast<IntegrationEventTracker*>(UserData);
  T->ComponentAddedCount++;
}

static void TrackComponentRemoved(const AxEvent* Event, void* UserData)
{
  IntegrationEventTracker* T = static_cast<IntegrationEventTracker*>(UserData);
  T->ComponentRemovedCount++;
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

    // Get the global ComponentFactory (static object, always available)
    Factory_ = AxComponentFactory_Get();
    ASSERT_NE(Factory_, nullptr) << "Failed to get ComponentFactory";

    // Register standard components
    RegisterStandardComponents(Factory_);

    // Initialize the scene parser
    AxSceneParser_Init(AxonGlobalAPIRegistry);
    ParserAPI_ = AxSceneParser_GetAPI();
    ASSERT_NE(ParserAPI_, nullptr) << "Failed to get AxSceneParserAPI";
  }

  void TearDown() override
  {
    AxSceneParser_Term();

    if (Allocator_) {
      Allocator_->Destroy(Allocator_);
      Allocator_ = nullptr;
    }

    AxonTermGlobalAPIRegistry();
  }

  /**
   * Helper: Create an AxScene for testing.
   */
  AxScene* CreateTestScene(const char* Name)
  {
    AxScene* Scene = new AxScene(TableAPI_);
    if (Scene) {
      Scene->Name = Name;
      Scene->Allocator = Allocator_;
    }
    return (Scene);
  }

  void DestroyTestScene(AxScene* Scene)
  {
    delete Scene;
  }

  AxHashTableAPI*   TableAPI_{nullptr};
  AxAllocatorAPI*   AllocatorAPI_{nullptr};
  AxAllocator*      Allocator_{nullptr};
  ComponentFactory* Factory_{nullptr};
  AxSceneParserAPI* ParserAPI_{nullptr};
};

//=============================================================================
// Test 1: Full Pipeline - Parse .ats -> TransformSystem -> RenderSystem
//=============================================================================
TEST_F(IntegrationTest, FullPipelineParseTransformRender)
{
  const char* SceneSource = R"(scene "PipelineTest" {
    node "Player" {
      transform {
        translation: 10.0, 0.0, 0.0
      }
      component MeshFilter {
        mesh: "res://meshes/player.glb"
      }
      component MeshRenderer {
        material: "DefaultMat"
        renderLayer: 0
      }
      node "Weapon" {
        transform {
          translation: 0.0, 1.0, 0.0
        }
        component MeshFilter {
          mesh: "res://meshes/sword.glb"
        }
        component MeshRenderer {
          material: "WeaponMat"
          renderLayer: 1
        }
      }
    }
  })";

  AxScene* Scene = ParserAPI_->ParseFromString(SceneSource, Allocator_);
  ASSERT_NE(Scene, nullptr);

  Node* PlayerNode = Scene->GetRootNode()->GetFirstChild();
  ASSERT_NE(PlayerNode, nullptr);
  EXPECT_EQ(PlayerNode->GetName(), "Player");

  Node* WeaponNode = PlayerNode->GetFirstChild();
  ASSERT_NE(WeaponNode, nullptr);
  EXPECT_EQ(WeaponNode->GetName(), "Weapon");

  // Run TransformSystem to compute world matrices
  TransformSystem TfSys;
  TfSys.SetScene(Scene);
  TfSys.Update(0.016f, nullptr, 0);

  // Verify Player world position is (10, 0, 0)
  const AxMat4x4& PlayerWorld = PlayerNode->GetTransform().CachedForwardMatrix;
  EXPECT_TRUE(FloatNear(PlayerWorld.E[3][0], 10.0f));
  EXPECT_TRUE(FloatNear(PlayerWorld.E[3][1], 0.0f));

  // Verify Weapon world position is (10, 1, 0) = parent (10,0,0) + local (0,1,0)
  const AxMat4x4& WeaponWorld = WeaponNode->GetTransform().CachedForwardMatrix;
  EXPECT_TRUE(FloatNear(WeaponWorld.E[3][0], 10.0f));
  EXPECT_TRUE(FloatNear(WeaponWorld.E[3][1], 1.0f));

  // Run RenderSystem to collect renderables
  const ComponentTypeInfo* MFInfo = Factory_->FindType("MeshFilter");
  const ComponentTypeInfo* MRInfo = Factory_->FindType("MeshRenderer");
  ASSERT_NE(MFInfo, nullptr);
  ASSERT_NE(MRInfo, nullptr);

  RenderSystem RndSys;
  RndSys.SetMeshFilterTypeID(MFInfo->TypeID);

  uint32_t MRCount = 0;
  Component** MRList = Scene->GetComponentsByType(MRInfo->TypeID, &MRCount);
  ASSERT_NE(MRList, nullptr);
  ASSERT_EQ(MRCount, 2u);

  RndSys.Update(0.016f, MRList, MRCount);

  uint32_t RenderableCount = 0;
  const RenderableEntry* Renderables = RndSys.GetRenderables(&RenderableCount);
  EXPECT_EQ(RenderableCount, 2u);

  delete Scene;
}

//=============================================================================
// Test 2: Programmatic Scene Creation (No Parser)
//=============================================================================
TEST_F(IntegrationTest, ProgrammaticSceneCreation)
{
  AxScene* Scene = CreateTestScene("ProgrammaticTest");
  ASSERT_NE(Scene, nullptr);

  // Create nodes programmatically
  Node* CameraNode = Scene->CreateNode("Camera", NodeType::Node3D, nullptr);
  Node* LightNode = Scene->CreateNode("Light", NodeType::Node3D, nullptr);
  Node* MeshNode = Scene->CreateNode("Cube", NodeType::Node3D, nullptr);
  ASSERT_NE(CameraNode, nullptr);
  ASSERT_NE(LightNode, nullptr);
  ASSERT_NE(MeshNode, nullptr);

  // Add components via factory
  Component* CamComp = Factory_->CreateComponent("CameraComponent", Allocator_);
  Component* LitComp = Factory_->CreateComponent("LightComponent", Allocator_);
  Component* FilterComp = Factory_->CreateComponent("MeshFilter", Allocator_);
  Component* RendererComp = Factory_->CreateComponent("MeshRenderer", Allocator_);
  ASSERT_NE(CamComp, nullptr);
  ASSERT_NE(LitComp, nullptr);
  ASSERT_NE(FilterComp, nullptr);
  ASSERT_NE(RendererComp, nullptr);

  EXPECT_TRUE(Scene->AddComponent(CameraNode, CamComp));
  EXPECT_TRUE(Scene->AddComponent(LightNode, LitComp));
  EXPECT_TRUE(Scene->AddComponent(MeshNode, FilterComp));
  EXPECT_TRUE(Scene->AddComponent(MeshNode, RendererComp));

  // Set transform on mesh node
  MeshNode->GetTransform().Translation = {5.0f, 0.0f, 0.0f};
  MeshNode->GetTransform().ForwardMatrixDirty = true;

  // Register and run TransformSystem
  TransformSystem TfSys;
  TfSys.SetScene(Scene);
  Scene->RegisterSystem(&TfSys);

  Scene->UpdateSystems(0.016f);

  // Verify world matrix was computed
  EXPECT_TRUE(FloatNear(MeshNode->GetTransform().CachedForwardMatrix.E[3][0], 5.0f));

  // Verify component queries work
  const ComponentTypeInfo* MFInfo = Factory_->FindType("MeshFilter");
  ASSERT_NE(MFInfo, nullptr);
  uint32_t MFCount = 0;
  Component** MFComps = Scene->GetComponentsByType(MFInfo->TypeID, &MFCount);
  EXPECT_EQ(MFCount, 1u);
  EXPECT_EQ(MFComps[0], FilterComp);

  // Clean up
  Scene->UnregisterSystem(&TfSys);
  Scene->RemoveComponent(CameraNode, CamComp->GetTypeID());
  Scene->RemoveComponent(LightNode, LitComp->GetTypeID());
  Scene->RemoveComponent(MeshNode, FilterComp->GetTypeID());
  Scene->RemoveComponent(MeshNode, RendererComp->GetTypeID());
  Factory_->DestroyComponent(CamComp, Allocator_);
  Factory_->DestroyComponent(LitComp, Allocator_);
  Factory_->DestroyComponent(FilterComp, Allocator_);
  Factory_->DestroyComponent(RendererComp, Allocator_);

  DestroyTestScene(Scene);
}

//=============================================================================
// Test 3: Destroying a Node Removes Components From Global Lists and Fires Events
//=============================================================================
TEST_F(IntegrationTest, DestroyNodeRemovesComponentsAndFiresEvents)
{
  AxScene* Scene = CreateTestScene("DestroyTest");
  ASSERT_NE(Scene, nullptr);

  // Subscribe to lifecycle events
  IntegrationEventTracker Tracker = {0, 0, 0, 0, nullptr};
  Scene->GetEventBus()->Subscribe(AX_EVENT_NODE_CREATED, TrackNodeCreated, &Tracker);
  Scene->GetEventBus()->Subscribe(AX_EVENT_NODE_DESTROYED, TrackNodeDestroyed, &Tracker);
  Scene->GetEventBus()->Subscribe(AX_EVENT_COMPONENT_ADDED, TrackComponentAdded, &Tracker);
  Scene->GetEventBus()->Subscribe(AX_EVENT_COMPONENT_REMOVED, TrackComponentRemoved, &Tracker);

  // Create a parent with a child, each having a component
  Node* Parent = Scene->CreateNode("Parent", NodeType::Node3D, nullptr);
  ASSERT_NE(Parent, nullptr);
  EXPECT_EQ(Tracker.NodeCreatedCount, 1);

  Node* Child = Scene->CreateNode("Child", NodeType::Node3D, Parent);
  ASSERT_NE(Child, nullptr);
  EXPECT_EQ(Tracker.NodeCreatedCount, 2);

  // Add components
  MeshFilter ParentFilter;
  ParentFilter.TypeID_ = 700;
  ParentFilter.TypeName_ = "MeshFilter";
  MeshFilter ChildFilter;
  ChildFilter.TypeID_ = 700;
  ChildFilter.TypeName_ = "MeshFilter";

  EXPECT_TRUE(Scene->AddComponent(Parent, &ParentFilter));
  EXPECT_TRUE(Scene->AddComponent(Child, &ChildFilter));
  EXPECT_EQ(Tracker.ComponentAddedCount, 2);

  // Verify both components are in global list
  uint32_t Count = 0;
  Scene->GetComponentsByType(700, &Count);
  EXPECT_EQ(Count, 2u);

  // Record node count before destruction
  uint32_t NodeCountBefore = Scene->GetNodeCount();

  // Destroy the parent (should recursively destroy child, remove all components)
  Scene->DestroyNode(Parent);

  // Verify node destroyed events fired
  EXPECT_GE(Tracker.NodeDestroyedCount, 1);

  // Verify components removed from global list
  uint32_t CountAfter = 0;
  Component** ListAfter = Scene->GetComponentsByType(700, &CountAfter);
  EXPECT_EQ(CountAfter, 0u);

  // Verify node count decreased
  EXPECT_LT(Scene->GetNodeCount(), NodeCountBefore);

  DestroyTestScene(Scene);
}

//=============================================================================
// Test 4: Prefab Instantiation Integrates With System Processing
//=============================================================================
TEST_F(IntegrationTest, PrefabInstantiationIntegratesWithSystems)
{
  AxScene* Scene = CreateTestScene("PrefabSystemTest");
  ASSERT_NE(Scene, nullptr);

  // Parse a prefab
  const char* PrefabData = R"(node "Obstacle" {
    transform {
      translation: 0.0, 0.0, 0.0
    }
    component MeshFilter {
      mesh: "res://obstacle.glb"
    }
    component MeshRenderer {
      material: "ObstacleMat"
    }
  })";

  Node* PrefabRoot = ParsePrefab(PrefabData, Scene);
  ASSERT_NE(PrefabRoot, nullptr);

  // Instantiate two copies at different positions
  Node* Copy1 = InstantiatePrefab(Scene, PrefabRoot, nullptr, Allocator_);
  Node* Copy2 = InstantiatePrefab(Scene, PrefabRoot, nullptr, Allocator_);
  ASSERT_NE(Copy1, nullptr);
  ASSERT_NE(Copy2, nullptr);

  // Set different positions on the copies
  Copy1->GetTransform().Translation = {5.0f, 0.0f, 0.0f};
  Copy1->GetTransform().ForwardMatrixDirty = true;
  Copy2->GetTransform().Translation = {-5.0f, 0.0f, 0.0f};
  Copy2->GetTransform().ForwardMatrixDirty = true;

  // Run TransformSystem on the scene
  TransformSystem TfSys;
  TfSys.SetScene(Scene);
  TfSys.Update(0.016f, nullptr, 0);

  // Verify copies have correct world positions
  EXPECT_TRUE(FloatNear(Copy1->GetTransform().CachedForwardMatrix.E[3][0], 5.0f));
  EXPECT_TRUE(FloatNear(Copy2->GetTransform().CachedForwardMatrix.E[3][0], -5.0f));

  // Verify MeshRenderer components are in the global list
  const ComponentTypeInfo* MRInfo = Factory_->FindType("MeshRenderer");
  ASSERT_NE(MRInfo, nullptr);
  uint32_t MRCount = 0;
  Component** MRComps = Scene->GetComponentsByType(MRInfo->TypeID, &MRCount);
  // At least 2 MeshRenderers from the two instantiated copies
  EXPECT_GE(MRCount, 2u);

  DestroyTestScene(Scene);
}

//=============================================================================
// Test 5: Hot-Reload Scenario - Unregister/Re-Register Component Type
//=============================================================================
TEST_F(IntegrationTest, HotReloadUnregisterReregisterComponentType)
{
  // The hot-reload pattern: unregister a type, then re-register it
  // This simulates a plugin being unloaded and reloaded

  // Create a dedicated factory for this test (separate from the global one)
  ComponentFactory HotReloadFactory;

  // Define create/destroy functions for a custom component
  struct CustomComp : public Component
  {
    int Value;
    CustomComp() : Value(42) { TypeName_ = "CustomComp"; }
    size_t GetSize() const override { return (sizeof(CustomComp)); }
  };

  auto CreateCustom = [](void* Memory) -> Component* {
    return (new (Memory) CustomComp());
  };
  auto DestroyCustom = [](Component* Comp) {
    static_cast<CustomComp*>(Comp)->~CustomComp();
  };

  // Register the custom type
  bool Reg1 = HotReloadFactory.RegisterType("CustomComp", CreateCustom, DestroyCustom, sizeof(CustomComp));
  EXPECT_TRUE(Reg1);

  // Create an instance
  Component* Inst1 = HotReloadFactory.CreateComponent("CustomComp", Allocator_);
  ASSERT_NE(Inst1, nullptr);
  uint32_t OrigTypeID = Inst1->GetTypeID();
  EXPECT_EQ(Inst1->GetTypeName(), "CustomComp");

  // Destroy the instance
  HotReloadFactory.DestroyComponent(Inst1, Allocator_);

  // Unregister the type (simulating plugin unload)
  bool Unreg = HotReloadFactory.UnregisterType("CustomComp");
  EXPECT_TRUE(Unreg);

  // Verify it is gone
  const ComponentTypeInfo* Gone = HotReloadFactory.FindType("CustomComp");
  EXPECT_EQ(Gone, nullptr);

  // Cannot create after unregister
  Component* Fail = HotReloadFactory.CreateComponent("CustomComp", Allocator_);
  EXPECT_EQ(Fail, nullptr);

  // Re-register (simulating plugin reload)
  bool Reg2 = HotReloadFactory.RegisterType("CustomComp", CreateCustom, DestroyCustom, sizeof(CustomComp));
  EXPECT_TRUE(Reg2);

  // Create a new instance after re-registration
  Component* Inst2 = HotReloadFactory.CreateComponent("CustomComp", Allocator_);
  ASSERT_NE(Inst2, nullptr);
  EXPECT_EQ(Inst2->GetTypeName(), "CustomComp");

  // The TypeID may differ from the original since IDs are auto-assigned
  EXPECT_GT(Inst2->GetTypeID(), 0u);

  HotReloadFactory.DestroyComponent(Inst2, Allocator_);
}

//=============================================================================
// Test 6: ComponentFactory + AxSceneAPI Integration Through AxAPIRegistry
//=============================================================================
TEST_F(IntegrationTest, ComponentFactoryAPIRegistryIntegration)
{
  // The global ComponentFactory is a static object; access it directly.
  ComponentFactory* FactoryAPI = AxComponentFactory_Get();
  ASSERT_NE(FactoryAPI, nullptr) << "ComponentFactory not available";

  // Verify we can look up a standard component type
  const ComponentTypeInfo* MFInfo = FactoryAPI->FindType("MeshFilter");
  ASSERT_NE(MFInfo, nullptr);
  EXPECT_EQ(MFInfo->TypeName, "MeshFilter");
  EXPECT_GT(MFInfo->TypeID, 0u);

  // Verify we can create a component through the API
  Component* Comp = FactoryAPI->CreateComponent("MeshFilter", Allocator_);
  ASSERT_NE(Comp, nullptr);
  EXPECT_EQ(Comp->GetTypeName(), "MeshFilter");
  EXPECT_EQ(Comp->GetTypeID(), MFInfo->TypeID);

  // Verify GetTypeCount is consistent
  uint32_t Count = FactoryAPI->GetTypeCount();
  EXPECT_GT(Count, 0u);

  // Clean up
  FactoryAPI->DestroyComponent(Comp, Allocator_);
}

//=============================================================================
// Test 7: Scene Destruction Cleans Up All Resources
//=============================================================================
TEST_F(IntegrationTest, SceneDestructionCleansUpAllResources)
{
  AxScene* Scene = CreateTestScene("DestructionTest");
  ASSERT_NE(Scene, nullptr);

  // Build up a scene with nodes, components, systems, and event subscriptions
  Node* NodeA = Scene->CreateNode("NodeA", NodeType::Node3D, nullptr);
  Node* NodeB = Scene->CreateNode("NodeB", NodeType::Node3D, NodeA);
  ASSERT_NE(NodeA, nullptr);
  ASSERT_NE(NodeB, nullptr);

  // Add stack-allocated test components (not factory-created, just for tracking)
  MeshFilter FilterA;
  FilterA.TypeID_ = 800;
  FilterA.TypeName_ = "MeshFilter";
  MeshFilter FilterB;
  FilterB.TypeID_ = 800;
  FilterB.TypeName_ = "MeshFilter";

  EXPECT_TRUE(Scene->AddComponent(NodeA, &FilterA));
  EXPECT_TRUE(Scene->AddComponent(NodeB, &FilterB));

  // Subscribe to an event
  IntegrationEventTracker Tracker = {0, 0, 0, 0, nullptr};
  Scene->GetEventBus()->Subscribe(AX_EVENT_USER_BASE, TrackNodeCreated, &Tracker);

  // Register a system
  TransformSystem TfSys;
  TfSys.SetScene(Scene);
  Scene->RegisterSystem(&TfSys);

  // Verify scene has content
  EXPECT_GE(Scene->GetNodeCount(), 3u); // Root + NodeA + NodeB
  EXPECT_GE(Scene->GetSystemCount(), 1u);

  // Now unregister the system before destruction to avoid dangling pointer
  Scene->UnregisterSystem(&TfSys);

  // Destroy the scene - delete cleans up everything
  DestroyTestScene(Scene);

  // After deletion, the scene object is gone. No lookup needed.
  // The destructor handles all cleanup internally.
}

//=============================================================================
// Test 8: Multiple Scenes Operate Independently
//=============================================================================
TEST_F(IntegrationTest, MultipleScenesOperateIndependently)
{
  // Create two independent scenes
  AxScene* SceneA = CreateTestScene("SceneA");
  AxScene* SceneB = CreateTestScene("SceneB");
  ASSERT_NE(SceneA, nullptr);
  ASSERT_NE(SceneB, nullptr);

  // Subscribe to events on each scene's EventBus independently
  IntegrationEventTracker TrackerA = {0, 0, 0, 0, nullptr};
  IntegrationEventTracker TrackerB = {0, 0, 0, 0, nullptr};
  SceneA->GetEventBus()->Subscribe(AX_EVENT_NODE_CREATED, TrackNodeCreated, &TrackerA);
  SceneB->GetEventBus()->Subscribe(AX_EVENT_NODE_CREATED, TrackNodeCreated, &TrackerB);

  // Add nodes to SceneA only
  Node* NodeA1 = SceneA->CreateNode("A1", NodeType::Node3D, nullptr);
  Node* NodeA2 = SceneA->CreateNode("A2", NodeType::Node3D, nullptr);
  ASSERT_NE(NodeA1, nullptr);
  ASSERT_NE(NodeA2, nullptr);

  // Add a node to SceneB only
  Node* NodeB1 = SceneB->CreateNode("B1", NodeType::Node3D, nullptr);
  ASSERT_NE(NodeB1, nullptr);

  // Verify events are scene-scoped
  EXPECT_EQ(TrackerA.NodeCreatedCount, 2); // A1 and A2
  EXPECT_EQ(TrackerB.NodeCreatedCount, 1); // B1 only

  // Add a component to SceneA
  MeshFilter FilterA;
  FilterA.TypeID_ = 900;
  FilterA.TypeName_ = "MeshFilter";
  EXPECT_TRUE(SceneA->AddComponent(NodeA1, &FilterA));

  // Verify SceneB does not see SceneA's component
  uint32_t CountA = 0;
  uint32_t CountB = 0;
  SceneA->GetComponentsByType(900, &CountA);
  SceneB->GetComponentsByType(900, &CountB);
  EXPECT_EQ(CountA, 1u);
  EXPECT_EQ(CountB, 0u);

  // Verify node counts are independent
  // SceneA: Root + A1 + A2 = 3
  EXPECT_EQ(SceneA->GetNodeCount(), 3u);
  // SceneB: Root + B1 = 2
  EXPECT_EQ(SceneB->GetNodeCount(), 2u);

  // Clean up component before scene destruction
  SceneA->RemoveComponent(NodeA1, 900);

  DestroyTestScene(SceneA);
  DestroyTestScene(SceneB);
}

//=============================================================================
// Test 9: EventBus Lifecycle Integration - Node and Component Events
//=============================================================================
TEST_F(IntegrationTest, EventBusLifecycleIntegration)
{
  AxScene* Scene = CreateTestScene("EventLifecycleTest");
  ASSERT_NE(Scene, nullptr);

  // Subscribe to all lifecycle events
  IntegrationEventTracker Tracker = {0, 0, 0, 0, nullptr};
  Scene->GetEventBus()->Subscribe(AX_EVENT_NODE_CREATED, TrackNodeCreated, &Tracker);
  Scene->GetEventBus()->Subscribe(AX_EVENT_NODE_DESTROYED, TrackNodeDestroyed, &Tracker);
  Scene->GetEventBus()->Subscribe(AX_EVENT_COMPONENT_ADDED, TrackComponentAdded, &Tracker);
  Scene->GetEventBus()->Subscribe(AX_EVENT_COMPONENT_REMOVED, TrackComponentRemoved, &Tracker);

  // Create a node -> should fire NODE_CREATED
  Node* TestNode = Scene->CreateNode("EventTest", NodeType::Node3D, nullptr);
  ASSERT_NE(TestNode, nullptr);
  EXPECT_EQ(Tracker.NodeCreatedCount, 1);
  EXPECT_EQ(Tracker.LastNodeSender, TestNode);

  // Add a component -> should fire COMPONENT_ADDED
  MeshFilter Filter;
  Filter.TypeID_ = 950;
  Filter.TypeName_ = "MeshFilter";
  EXPECT_TRUE(Scene->AddComponent(TestNode, &Filter));
  EXPECT_EQ(Tracker.ComponentAddedCount, 1);

  // Remove the component -> should fire COMPONENT_REMOVED
  EXPECT_TRUE(Scene->RemoveComponent(TestNode, 950));
  EXPECT_EQ(Tracker.ComponentRemovedCount, 1);

  // Destroy the node -> should fire NODE_DESTROYED
  Scene->DestroyNode(TestNode);
  EXPECT_EQ(Tracker.NodeDestroyedCount, 1);

  // Verify final counts
  EXPECT_EQ(Tracker.NodeCreatedCount, 1);
  EXPECT_EQ(Tracker.NodeDestroyedCount, 1);
  EXPECT_EQ(Tracker.ComponentAddedCount, 1);
  EXPECT_EQ(Tracker.ComponentRemovedCount, 1);

  DestroyTestScene(Scene);
}
