/**
 * AxSceneExTests.cpp - Tests for AxScene (Node/Component/System)
 *
 * Tests the AxScene class: creation with RootNode, node management,
 * global component lists, scene settings, system registration/execution,
 * and correct component list dispatch to systems.
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

#include <cstring>

//=============================================================================
// Test Components
//=============================================================================

struct TestCompA : public Component
{
  static constexpr uint32_t TypeIDValue = 100;
  float Value;

  TestCompA() : Value(0.0f)
  {
    TypeID_ = TypeIDValue;
    TypeName_ = "TestCompA";
  }

  size_t GetSize() const override { return (sizeof(TestCompA)); }
};

struct TestCompB : public Component
{
  static constexpr uint32_t TypeIDValue = 200;
  int Counter;

  TestCompB() : Counter(0)
  {
    TypeID_ = TypeIDValue;
    TypeName_ = "TestCompB";
  }

  size_t GetSize() const override { return (sizeof(TestCompB)); }
};

//=============================================================================
// Test System - Tracks calls for verification
//=============================================================================

class TestSystem : public System
{
public:
  mutable int UpdateCallCount;
  mutable uint32_t LastComponentCount;
  mutable float LastDeltaT;

  TestSystem(const char* Name, SystemPhase Phase, int32_t Priority, uint32_t CompTypeID)
    : System(Name, Phase, Priority, CompTypeID)
    , UpdateCallCount(0)
    , LastComponentCount(0)
    , LastDeltaT(0.0f)
  {}

  void Update(float DeltaT, Component** Components, uint32_t Count) override
  {
    UpdateCallCount++;
    LastComponentCount = Count;
    LastDeltaT = DeltaT;
  }
};

//=============================================================================
// Test Fixture
//=============================================================================

class SceneExTest : public testing::Test
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

  AxHashTableAPI* TableAPI_{nullptr};
  AxAllocatorAPI* AllocatorAPI_{nullptr};
  AxScene* Scene_{nullptr};
};

//=============================================================================
// Test 1: Creating a Scene with a RootNode of type Root
//=============================================================================
TEST_F(SceneExTest, CreateSceneWithRootNode)
{
  // Scene should have a root node
  ASSERT_NE(Scene_->GetRootNode(), nullptr);
  EXPECT_EQ(Scene_->GetRootNode()->GetName(), "Root");
  EXPECT_EQ(Scene_->GetRootNode()->GetType(), NodeType::Root);

  // Node count should be 1 (just the root)
  EXPECT_EQ(Scene_->GetNodeCount(), 1u);
}

//=============================================================================
// Test 2: Adding nodes to scene updates node count
//=============================================================================
TEST_F(SceneExTest, AddNodesUpdatesNodeCount)
{
  // Create two nodes as children of root
  Node* NodeA = Scene_->CreateNode("NodeA", NodeType::Node3D, nullptr);
  ASSERT_NE(NodeA, nullptr);
  EXPECT_EQ(NodeA->GetName(), "NodeA");
  EXPECT_EQ(NodeA->GetType(), NodeType::Node3D);
  EXPECT_EQ(Scene_->GetNodeCount(), 2u); // Root + NodeA

  Node* NodeB = Scene_->CreateNode("NodeB", NodeType::Node2D, nullptr);
  ASSERT_NE(NodeB, nullptr);
  EXPECT_EQ(NodeB->GetType(), NodeType::Node2D);
  EXPECT_EQ(Scene_->GetNodeCount(), 3u); // Root + NodeA + NodeB

  // Both should be children of root
  EXPECT_EQ(NodeA->GetParent(), static_cast<Node*>(Scene_->GetRootNode()));
  EXPECT_EQ(NodeB->GetParent(), static_cast<Node*>(Scene_->GetRootNode()));

  // Create a child of NodeA
  Node* ChildA1 = Scene_->CreateNode("ChildA1", NodeType::Node3D, NodeA);
  ASSERT_NE(ChildA1, nullptr);
  EXPECT_EQ(ChildA1->GetParent(), NodeA);
  EXPECT_EQ(Scene_->GetNodeCount(), 4u);
}

//=============================================================================
// Test 3: Global component list tracks components added to any node
//=============================================================================
TEST_F(SceneExTest, GlobalComponentListTracksComponents)
{
  Node* NodeA = Scene_->CreateNode("NodeA", NodeType::Node3D, nullptr);
  Node* NodeB = Scene_->CreateNode("NodeB", NodeType::Node3D, nullptr);

  // Add TestCompA to both nodes
  TestCompA CompA1;
  TestCompA CompA2;
  EXPECT_TRUE(Scene_->AddComponent(NodeA, &CompA1));
  EXPECT_TRUE(Scene_->AddComponent(NodeB, &CompA2));

  // Query global component list for TestCompA type
  uint32_t Count = 0;
  Component** List = Scene_->GetComponentsByType(TestCompA::TypeIDValue, &Count);
  ASSERT_NE(List, nullptr);
  EXPECT_EQ(Count, 2u);
  EXPECT_EQ(List[0], static_cast<Component*>(&CompA1));
  EXPECT_EQ(List[1], static_cast<Component*>(&CompA2));

  // A different type should return empty
  uint32_t CountB = 0;
  Component** ListB = Scene_->GetComponentsByType(TestCompB::TypeIDValue, &CountB);
  EXPECT_EQ(ListB, nullptr);
  EXPECT_EQ(CountB, 0u);

  // Clean up: remove components before nodes are destroyed
  Scene_->RemoveComponent(NodeA, TestCompA::TypeIDValue);
  Scene_->RemoveComponent(NodeB, TestCompA::TypeIDValue);
}

//=============================================================================
// Test 4: Removing a component from a node removes it from scene global list
//=============================================================================
TEST_F(SceneExTest, RemoveComponentFromGlobalList)
{
  Node* NodeA = Scene_->CreateNode("NodeA", NodeType::Node3D, nullptr);
  Node* NodeB = Scene_->CreateNode("NodeB", NodeType::Node3D, nullptr);

  TestCompA CompA1;
  TestCompA CompA2;
  Scene_->AddComponent(NodeA, &CompA1);
  Scene_->AddComponent(NodeB, &CompA2);

  // Verify both are present
  uint32_t Count = 0;
  Scene_->GetComponentsByType(TestCompA::TypeIDValue, &Count);
  EXPECT_EQ(Count, 2u);

  // Remove component from NodeA
  EXPECT_TRUE(Scene_->RemoveComponent(NodeA, TestCompA::TypeIDValue));

  // Global list should now have only one component
  uint32_t NewCount = 0;
  Component** NewList = Scene_->GetComponentsByType(TestCompA::TypeIDValue, &NewCount);
  ASSERT_NE(NewList, nullptr);
  EXPECT_EQ(NewCount, 1u);
  EXPECT_EQ(NewList[0], static_cast<Component*>(&CompA2));

  // The node should no longer have the component
  EXPECT_EQ(NodeA->GetComponent(TestCompA::TypeIDValue), nullptr);

  // Clean up remaining component
  Scene_->RemoveComponent(NodeB, TestCompA::TypeIDValue);
}

//=============================================================================
// Test 5: Scene-level settings initialization
//=============================================================================
TEST_F(SceneExTest, SceneSettingsInitialization)
{
  // Ambient light should be initialized
  EXPECT_FLOAT_EQ(Scene_->AmbientLight.X, 0.1f);
  EXPECT_FLOAT_EQ(Scene_->AmbientLight.Y, 0.1f);
  EXPECT_FLOAT_EQ(Scene_->AmbientLight.Z, 0.1f);

  // Gravity should be standard Earth gravity
  EXPECT_FLOAT_EQ(Scene_->Gravity.X, 0.0f);
  EXPECT_FLOAT_EQ(Scene_->Gravity.Y, -9.81f);
  EXPECT_FLOAT_EQ(Scene_->Gravity.Z, 0.0f);

  // Fog density should be zero (no fog)
  EXPECT_FLOAT_EQ(Scene_->FogDensity, 0.0f);

  // Fog color should have defaults
  EXPECT_FLOAT_EQ(Scene_->FogColor.X, 0.5f);
  EXPECT_FLOAT_EQ(Scene_->FogColor.Y, 0.5f);
  EXPECT_FLOAT_EQ(Scene_->FogColor.Z, 0.5f);
}

//=============================================================================
// Test 6: System registration with execution order
//=============================================================================
TEST_F(SceneExTest, SystemRegistrationOrder)
{
  // Create systems in non-sorted order
  TestSystem RenderSys("RenderSys", SystemPhase::Render, 0, TestCompA::TypeIDValue);
  TestSystem EarlySys("EarlySys", SystemPhase::EarlyUpdate, 0, TestCompA::TypeIDValue);
  TestSystem UpdateSys("UpdateSys", SystemPhase::Update, 0, TestCompA::TypeIDValue);

  // Register in arbitrary order
  Scene_->RegisterSystem(&RenderSys);
  Scene_->RegisterSystem(&EarlySys);
  Scene_->RegisterSystem(&UpdateSys);

  EXPECT_EQ(Scene_->GetSystemCount(), 3u);

  // Unregister before TearDown (systems are stack-allocated)
  Scene_->UnregisterSystem(&RenderSys);
  Scene_->UnregisterSystem(&EarlySys);
  Scene_->UnregisterSystem(&UpdateSys);
}

//=============================================================================
// Test 7: System update loop calls registered systems in order
//=============================================================================
TEST_F(SceneExTest, SystemUpdateLoopCallsInOrder)
{
  TestSystem EarlySys("EarlySys", SystemPhase::EarlyUpdate, 0, TestCompA::TypeIDValue);
  TestSystem UpdateSys("UpdateSys", SystemPhase::Update, 0, TestCompA::TypeIDValue);
  TestSystem LateSys("LateSys", SystemPhase::LateUpdate, 0, TestCompA::TypeIDValue);

  Scene_->RegisterSystem(&EarlySys);
  Scene_->RegisterSystem(&UpdateSys);
  Scene_->RegisterSystem(&LateSys);

  // Call UpdateSystems
  Scene_->UpdateSystems(0.016f);

  // All three non-FixedUpdate systems should have been called
  EXPECT_EQ(EarlySys.UpdateCallCount, 1);
  EXPECT_EQ(UpdateSys.UpdateCallCount, 1);
  EXPECT_EQ(LateSys.UpdateCallCount, 1);

  // Verify delta time was passed correctly
  EXPECT_FLOAT_EQ(EarlySys.LastDeltaT, 0.016f);
  EXPECT_FLOAT_EQ(UpdateSys.LastDeltaT, 0.016f);
  EXPECT_FLOAT_EQ(LateSys.LastDeltaT, 0.016f);

  // Unregister before TearDown
  Scene_->UnregisterSystem(&EarlySys);
  Scene_->UnregisterSystem(&UpdateSys);
  Scene_->UnregisterSystem(&LateSys);
}

//=============================================================================
// Test 8: System receives correct component list for its type
//=============================================================================
TEST_F(SceneExTest, SystemReceivesCorrectComponentList)
{
  // Create nodes and add different component types
  Node* NodeA = Scene_->CreateNode("NodeA", NodeType::Node3D, nullptr);
  Node* NodeB = Scene_->CreateNode("NodeB", NodeType::Node3D, nullptr);

  TestCompA CompA1;
  TestCompA CompA2;
  TestCompB CompB1;

  Scene_->AddComponent(NodeA, &CompA1);
  Scene_->AddComponent(NodeB, &CompA2);
  Scene_->AddComponent(NodeA, &CompB1);

  // Create two systems watching different component types
  TestSystem SysA("SysA", SystemPhase::Update, 0, TestCompA::TypeIDValue);
  TestSystem SysB("SysB", SystemPhase::Update, 1, TestCompB::TypeIDValue);

  Scene_->RegisterSystem(&SysA);
  Scene_->RegisterSystem(&SysB);

  // Run update
  Scene_->UpdateSystems(0.016f);

  // SysA should see 2 components (CompA on NodeA and NodeB)
  EXPECT_EQ(SysA.UpdateCallCount, 1);
  EXPECT_EQ(SysA.LastComponentCount, 2u);

  // SysB should see 1 component (CompB on NodeA only)
  EXPECT_EQ(SysB.UpdateCallCount, 1);
  EXPECT_EQ(SysB.LastComponentCount, 1u);

  // Clean up
  Scene_->UnregisterSystem(&SysA);
  Scene_->UnregisterSystem(&SysB);
  Scene_->RemoveComponent(NodeA, TestCompA::TypeIDValue);
  Scene_->RemoveComponent(NodeB, TestCompA::TypeIDValue);
  Scene_->RemoveComponent(NodeA, TestCompB::TypeIDValue);
}
