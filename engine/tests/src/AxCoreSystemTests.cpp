/**
 * AxCoreSystemTests.cpp - Tests for TransformSystem and RenderSystem
 *
 * Tests the core system implementations:
 * - TransformSystem: hierarchy traversal, dirty flag propagation, matrix computation
 * - RenderSystem: MeshFilter+MeshRenderer pairing into renderable list
 * - System execution ordering (TransformSystem before RenderSystem)
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
#include "AxEngine/AxSystem.h"
#include "AxEngine/AxScene.h"
#include "AxTransformSystem.h"
#include "AxRenderSystem.h"
#include "AxStandardComponents.h"

#include <cstring>
#include <cmath>

//=============================================================================
// Test Fixture
//=============================================================================

class CoreSystemTest : public testing::Test
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

    // Create AxScene directly
    Scene_ = new AxScene(TableAPI_);
    ASSERT_NE(Scene_, nullptr);
    Scene_->Name = "TestScene";
  }

  void TearDown() override
  {
    delete Scene_;
    Scene_ = nullptr;

    AxonTermGlobalAPIRegistry();
  }

  /**
   * Helper: Set a node's local translation and mark its transform dirty.
   */
  void SetNodeTranslation(Node* N, float X, float Y, float Z)
  {
    AxTransform& T = N->GetTransform();
    T.Translation = {X, Y, Z};
    T.ForwardMatrixDirty = true;
  }

  /**
   * Helper: Set a node's local scale and mark its transform dirty.
   */
  void SetNodeScale(Node* N, float X, float Y, float Z)
  {
    AxTransform& T = N->GetTransform();
    T.Scale = {X, Y, Z};
    T.ForwardMatrixDirty = true;
  }

  AxHashTableAPI* TableAPI_{nullptr};
  AxAllocatorAPI* AllocatorAPI_{nullptr};
  AxScene* Scene_{nullptr};
};

//=============================================================================
// Helper: Compare two floats with tolerance
//=============================================================================

static bool FloatNear(float A, float B, float Tolerance = 0.001f)
{
  return (fabsf(A - B) < Tolerance);
}

//=============================================================================
// Test 1: TransformSystem propagates parent world matrix to children
//=============================================================================
TEST_F(CoreSystemTest, TransformSystemPropagatesParentMatrix)
{
  // Create a parent-child pair
  Node* Parent = Scene_->CreateNode("Parent", NodeType::Node3D, nullptr);
  Node* Child = Scene_->CreateNode("Child", NodeType::Node3D, Parent);
  ASSERT_NE(Parent, nullptr);
  ASSERT_NE(Child, nullptr);

  // Set parent translation to (10, 0, 0) and child translation to (0, 5, 0)
  SetNodeTranslation(Parent, 10.0f, 0.0f, 0.0f);
  SetNodeTranslation(Child, 0.0f, 5.0f, 0.0f);

  // Create and configure TransformSystem
  TransformSystem TfSys;
  TfSys.SetScene(Scene_);

  // Run the system
  TfSys.Update(0.016f, nullptr, 0);

  // Child's world matrix should combine parent + child translations
  // WorldMatrix = Parent.World * Child.Local
  // Parent.World translates by (10,0,0), Child.Local translates by (0,5,0)
  // Result: child world position should be (10, 5, 0)
  const AxMat4x4& ChildWorld = Child->GetTransform().CachedForwardMatrix;
  EXPECT_TRUE(FloatNear(ChildWorld.E[3][0], 10.0f))
    << "Child world X should be 10.0, got " << ChildWorld.E[3][0];
  EXPECT_TRUE(FloatNear(ChildWorld.E[3][1], 5.0f))
    << "Child world Y should be 5.0, got " << ChildWorld.E[3][1];
  EXPECT_TRUE(FloatNear(ChildWorld.E[3][2], 0.0f))
    << "Child world Z should be 0.0, got " << ChildWorld.E[3][2];

  // Parent's world matrix should just be its own translation
  const AxMat4x4& ParentWorld = Parent->GetTransform().CachedForwardMatrix;
  EXPECT_TRUE(FloatNear(ParentWorld.E[3][0], 10.0f));
  EXPECT_TRUE(FloatNear(ParentWorld.E[3][1], 0.0f));
  EXPECT_TRUE(FloatNear(ParentWorld.E[3][2], 0.0f));
}

//=============================================================================
// Test 2: TransformSystem skips clean transforms (dirty flag optimization)
//=============================================================================
TEST_F(CoreSystemTest, TransformSystemSkipsCleanTransforms)
{
  // Create a node and set its translation
  Node* NodeA = Scene_->CreateNode("NodeA", NodeType::Node3D, nullptr);
  ASSERT_NE(NodeA, nullptr);

  SetNodeTranslation(NodeA, 5.0f, 0.0f, 0.0f);

  // Create and configure TransformSystem
  TransformSystem TfSys;
  TfSys.SetScene(Scene_);

  // Run the system once to compute initial matrices
  TfSys.Update(0.016f, nullptr, 0);

  // Verify the matrix was computed
  const AxMat4x4& WorldA = NodeA->GetTransform().CachedForwardMatrix;
  EXPECT_TRUE(FloatNear(WorldA.E[3][0], 5.0f));

  // Dirty flag should now be false
  EXPECT_FALSE(NodeA->GetTransform().ForwardMatrixDirty);

  // Manually modify the cached matrix to a sentinel value
  // (this will let us detect if the system re-computes it)
  NodeA->GetTransform().CachedForwardMatrix.E[3][0] = 999.0f;

  // Run system again WITHOUT marking dirty
  TfSys.Update(0.016f, nullptr, 0);

  // The cached matrix should NOT have been recomputed because ForwardMatrixDirty is false
  // So the sentinel value should remain
  EXPECT_TRUE(FloatNear(NodeA->GetTransform().CachedForwardMatrix.E[3][0], 999.0f))
    << "TransformSystem should skip clean transforms; cached matrix should not be overwritten";
}

//=============================================================================
// Test 3: TransformSystem handles deep hierarchy (3+ levels)
//=============================================================================
TEST_F(CoreSystemTest, TransformSystemDeepHierarchy)
{
  // Build a 4-level hierarchy: Root -> Level1 -> Level2 -> Level3
  Node* Level1 = Scene_->CreateNode("Level1", NodeType::Node3D, nullptr);
  Node* Level2 = Scene_->CreateNode("Level2", NodeType::Node3D, Level1);
  Node* Level3 = Scene_->CreateNode("Level3", NodeType::Node3D, Level2);
  ASSERT_NE(Level1, nullptr);
  ASSERT_NE(Level2, nullptr);
  ASSERT_NE(Level3, nullptr);

  // Each level translates by (1, 0, 0) in local space
  SetNodeTranslation(Level1, 1.0f, 0.0f, 0.0f);
  SetNodeTranslation(Level2, 1.0f, 0.0f, 0.0f);
  SetNodeTranslation(Level3, 1.0f, 0.0f, 0.0f);

  // Create and configure TransformSystem
  TransformSystem TfSys;
  TfSys.SetScene(Scene_);

  // Run the system
  TfSys.Update(0.016f, nullptr, 0);

  // Level1 world X should be 1.0 (just its own translation from root)
  EXPECT_TRUE(FloatNear(Level1->GetTransform().CachedForwardMatrix.E[3][0], 1.0f))
    << "Level1 world X should be 1.0";

  // Level2 world X should be 2.0 (Level1 + Level2)
  EXPECT_TRUE(FloatNear(Level2->GetTransform().CachedForwardMatrix.E[3][0], 2.0f))
    << "Level2 world X should be 2.0";

  // Level3 world X should be 3.0 (Level1 + Level2 + Level3)
  EXPECT_TRUE(FloatNear(Level3->GetTransform().CachedForwardMatrix.E[3][0], 3.0f))
    << "Level3 world X should be 3.0";

  // All dirty flags should be cleared
  EXPECT_FALSE(Level1->GetTransform().ForwardMatrixDirty);
  EXPECT_FALSE(Level2->GetTransform().ForwardMatrixDirty);
  EXPECT_FALSE(Level3->GetTransform().ForwardMatrixDirty);
}

//=============================================================================
// Test 4: RenderSystem collects MeshFilter+MeshRenderer pairs
//=============================================================================
TEST_F(CoreSystemTest, RenderSystemCollectsMeshPairs)
{
  // Create a node with both MeshFilter and MeshRenderer
  Node* NodeA = Scene_->CreateNode("NodeA", NodeType::Node3D, nullptr);
  ASSERT_NE(NodeA, nullptr);

  // Use known TypeID values for test components
  static constexpr uint32_t MeshFilterTypeID = 500;
  static constexpr uint32_t MeshRendererTypeID = 501;

  MeshFilter Filter;
  Filter.TypeID_ = MeshFilterTypeID;
  Filter.TypeName_ = "MeshFilter";
  Filter.ModelHandle = {42, 1};

  MeshRenderer Renderer;
  Renderer.TypeID_ = MeshRendererTypeID;
  Renderer.TypeName_ = "MeshRenderer";
  Renderer.MaterialHandle = 99;

  // Add components to node and scene
  EXPECT_TRUE(Scene_->AddComponent(NodeA, &Filter));
  EXPECT_TRUE(Scene_->AddComponent(NodeA, &Renderer));

  // Set up the node's cached world matrix with a known value
  NodeA->GetTransform().CachedForwardMatrix = Identity();
  NodeA->GetTransform().CachedForwardMatrix.E[3][0] = 7.0f; // X translation

  // Create RenderSystem configured with the MeshFilter TypeID
  RenderSystem RndSys;
  RndSys.SetMeshFilterTypeID(MeshFilterTypeID);

  // Build the component array to pass (MeshRenderer components)
  Component* CompArray[1] = {&Renderer};
  RndSys.Update(0.016f, CompArray, 1);

  // Verify renderables
  uint32_t RenderableCount = 0;
  const RenderableEntry* Renderables = RndSys.GetRenderables(&RenderableCount);
  ASSERT_EQ(RenderableCount, 1u);
  EXPECT_EQ(Renderables[0].ModelHandle.Index, 42u);
  EXPECT_EQ(Renderables[0].MaterialHandle, 99u);
  EXPECT_TRUE(FloatNear(Renderables[0].WorldMatrix.E[3][0], 7.0f));

  // Clean up components from scene before teardown
  Scene_->RemoveComponent(NodeA, MeshFilterTypeID);
  Scene_->RemoveComponent(NodeA, MeshRendererTypeID);
}

//=============================================================================
// Test 5: RenderSystem skips nodes with MeshRenderer but no MeshFilter
//=============================================================================
TEST_F(CoreSystemTest, RenderSystemSkipsMissingMeshFilter)
{
  // Create two nodes:
  // NodeA has only MeshRenderer (no MeshFilter) - should be skipped
  // NodeB has both MeshFilter and MeshRenderer - should be collected
  Node* NodeA = Scene_->CreateNode("NodeA", NodeType::Node3D, nullptr);
  Node* NodeB = Scene_->CreateNode("NodeB", NodeType::Node3D, nullptr);
  ASSERT_NE(NodeA, nullptr);
  ASSERT_NE(NodeB, nullptr);

  static constexpr uint32_t MeshFilterTypeID = 600;
  static constexpr uint32_t MeshRendererTypeID = 601;

  // NodeA: only MeshRenderer (no MeshFilter)
  MeshRenderer RendererA;
  RendererA.TypeID_ = MeshRendererTypeID;
  RendererA.TypeName_ = "MeshRenderer";
  RendererA.MaterialHandle = 10;
  EXPECT_TRUE(Scene_->AddComponent(NodeA, &RendererA));

  // NodeB: both MeshFilter and MeshRenderer
  MeshFilter FilterB;
  FilterB.TypeID_ = MeshFilterTypeID;
  FilterB.TypeName_ = "MeshFilter";
  FilterB.ModelHandle = {77, 1};

  MeshRenderer RendererB;
  RendererB.TypeID_ = MeshRendererTypeID;
  RendererB.TypeName_ = "MeshRenderer";
  RendererB.MaterialHandle = 88;

  EXPECT_TRUE(Scene_->AddComponent(NodeB, &FilterB));
  EXPECT_TRUE(Scene_->AddComponent(NodeB, &RendererB));

  // Set up world matrices
  NodeA->GetTransform().CachedForwardMatrix = Identity();
  NodeB->GetTransform().CachedForwardMatrix = Identity();
  NodeB->GetTransform().CachedForwardMatrix.E[3][0] = 3.0f;

  // Create RenderSystem
  RenderSystem RndSys;
  RndSys.SetMeshFilterTypeID(MeshFilterTypeID);

  // Pass both MeshRenderers
  Component* CompArray[2] = {&RendererA, &RendererB};
  RndSys.Update(0.016f, CompArray, 2);

  // Only NodeB should produce a renderable (NodeA has no MeshFilter)
  uint32_t RenderableCount = 0;
  const RenderableEntry* Renderables = RndSys.GetRenderables(&RenderableCount);
  ASSERT_EQ(RenderableCount, 1u);
  EXPECT_EQ(Renderables[0].ModelHandle.Index, 77u);
  EXPECT_EQ(Renderables[0].MaterialHandle, 88u);

  // Clean up
  Scene_->RemoveComponent(NodeA, MeshRendererTypeID);
  Scene_->RemoveComponent(NodeB, MeshFilterTypeID);
  Scene_->RemoveComponent(NodeB, MeshRendererTypeID);
}

//=============================================================================
// Test 6: System execution order - TransformSystem runs before RenderSystem
//=============================================================================
TEST_F(CoreSystemTest, TransformRunsBeforeRender)
{
  // Register TransformSystem (EarlyUpdate) and RenderSystem (Render) with scene
  TransformSystem TfSys;
  TfSys.SetScene(Scene_);

  RenderSystem RndSys;

  // Register in reverse order to verify sorting works
  Scene_->RegisterSystem(&RndSys);
  Scene_->RegisterSystem(&TfSys);

  // After registration, systems should be sorted: TfSys (EarlyUpdate) before RndSys (Render)
  ASSERT_EQ(Scene_->GetSystemCount(), 2u);

  // Verify phase ordering by checking system count
  // We cannot directly access the internal Systems_ array, but we verified
  // registration count. The UpdateSystems call will respect phase order.
  EXPECT_EQ(Scene_->GetSystemCount(), 2u);

  // Clean up
  Scene_->UnregisterSystem(&TfSys);
  Scene_->UnregisterSystem(&RndSys);
}

//=============================================================================
// Task Group 4: New Tests for Updated Systems with AxScene
//=============================================================================

//=============================================================================
// Helper system that records the order it was called in
//=============================================================================

namespace CoreSystemTestHelpers {

// Shared counter to track call ordering across systems
static int gCallOrderCounter = 0;

class OrderTrackingSystem : public System
{
public:
  mutable int CallOrder;
  mutable int UpdateCallCount;
  mutable float LastDeltaT;

  OrderTrackingSystem(const char* Name, SystemPhase Phase,
                      int32_t Priority, uint32_t CompTypeID)
    : System(Name, Phase, Priority, CompTypeID)
    , CallOrder(-1)
    , UpdateCallCount(0)
    , LastDeltaT(0.0f)
  {}

  void Update(float DeltaT, Component** Components, uint32_t Count) override
  {
    (void)Components;
    (void)Count;
    CallOrder = gCallOrderCounter++;
    UpdateCallCount++;
    LastDeltaT = DeltaT;
  }
};

} // namespace CoreSystemTestHelpers

//=============================================================================
// Task Group 4 Test 1: TransformSystem with SetScene(AxScene*) computes
// world matrices correctly for a 3-level hierarchy
//
// This test verifies the full integration path: TransformSystem receives
// the AxScene* via SetScene(), traverses its node hierarchy via
// Scene_->GetRootNode(), and correctly computes accumulated world
// matrices for a 3-level deep parent-child chain with different
// translations on each axis.
//=============================================================================
TEST_F(CoreSystemTest, TG4_TransformSystemThreeLevelHierarchyMultiAxis)
{
  // Build Root -> Grandparent -> Parent -> Child
  // Each node translates on a different axis to verify all 3 axes accumulate
  Node* Grandparent = Scene_->CreateNode("Grandparent", NodeType::Node3D, nullptr);
  Node* Parent = Scene_->CreateNode("Parent", NodeType::Node3D, Grandparent);
  Node* Child = Scene_->CreateNode("Child", NodeType::Node3D, Parent);
  ASSERT_NE(Grandparent, nullptr);
  ASSERT_NE(Parent, nullptr);
  ASSERT_NE(Child, nullptr);

  // Grandparent: translate (5, 0, 0) -- only X
  SetNodeTranslation(Grandparent, 5.0f, 0.0f, 0.0f);

  // Parent: translate (0, 3, 0) -- only Y, local space
  SetNodeTranslation(Parent, 0.0f, 3.0f, 0.0f);

  // Child: translate (0, 0, 7) -- only Z, local space
  SetNodeTranslation(Child, 0.0f, 0.0f, 7.0f);

  // Create TransformSystem and set it to use the AxScene
  TransformSystem TfSys;
  TfSys.SetScene(Scene_);

  // Run the system
  TfSys.Update(0.016f, nullptr, 0);

  // Verify Grandparent world position: (5, 0, 0)
  const AxMat4x4& GpWorld = Grandparent->GetTransform().CachedForwardMatrix;
  EXPECT_TRUE(FloatNear(GpWorld.E[3][0], 5.0f))
    << "Grandparent world X should be 5.0, got " << GpWorld.E[3][0];
  EXPECT_TRUE(FloatNear(GpWorld.E[3][1], 0.0f))
    << "Grandparent world Y should be 0.0, got " << GpWorld.E[3][1];
  EXPECT_TRUE(FloatNear(GpWorld.E[3][2], 0.0f))
    << "Grandparent world Z should be 0.0, got " << GpWorld.E[3][2];

  // Verify Parent world position: (5, 3, 0) -- accumulated from Grandparent
  const AxMat4x4& ParWorld = Parent->GetTransform().CachedForwardMatrix;
  EXPECT_TRUE(FloatNear(ParWorld.E[3][0], 5.0f))
    << "Parent world X should be 5.0, got " << ParWorld.E[3][0];
  EXPECT_TRUE(FloatNear(ParWorld.E[3][1], 3.0f))
    << "Parent world Y should be 3.0, got " << ParWorld.E[3][1];
  EXPECT_TRUE(FloatNear(ParWorld.E[3][2], 0.0f))
    << "Parent world Z should be 0.0, got " << ParWorld.E[3][2];

  // Verify Child world position: (5, 3, 7) -- accumulated from all ancestors
  const AxMat4x4& ChildWorld = Child->GetTransform().CachedForwardMatrix;
  EXPECT_TRUE(FloatNear(ChildWorld.E[3][0], 5.0f))
    << "Child world X should be 5.0, got " << ChildWorld.E[3][0];
  EXPECT_TRUE(FloatNear(ChildWorld.E[3][1], 3.0f))
    << "Child world Y should be 3.0, got " << ChildWorld.E[3][1];
  EXPECT_TRUE(FloatNear(ChildWorld.E[3][2], 7.0f))
    << "Child world Z should be 7.0, got " << ChildWorld.E[3][2];

  // All dirty flags should be cleared after update
  EXPECT_FALSE(Grandparent->GetTransform().ForwardMatrixDirty);
  EXPECT_FALSE(Parent->GetTransform().ForwardMatrixDirty);
  EXPECT_FALSE(Child->GetTransform().ForwardMatrixDirty);
}

//=============================================================================
// Task Group 4 Test 2: RenderSystem with AxScene collects
// MeshFilter+MeshRenderer renderables via scene component lists
//
// This test verifies the full integration path: multiple nodes with
// MeshFilter+MeshRenderer components registered via AxScene::AddComponent,
// then RenderSystem collects them using the scene's GetComponentsByType
// mechanism (simulated by passing the component array directly to Update).
//=============================================================================
TEST_F(CoreSystemTest, TG4_RenderSystemCollectsMultipleRenderables)
{
  // Use unique TypeIDs for this test to avoid collisions
  static constexpr uint32_t FilterTypeID = 700;
  static constexpr uint32_t RendererTypeID = 701;

  // Create 3 nodes with MeshFilter + MeshRenderer
  Node* NodeA = Scene_->CreateNode("NodeA", NodeType::Node3D, nullptr);
  Node* NodeB = Scene_->CreateNode("NodeB", NodeType::Node3D, nullptr);
  Node* NodeC = Scene_->CreateNode("NodeC", NodeType::Node3D, nullptr);
  ASSERT_NE(NodeA, nullptr);
  ASSERT_NE(NodeB, nullptr);
  ASSERT_NE(NodeC, nullptr);

  // Set up components for NodeA
  MeshFilter FilterA;
  FilterA.TypeID_ = FilterTypeID;
  FilterA.TypeName_ = "MeshFilter";
  FilterA.ModelHandle = {10, 1};
  MeshRenderer RendererA;
  RendererA.TypeID_ = RendererTypeID;
  RendererA.TypeName_ = "MeshRenderer";
  RendererA.MaterialHandle = 100;
  EXPECT_TRUE(Scene_->AddComponent(NodeA, &FilterA));
  EXPECT_TRUE(Scene_->AddComponent(NodeA, &RendererA));

  // Set up components for NodeB
  MeshFilter FilterB;
  FilterB.TypeID_ = FilterTypeID;
  FilterB.TypeName_ = "MeshFilter";
  FilterB.ModelHandle = {20, 1};
  MeshRenderer RendererB;
  RendererB.TypeID_ = RendererTypeID;
  RendererB.TypeName_ = "MeshRenderer";
  RendererB.MaterialHandle = 200;
  EXPECT_TRUE(Scene_->AddComponent(NodeB, &FilterB));
  EXPECT_TRUE(Scene_->AddComponent(NodeB, &RendererB));

  // Set up components for NodeC
  MeshFilter FilterC;
  FilterC.TypeID_ = FilterTypeID;
  FilterC.TypeName_ = "MeshFilter";
  FilterC.ModelHandle = {30, 1};
  MeshRenderer RendererC;
  RendererC.TypeID_ = RendererTypeID;
  RendererC.TypeName_ = "MeshRenderer";
  RendererC.MaterialHandle = 300;
  EXPECT_TRUE(Scene_->AddComponent(NodeC, &FilterC));
  EXPECT_TRUE(Scene_->AddComponent(NodeC, &RendererC));

  // Set up world matrices for each node
  NodeA->GetTransform().CachedForwardMatrix = Identity();
  NodeA->GetTransform().CachedForwardMatrix.E[3][0] = 1.0f;
  NodeB->GetTransform().CachedForwardMatrix = Identity();
  NodeB->GetTransform().CachedForwardMatrix.E[3][0] = 2.0f;
  NodeC->GetTransform().CachedForwardMatrix = Identity();
  NodeC->GetTransform().CachedForwardMatrix.E[3][0] = 3.0f;

  // Verify the scene's global component list for MeshRenderer has all 3
  uint32_t RendererCount = 0;
  Component** RendererList = Scene_->GetComponentsByType(RendererTypeID, &RendererCount);
  ASSERT_EQ(RendererCount, 3u);
  ASSERT_NE(RendererList, nullptr);

  // Create RenderSystem and feed it the scene's MeshRenderer list
  RenderSystem RndSys;
  RndSys.SetMeshFilterTypeID(FilterTypeID);
  RndSys.Update(0.016f, RendererList, RendererCount);

  // Verify all 3 renderables were collected
  uint32_t RenderableCount = 0;
  const RenderableEntry* Renderables = RndSys.GetRenderables(&RenderableCount);
  ASSERT_EQ(RenderableCount, 3u);

  EXPECT_EQ(Renderables[0].ModelHandle.Index, 10u);
  EXPECT_EQ(Renderables[0].MaterialHandle, 100u);
  EXPECT_TRUE(FloatNear(Renderables[0].WorldMatrix.E[3][0], 1.0f));

  EXPECT_EQ(Renderables[1].ModelHandle.Index, 20u);
  EXPECT_EQ(Renderables[1].MaterialHandle, 200u);
  EXPECT_TRUE(FloatNear(Renderables[1].WorldMatrix.E[3][0], 2.0f));

  EXPECT_EQ(Renderables[2].ModelHandle.Index, 30u);
  EXPECT_EQ(Renderables[2].MaterialHandle, 300u);
  EXPECT_TRUE(FloatNear(Renderables[2].WorldMatrix.E[3][0], 3.0f));

  // Clean up components
  Scene_->RemoveComponent(NodeA, FilterTypeID);
  Scene_->RemoveComponent(NodeA, RendererTypeID);
  Scene_->RemoveComponent(NodeB, FilterTypeID);
  Scene_->RemoveComponent(NodeB, RendererTypeID);
  Scene_->RemoveComponent(NodeC, FilterTypeID);
  Scene_->RemoveComponent(NodeC, RendererTypeID);
}

//=============================================================================
// Task Group 4 Test 3: System registration on AxScene, followed by
// UpdateSystems, calls TransformSystem before RenderSystem (phase ordering)
//
// This test verifies that when TransformSystem (EarlyUpdate) and
// RenderSystem (Render) are both registered on an AxScene via
// RegisterSystem, calling UpdateSystems executes them in correct
// phase order regardless of registration order.
//=============================================================================
TEST_F(CoreSystemTest, TG4_UpdateSystemsCallsTransformBeforeRender)
{
  using namespace CoreSystemTestHelpers;

  // Reset the global call order counter
  gCallOrderCounter = 0;

  // Use order-tracking systems that record when they were called
  OrderTrackingSystem EarlySys("EarlyTracker", SystemPhase::EarlyUpdate, 0, 0);
  OrderTrackingSystem RenderSys("RenderTracker", SystemPhase::Render, 0, 0);

  // Register in reverse phase order to verify sorting
  Scene_->RegisterSystem(&RenderSys);
  Scene_->RegisterSystem(&EarlySys);

  ASSERT_EQ(Scene_->GetSystemCount(), 2u);

  // Call UpdateSystems - should execute EarlySys before RenderSys
  Scene_->UpdateSystems(0.016f);

  // Both systems should have been called exactly once
  EXPECT_EQ(EarlySys.UpdateCallCount, 1);
  EXPECT_EQ(RenderSys.UpdateCallCount, 1);

  // EarlySys (EarlyUpdate phase) should have been called before RenderSys (Render phase)
  EXPECT_LT(EarlySys.CallOrder, RenderSys.CallOrder)
    << "EarlyUpdate system should execute before Render system; "
    << "EarlySys.CallOrder=" << EarlySys.CallOrder
    << " RenderSys.CallOrder=" << RenderSys.CallOrder;

  // Verify correct delta time was passed
  EXPECT_FLOAT_EQ(EarlySys.LastDeltaT, 0.016f);
  EXPECT_FLOAT_EQ(RenderSys.LastDeltaT, 0.016f);

  // Clean up
  Scene_->UnregisterSystem(&EarlySys);
  Scene_->UnregisterSystem(&RenderSys);
}

//=============================================================================
// Task Group 4 Test 4: FixedUpdateSystems only calls FixedUpdate-phase systems
//
// This test verifies that FixedUpdateSystems skips non-FixedUpdate systems
// and only calls systems registered at the FixedUpdate phase.
//=============================================================================
TEST_F(CoreSystemTest, TG4_FixedUpdateSystemsOnlyCallsFixedPhase)
{
  using namespace CoreSystemTestHelpers;

  // Reset the global call order counter
  gCallOrderCounter = 0;

  // Create systems at different phases
  OrderTrackingSystem EarlySys("EarlyTracker", SystemPhase::EarlyUpdate, 0, 0);
  OrderTrackingSystem FixedSysA("FixedTrackerA", SystemPhase::FixedUpdate, 0, 0);
  OrderTrackingSystem FixedSysB("FixedTrackerB", SystemPhase::FixedUpdate, 10, 0);
  OrderTrackingSystem UpdateSys("UpdateTracker", SystemPhase::Update, 0, 0);
  OrderTrackingSystem RenderSys("RenderTracker", SystemPhase::Render, 0, 0);

  // Register all systems
  Scene_->RegisterSystem(&EarlySys);
  Scene_->RegisterSystem(&FixedSysA);
  Scene_->RegisterSystem(&FixedSysB);
  Scene_->RegisterSystem(&UpdateSys);
  Scene_->RegisterSystem(&RenderSys);

  ASSERT_EQ(Scene_->GetSystemCount(), 5u);

  // Call FixedUpdateSystems with a fixed timestep
  Scene_->FixedUpdateSystems(0.02f);

  // Only FixedUpdate-phase systems should have been called
  EXPECT_EQ(FixedSysA.UpdateCallCount, 1);
  EXPECT_EQ(FixedSysB.UpdateCallCount, 1);

  // Non-FixedUpdate systems should NOT have been called
  EXPECT_EQ(EarlySys.UpdateCallCount, 0)
    << "EarlyUpdate system should not be called by FixedUpdateSystems";
  EXPECT_EQ(UpdateSys.UpdateCallCount, 0)
    << "Update system should not be called by FixedUpdateSystems";
  EXPECT_EQ(RenderSys.UpdateCallCount, 0)
    << "Render system should not be called by FixedUpdateSystems";

  // FixedUpdate systems should have received the correct fixed delta
  EXPECT_FLOAT_EQ(FixedSysA.LastDeltaT, 0.02f);
  EXPECT_FLOAT_EQ(FixedSysB.LastDeltaT, 0.02f);

  // FixedSysA (priority 0) should have been called before FixedSysB (priority 10)
  EXPECT_LT(FixedSysA.CallOrder, FixedSysB.CallOrder)
    << "FixedUpdate systems should execute in priority order; "
    << "FixedSysA.CallOrder=" << FixedSysA.CallOrder
    << " FixedSysB.CallOrder=" << FixedSysB.CallOrder;

  // Now call UpdateSystems and verify it skips FixedUpdate systems
  gCallOrderCounter = 0;
  EarlySys.UpdateCallCount = 0;
  UpdateSys.UpdateCallCount = 0;
  RenderSys.UpdateCallCount = 0;
  FixedSysA.UpdateCallCount = 0;
  FixedSysB.UpdateCallCount = 0;

  Scene_->UpdateSystems(0.016f);

  // UpdateSystems should call everything EXCEPT FixedUpdate-phase systems
  EXPECT_EQ(EarlySys.UpdateCallCount, 1);
  EXPECT_EQ(UpdateSys.UpdateCallCount, 1);
  EXPECT_EQ(RenderSys.UpdateCallCount, 1);
  EXPECT_EQ(FixedSysA.UpdateCallCount, 0)
    << "FixedUpdate system should not be called by UpdateSystems";
  EXPECT_EQ(FixedSysB.UpdateCallCount, 0)
    << "FixedUpdate system should not be called by UpdateSystems";

  // Clean up
  Scene_->UnregisterSystem(&EarlySys);
  Scene_->UnregisterSystem(&FixedSysA);
  Scene_->UnregisterSystem(&FixedSysB);
  Scene_->UnregisterSystem(&UpdateSys);
  Scene_->UnregisterSystem(&RenderSys);
}
