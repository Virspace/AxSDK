/**
 * AxSceneExTests.cpp - Tests for SceneTree (formerly AxScene)
 *
 * Tests the SceneTree class: creation with RootNode, node management,
 * typed-node tracking lists, and scene settings.
 *
 * Component-based tests have been replaced with typed-node tests as part
 * of the migration to Godot-style typed node subclasses.
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

#include <cstring>

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

    // Create SceneTree directly
    Tree_ = new SceneTree(TableAPI_, nullptr);
    ASSERT_NE(Tree_, nullptr);
    Tree_->Name = "TestScene";
  }

  void TearDown() override
  {
    delete Tree_;
    Tree_ = nullptr;

    AxonTermGlobalAPIRegistry();
  }

  AxHashTableAPI* TableAPI_{nullptr};
  AxAllocatorAPI* AllocatorAPI_{nullptr};
  SceneTree* Tree_{nullptr};
};

//=============================================================================
// Test 1: Creating a Scene with a RootNode of type Root
//=============================================================================
TEST_F(SceneExTest, CreateSceneWithRootNode)
{
  // Scene should have a root node
  ASSERT_NE(Tree_->GetRootNode(), nullptr);
  EXPECT_EQ(Tree_->GetRootNode()->GetName(), "Root");
  EXPECT_EQ(Tree_->GetRootNode()->GetType(), NodeType::Root);

  // Node count should be 1 (just the root)
  EXPECT_EQ(Tree_->GetNodeCount(), 1u);
}

//=============================================================================
// Test 2: Adding nodes to scene updates node count
//=============================================================================
TEST_F(SceneExTest, AddNodesUpdatesNodeCount)
{
  // Create two nodes as children of root
  Node* NodeA = Tree_->CreateNode("NodeA", NodeType::Node3D, nullptr);
  ASSERT_NE(NodeA, nullptr);
  EXPECT_EQ(NodeA->GetName(), "NodeA");
  EXPECT_EQ(NodeA->GetType(), NodeType::Node3D);
  EXPECT_EQ(Tree_->GetNodeCount(), 2u); // Root + NodeA

  Node* NodeB = Tree_->CreateNode("NodeB", NodeType::Node2D, nullptr);
  ASSERT_NE(NodeB, nullptr);
  EXPECT_EQ(NodeB->GetType(), NodeType::Node2D);
  EXPECT_EQ(Tree_->GetNodeCount(), 3u); // Root + NodeA + NodeB

  // Both should be children of root
  EXPECT_EQ(NodeA->GetParent(), static_cast<Node*>(Tree_->GetRootNode()));
  EXPECT_EQ(NodeB->GetParent(), static_cast<Node*>(Tree_->GetRootNode()));

  // Create a child of NodeA
  Node* ChildA1 = Tree_->CreateNode("ChildA1", NodeType::Node3D, NodeA);
  ASSERT_NE(ChildA1, nullptr);
  EXPECT_EQ(ChildA1->GetParent(), NodeA);
  EXPECT_EQ(Tree_->GetNodeCount(), 4u);
}

//=============================================================================
// Test 3: Typed node tracking lists track nodes added to the scene
//=============================================================================
TEST_F(SceneExTest, TypedNodeListTracksNodes)
{
  // Create MeshInstance nodes
  Node* MeshA = Tree_->CreateNode("MeshA", NodeType::MeshInstance, nullptr);
  Node* MeshB = Tree_->CreateNode("MeshB", NodeType::MeshInstance, nullptr);
  ASSERT_NE(MeshA, nullptr);
  ASSERT_NE(MeshB, nullptr);

  // Query typed node list for MeshInstance
  uint32_t Count = 0;
  Node** List = Tree_->GetNodesByType(NodeType::MeshInstance, &Count);
  ASSERT_NE(List, nullptr);
  EXPECT_EQ(Count, 2u);
  EXPECT_EQ(List[0], MeshA);
  EXPECT_EQ(List[1], MeshB);

  // A different type should return empty
  uint32_t LightCount = 0;
  Node** LightList = Tree_->GetNodesByType(NodeType::Light, &LightCount);
  EXPECT_EQ(LightList, nullptr);
  EXPECT_EQ(LightCount, 0u);

  // Verify the typed nodes carry correct data
  MeshInstance* MI = static_cast<MeshInstance*>(MeshA);
  MI->SetMeshPath("models/test.glb");
  EXPECT_EQ(MI->GetMeshPath(), "models/test.glb");
}

//=============================================================================
// Test 4: Destroying a node removes it from typed node tracking list
//=============================================================================
TEST_F(SceneExTest, DestroyNodeRemovesFromTypedList)
{
  Node* MeshA = Tree_->CreateNode("MeshA", NodeType::MeshInstance, nullptr);
  Node* MeshB = Tree_->CreateNode("MeshB", NodeType::MeshInstance, nullptr);

  // Verify both are present
  uint32_t Count = 0;
  Tree_->GetNodesByType(NodeType::MeshInstance, &Count);
  EXPECT_EQ(Count, 2u);

  // Destroy MeshA
  Tree_->DestroyNode(MeshA);

  // Typed list should now have only one node
  uint32_t NewCount = 0;
  Node** NewList = Tree_->GetNodesByType(NodeType::MeshInstance, &NewCount);
  ASSERT_NE(NewList, nullptr);
  EXPECT_EQ(NewCount, 1u);
  EXPECT_EQ(NewList[0], MeshB);
}

//=============================================================================
// Test 5: Scene-level settings initialization
//=============================================================================
TEST_F(SceneExTest, SceneSettingsInitialization)
{
  // Ambient light should be initialized
  EXPECT_FLOAT_EQ(Tree_->AmbientLight.X, 0.1f);
  EXPECT_FLOAT_EQ(Tree_->AmbientLight.Y, 0.1f);
  EXPECT_FLOAT_EQ(Tree_->AmbientLight.Z, 0.1f);

  // Gravity should be standard Earth gravity
  EXPECT_FLOAT_EQ(Tree_->Gravity.X, 0.0f);
  EXPECT_FLOAT_EQ(Tree_->Gravity.Y, -9.81f);
  EXPECT_FLOAT_EQ(Tree_->Gravity.Z, 0.0f);

  // Fog density should be zero (no fog)
  EXPECT_FLOAT_EQ(Tree_->FogDensity, 0.0f);

  // Fog color should have defaults
  EXPECT_FLOAT_EQ(Tree_->FogColor.X, 0.5f);
  EXPECT_FLOAT_EQ(Tree_->FogColor.Y, 0.5f);
  EXPECT_FLOAT_EQ(Tree_->FogColor.Z, 0.5f);
}
