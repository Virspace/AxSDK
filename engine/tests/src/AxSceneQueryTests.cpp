/**
 * AxSceneQueryTests.cpp - Tests for Scene Query APIs
 *
 * Tests: GetChildren iteration, GetNode path resolution, GetNode<T> typed
 * variant, FindChildByType<T>, string-based groups, and ScriptBase wrappers.
 */

#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxHashTable.h"
#include "Foundation/AxAPIRegistry.h"
#include "AxEngine/AxNode.h"
#include "AxEngine/AxTypedNodes.h"
#include "AxEngine/AxSceneTree.h"
#include "AxEngine/AxScriptBase.h"

#include <string>
#include <vector>

//=============================================================================
// Test Fixture
//=============================================================================

class SceneQueryTest : public testing::Test
{
protected:
  void SetUp() override
  {
    AxonInitGlobalAPIRegistry();
    AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);
    TableAPI_ = static_cast<AxHashTableAPI*>(
      AxonGlobalAPIRegistry->Get(AXON_HASH_TABLE_API_NAME));
    ASSERT_NE(TableAPI_, nullptr);

    Tree_ = new SceneTree(TableAPI_, nullptr);
    Tree_->Name = "TestScene";
  }

  void TearDown() override
  {
    delete Tree_;
    AxonTermGlobalAPIRegistry();
  }

  AxHashTableAPI* TableAPI_{nullptr};
  SceneTree* Tree_{nullptr};
};

//=============================================================================
// Children Iteration Tests
//=============================================================================

TEST_F(SceneQueryTest, GetChildren_NoChildren_EmptyRange)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  int Count = 0;
  for (Node* C : A->GetChildren()) {
    (void)C;
    Count++;
  }
  EXPECT_EQ(Count, 0);
}

TEST_F(SceneQueryTest, GetChildren_SingleChild_IteratesOnce)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  Node* B = Tree_->CreateNode("B", NodeType::Node3D, A);

  std::vector<Node*> Children;
  for (Node* C : A->GetChildren()) {
    Children.push_back(C);
  }
  ASSERT_EQ(Children.size(), 1u);
  EXPECT_EQ(Children[0], B);
}

TEST_F(SceneQueryTest, GetChildren_MultipleChildren_IteratesAllInOrder)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  Node* B = Tree_->CreateNode("B", NodeType::Node3D, A);
  Node* C = Tree_->CreateNode("C", NodeType::Node3D, A);
  Node* D = Tree_->CreateNode("D", NodeType::Node3D, A);

  std::vector<std::string> Names;
  for (Node* Child : A->GetChildren()) {
    Names.push_back(std::string(Child->GetName()));
  }
  ASSERT_EQ(Names.size(), 3u);
  EXPECT_EQ(Names[0], "B");
  EXPECT_EQ(Names[1], "C");
  EXPECT_EQ(Names[2], "D");
}

//=============================================================================
// Relative Path Query Tests
//=============================================================================

TEST_F(SceneQueryTest, GetNode_EmptyPath_ReturnsSelf)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  EXPECT_EQ(A->GetNode(""), A);
}

TEST_F(SceneQueryTest, GetNode_DirectChild_FindsByName)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  Node* B = Tree_->CreateNode("Camera", NodeType::Camera, A);
  EXPECT_EQ(A->GetNode("Camera"), B);
}

TEST_F(SceneQueryTest, GetNode_NestedPath_TraversesMultipleLevels)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  Node* Player = Tree_->CreateNode("Player", NodeType::Node3D, A);
  Node* Cam = Tree_->CreateNode("Camera", NodeType::Camera, Player);
  EXPECT_EQ(A->GetNode("Player/Camera"), Cam);
}

TEST_F(SceneQueryTest, GetNode_ParentTraversal_DotDotResolvesParent)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  Node* B = Tree_->CreateNode("B", NodeType::Node3D, A);
  EXPECT_EQ(B->GetNode(".."), A);
}

TEST_F(SceneQueryTest, GetNode_ParentThenChild_MixedTraversal)
{
  Node* Parent = Tree_->CreateNode("Parent", NodeType::Node3D);
  Node* A = Tree_->CreateNode("A", NodeType::Node3D, Parent);
  Node* B = Tree_->CreateNode("B", NodeType::Node3D, Parent);
  Node* C = Tree_->CreateNode("C", NodeType::Node3D, B);
  EXPECT_EQ(A->GetNode("../B/C"), C);
}

TEST_F(SceneQueryTest, GetNode_MultipleParentSteps_ClimbsHierarchy)
{
  Node* GP = Tree_->CreateNode("GP", NodeType::Node3D);
  Node* P = Tree_->CreateNode("P", NodeType::Node3D, GP);
  Node* Child = Tree_->CreateNode("Child", NodeType::Node3D, P);
  EXPECT_EQ(Child->GetNode("../.."), GP);
}

TEST_F(SceneQueryTest, GetNode_ParentAtRoot_ReturnsNullptr)
{
  // Root's parent is nullptr, so going up from Root returns nullptr
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  // A's parent is Root. Root's parent is nullptr.
  Node* Root = A->GetNode("..");
  ASSERT_NE(Root, nullptr);
  EXPECT_EQ(Root->GetNode(".."), nullptr);
}

TEST_F(SceneQueryTest, GetNode_NonexistentChild_ReturnsNullptr)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  EXPECT_EQ(A->GetNode("DoesNotExist"), nullptr);
}

TEST_F(SceneQueryTest, GetNode_PartialPathFails_ReturnsNullptr)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  Node* B = Tree_->CreateNode("Exists", NodeType::Node3D, A);
  (void)B;
  EXPECT_EQ(A->GetNode("Exists/DoesNotExist"), nullptr);
}

TEST_F(SceneQueryTest, GetNode_TrailingSlash_Ignored)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  Node* B = Tree_->CreateNode("Camera", NodeType::Camera, A);
  EXPECT_EQ(A->GetNode("Camera/"), B);
}

//=============================================================================
// Typed Path Query Tests
//=============================================================================

TEST_F(SceneQueryTest, GetNodeTyped_CorrectType_ReturnsTypedPointer)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  Tree_->CreateNode("Camera", NodeType::Camera, A);

  CameraNode* Cam = A->GetNode<CameraNode>("Camera");
  ASSERT_NE(Cam, nullptr);
  EXPECT_EQ(Cam->GetType(), NodeType::Camera);
}

TEST_F(SceneQueryTest, GetNodeTyped_WrongType_ReturnsNullptr)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  Tree_->CreateNode("Camera", NodeType::Camera, A);

  LightNode* Light = A->GetNode<LightNode>("Camera");
  EXPECT_EQ(Light, nullptr);
}

TEST_F(SceneQueryTest, GetNodeTyped_NotFound_ReturnsNullptr)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  CameraNode* Cam = A->GetNode<CameraNode>("Nope");
  EXPECT_EQ(Cam, nullptr);
}

//=============================================================================
// FindChildByType Tests
//=============================================================================

TEST_F(SceneQueryTest, FindChildByType_Exists_ReturnsFirst)
{
  Node* Parent = Tree_->CreateNode("Parent", NodeType::Node3D);
  Tree_->CreateNode("N1", NodeType::Node3D, Parent);
  Node* Cam = Tree_->CreateNode("Cam", NodeType::Camera, Parent);

  CameraNode* Found = Parent->FindChildByType<CameraNode>();
  EXPECT_EQ(Found, Cam);
}

TEST_F(SceneQueryTest, FindChildByType_MultipleMatches_ReturnsFirst)
{
  Node* Parent = Tree_->CreateNode("Parent", NodeType::Node3D);
  Node* Cam1 = Tree_->CreateNode("Cam1", NodeType::Camera, Parent);
  Tree_->CreateNode("Cam2", NodeType::Camera, Parent);

  CameraNode* Found = Parent->FindChildByType<CameraNode>();
  EXPECT_EQ(Found, Cam1);
}

TEST_F(SceneQueryTest, FindChildByType_NoMatch_ReturnsNullptr)
{
  Node* Parent = Tree_->CreateNode("Parent", NodeType::Node3D);
  Tree_->CreateNode("N1", NodeType::Node3D, Parent);

  CameraNode* Found = Parent->FindChildByType<CameraNode>();
  EXPECT_EQ(Found, nullptr);
}

TEST_F(SceneQueryTest, FindChildByType_NoChildren_ReturnsNullptr)
{
  Node* Leaf = Tree_->CreateNode("Leaf", NodeType::Node3D);
  CameraNode* Found = Leaf->FindChildByType<CameraNode>();
  EXPECT_EQ(Found, nullptr);
}

//=============================================================================
// Group Membership Tests
//=============================================================================

TEST_F(SceneQueryTest, AddToGroup_SingleGroup_NodeIsInGroup)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  A->AddToGroup("enemies");
  EXPECT_TRUE(A->IsInGroup("enemies"));
}

TEST_F(SceneQueryTest, AddToGroup_MultipleGroups_NodeInAll)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  A->AddToGroup("enemies");
  A->AddToGroup("damageable");
  EXPECT_TRUE(A->IsInGroup("enemies"));
  EXPECT_TRUE(A->IsInGroup("damageable"));
}

TEST_F(SceneQueryTest, AddToGroup_Duplicate_NoDuplicateEntry)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  A->AddToGroup("enemies");
  A->AddToGroup("enemies");
  EXPECT_EQ(Tree_->GetGroupSize("enemies"), 1u);
}

TEST_F(SceneQueryTest, RemoveFromGroup_Exists_NodeNoLongerInGroup)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  A->AddToGroup("enemies");
  A->RemoveFromGroup("enemies");
  EXPECT_FALSE(A->IsInGroup("enemies"));
}

TEST_F(SceneQueryTest, RemoveFromGroup_NotInGroup_NoOp)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  A->RemoveFromGroup("enemies"); // should not crash
  EXPECT_FALSE(A->IsInGroup("enemies"));
}

TEST_F(SceneQueryTest, IsInGroup_NeverAdded_ReturnsFalse)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  EXPECT_FALSE(A->IsInGroup("enemies"));
}

TEST_F(SceneQueryTest, AddToGroup_NoOwningTree_NoOp)
{
  Node N("Standalone", NodeType::Node3D, TableAPI_);
  N.AddToGroup("enemies"); // no crash, no-op
  EXPECT_FALSE(N.IsInGroup("enemies"));
}

//=============================================================================
// SceneTree Group Query Tests
//=============================================================================

TEST_F(SceneQueryTest, GetNodesInGroup_MultipleNodes_ReturnsAll)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  Node* B = Tree_->CreateNode("B", NodeType::Node3D);
  Node* C = Tree_->CreateNode("C", NodeType::Node3D);

  A->AddToGroup("enemies");
  B->AddToGroup("enemies");
  C->AddToGroup("enemies");

  const auto& Enemies = Tree_->GetNodesInGroup("enemies");
  EXPECT_EQ(Enemies.size(), 3u);
}

TEST_F(SceneQueryTest, GetNodesInGroup_EmptyGroup_ReturnsEmpty)
{
  const auto& Group = Tree_->GetNodesInGroup("nonexistent");
  EXPECT_TRUE(Group.empty());
}

TEST_F(SceneQueryTest, GetGroupSize_ReturnsCorrectCount)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  Node* B = Tree_->CreateNode("B", NodeType::Node3D);
  A->AddToGroup("enemies");
  B->AddToGroup("enemies");
  EXPECT_EQ(Tree_->GetGroupSize("enemies"), 2u);
}

TEST_F(SceneQueryTest, DestroyNode_RemovesFromAllGroups)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  A->AddToGroup("enemies");
  A->AddToGroup("damageable");

  Tree_->DestroyNode(A);

  EXPECT_EQ(Tree_->GetGroupSize("enemies"), 0u);
  EXPECT_EQ(Tree_->GetGroupSize("damageable"), 0u);
}

TEST_F(SceneQueryTest, DestroyNode_SubtreeRemovesFromGroups)
{
  Node* Parent = Tree_->CreateNode("Parent", NodeType::Node3D);
  Node* Child = Tree_->CreateNode("Child", NodeType::Node3D, Parent);
  Parent->AddToGroup("group1");
  Child->AddToGroup("group2");

  Tree_->DestroyNode(Parent);

  EXPECT_EQ(Tree_->GetGroupSize("group1"), 0u);
  EXPECT_EQ(Tree_->GetGroupSize("group2"), 0u);
}

//=============================================================================
// Integration Tests: ScriptBase Wrappers
//=============================================================================

struct QueryTestScript : public ScriptBase
{
  Node* FoundNode = nullptr;
  CameraNode* FoundCamera = nullptr;
  bool InGroup = false;

  void OnInit() override
  {
    FoundNode = GetNode("../Sibling");
    FoundCamera = GetNode<CameraNode>("../Cam");
    AddToGroup("scripted");
    InGroup = GetOwner()->IsInGroup("scripted");
  }
};

TEST_F(SceneQueryTest, Script_GetNode_DelegatesToOwner)
{
  Node* Parent = Tree_->CreateNode("Parent", NodeType::Node3D);
  Node* A = Tree_->CreateNode("A", NodeType::Node3D, Parent);
  Node* Sibling = Tree_->CreateNode("Sibling", NodeType::Node3D, Parent);
  Tree_->CreateNode("Cam", NodeType::Camera, Parent);

  auto* Script = new QueryTestScript();
  A->AttachScript(Script);

  Tree_->Update(0.016f); // triggers init

  EXPECT_EQ(Script->FoundNode, Sibling);
  ASSERT_NE(Script->FoundCamera, nullptr);
  EXPECT_EQ(Script->FoundCamera->GetType(), NodeType::Camera);
  EXPECT_TRUE(Script->InGroup);
}

//=============================================================================
// Integration: Combined Workflow
//=============================================================================

TEST_F(SceneQueryTest, GroupAndPath_FindGroupMembersThenQueryChildren)
{
  // Build scene: two enemies, each with a "Weapon" child
  Node* E1 = Tree_->CreateNode("E1", NodeType::Node3D);
  Node* W1 = Tree_->CreateNode("Weapon", NodeType::MeshInstance, E1);
  Node* E2 = Tree_->CreateNode("E2", NodeType::Node3D);
  Node* W2 = Tree_->CreateNode("Weapon", NodeType::MeshInstance, E2);

  E1->AddToGroup("enemies");
  E2->AddToGroup("enemies");

  // Query group, then find child by path on each member
  const auto& Enemies = Tree_->GetNodesInGroup("enemies");
  ASSERT_EQ(Enemies.size(), 2u);

  std::vector<Node*> Weapons;
  for (Node* Enemy : Enemies) {
    Node* W = Enemy->GetNode("Weapon");
    if (W) {
      Weapons.push_back(W);
    }
  }

  ASSERT_EQ(Weapons.size(), 2u);
  EXPECT_EQ(Weapons[0], W1);
  EXPECT_EQ(Weapons[1], W2);
}
