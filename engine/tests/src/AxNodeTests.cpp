/**
 * AxNodeTests.cpp - Tests for Node hierarchy operations and script slot
 *
 * Tests the Node base class: creation, parent-child linking,
 * sibling order, reparenting, recursive child count, and removal.
 *
 * Also tests the Node script slot: AttachScript, DetachScript,
 * active-state integration, and ownership/destruction semantics.
 */

#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxHashTable.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxMath.h"
#include "AxEngine/AxNode.h"
#include "AxEngine/AxScriptBase.h"

#include <cstring>
#include <vector>
#include <string>

/**
 * Test fixture that initializes Foundation APIs required by Node.
 */
class NodeTest : public testing::Test
{
protected:
  void SetUp() override
  {
    AxonInitGlobalAPIRegistry();
    AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);
    TableAPI_ = (AxHashTableAPI*)AxonGlobalAPIRegistry->Get(AXON_HASH_TABLE_API_NAME);
    ASSERT_NE(TableAPI_, nullptr) << "Failed to get HashTableAPI from registry";
  }

  void TearDown() override
  {
    AxonTermGlobalAPIRegistry();
  }

  AxHashTableAPI* TableAPI_{nullptr};
};

//=============================================================================
// Test 1: Node creation with name and transform initialization
//=============================================================================
TEST_F(NodeTest, CreationWithNameAndTransform)
{
  Node node("TestNode", NodeType::Node3D, TableAPI_);

  // Verify name is set correctly
  EXPECT_EQ(node.GetName(), "TestNode");

  // Verify type
  EXPECT_EQ(node.GetType(), NodeType::Node3D);

  // Verify transform is initialized to identity
  const AxTransform& Transform = node.GetTransform();
  EXPECT_FLOAT_EQ(Transform.Translation.X, 0.0f);
  EXPECT_FLOAT_EQ(Transform.Translation.Y, 0.0f);
  EXPECT_FLOAT_EQ(Transform.Translation.Z, 0.0f);
  EXPECT_FLOAT_EQ(Transform.Scale.X, 1.0f);
  EXPECT_FLOAT_EQ(Transform.Scale.Y, 1.0f);
  EXPECT_FLOAT_EQ(Transform.Scale.Z, 1.0f);
  EXPECT_TRUE(Transform.IsIdentity);

  // Verify hierarchy is empty
  EXPECT_EQ(node.GetParent(), nullptr);
  EXPECT_EQ(node.GetFirstChild(), nullptr);
  EXPECT_EQ(node.GetNextSibling(), nullptr);

  // Verify flags
  EXPECT_FALSE(node.IsInitialized());
  EXPECT_TRUE(node.IsActive());
}

//=============================================================================
// Test 2: Parent-child linking via FirstChild/NextSibling pointers
//=============================================================================
TEST_F(NodeTest, ParentChildLinking)
{
  Node parent("Parent", NodeType::Node3D, TableAPI_);
  Node child("Child", NodeType::Node3D, TableAPI_);

  parent.AddChild(&child);

  // Verify parent-child relationship
  EXPECT_EQ(child.GetParent(), &parent);
  EXPECT_EQ(parent.GetFirstChild(), &child);

  // Child has no siblings
  EXPECT_EQ(child.GetNextSibling(), nullptr);

  // Parent has one child
  EXPECT_EQ(parent.GetChildCount(), 1);

  // Clean up: detach child so destructor doesn't try to RemoveChild on destroyed parent
  child.SetParent(nullptr);
}

//=============================================================================
// Test 3: Adding multiple children preserves sibling order
//=============================================================================
TEST_F(NodeTest, MultipleChildrenPreserveSiblingOrder)
{
  Node parent("Parent", NodeType::Node3D, TableAPI_);
  Node childA("ChildA", NodeType::Node3D, TableAPI_);
  Node childB("ChildB", NodeType::Node3D, TableAPI_);
  Node childC("ChildC", NodeType::Node3D, TableAPI_);

  parent.AddChild(&childA);
  parent.AddChild(&childB);
  parent.AddChild(&childC);

  // FirstChild should be the first one added
  EXPECT_EQ(parent.GetFirstChild(), &childA);

  // Verify sibling chain order: A -> B -> C
  EXPECT_EQ(childA.GetNextSibling(), &childB);
  EXPECT_EQ(childB.GetNextSibling(), &childC);
  EXPECT_EQ(childC.GetNextSibling(), nullptr);

  // All children point to same parent
  EXPECT_EQ(childA.GetParent(), &parent);
  EXPECT_EQ(childB.GetParent(), &parent);
  EXPECT_EQ(childC.GetParent(), &parent);

  // Count should be 3
  EXPECT_EQ(parent.GetChildCount(), 3);

  // Clean up: detach children
  childA.SetParent(nullptr);
  childB.SetParent(nullptr);
  childC.SetParent(nullptr);
}

//=============================================================================
// Test 4: Reparenting a node updates old and new parent pointers
//=============================================================================
TEST_F(NodeTest, ReparentingUpdatesParentPointers)
{
  Node parentA("ParentA", NodeType::Node3D, TableAPI_);
  Node parentB("ParentB", NodeType::Node3D, TableAPI_);
  Node child("Child", NodeType::Node3D, TableAPI_);

  // Add child to parent A
  parentA.AddChild(&child);
  EXPECT_EQ(child.GetParent(), &parentA);
  EXPECT_EQ(parentA.GetChildCount(), 1);
  EXPECT_EQ(parentB.GetChildCount(), 0);

  // Reparent child to parent B
  child.SetParent(&parentB);

  // Verify child is now under parent B
  EXPECT_EQ(child.GetParent(), &parentB);
  EXPECT_EQ(parentB.GetFirstChild(), &child);
  EXPECT_EQ(parentB.GetChildCount(), 1);

  // Verify parent A no longer has the child
  EXPECT_EQ(parentA.GetFirstChild(), nullptr);
  EXPECT_EQ(parentA.GetChildCount(), 0);

  // Clean up
  child.SetParent(nullptr);
}

//=============================================================================
// Test 5: Recursive child count traversal
//=============================================================================
TEST_F(NodeTest, RecursiveChildCountTraversal)
{
  // Build a hierarchy:
  //   Root
  //     ChildA
  //       GrandchildA1
  //       GrandchildA2
  //     ChildB
  //       GrandchildB1
  Node root("Root", NodeType::Root, TableAPI_);
  Node childA("ChildA", NodeType::Node3D, TableAPI_);
  Node childB("ChildB", NodeType::Node3D, TableAPI_);
  Node grandchildA1("GrandchildA1", NodeType::Node3D, TableAPI_);
  Node grandchildA2("GrandchildA2", NodeType::Node3D, TableAPI_);
  Node grandchildB1("GrandchildB1", NodeType::Node3D, TableAPI_);

  root.AddChild(&childA);
  root.AddChild(&childB);
  childA.AddChild(&grandchildA1);
  childA.AddChild(&grandchildA2);
  childB.AddChild(&grandchildB1);

  // Root has 2 direct children
  EXPECT_EQ(root.GetChildCount(), 2);

  // ChildA has 2 direct children
  EXPECT_EQ(childA.GetChildCount(), 2);

  // ChildB has 1 direct child
  EXPECT_EQ(childB.GetChildCount(), 1);

  // Grandchildren have 0 children
  EXPECT_EQ(grandchildA1.GetChildCount(), 0);
  EXPECT_EQ(grandchildA2.GetChildCount(), 0);
  EXPECT_EQ(grandchildB1.GetChildCount(), 0);

  // FindChild works for direct children
  EXPECT_EQ(root.FindChild("ChildA"), &childA);
  EXPECT_EQ(root.FindChild("ChildB"), &childB);
  EXPECT_EQ(root.FindChild("GrandchildA1"), nullptr); // Not a direct child

  // Verify Initialize recurses through hierarchy
  root.Initialize();
  EXPECT_TRUE(root.IsInitialized());
  EXPECT_TRUE(childA.IsInitialized());
  EXPECT_TRUE(childB.IsInitialized());
  EXPECT_TRUE(grandchildA1.IsInitialized());
  EXPECT_TRUE(grandchildA2.IsInitialized());
  EXPECT_TRUE(grandchildB1.IsInitialized());

  // Shutdown recurses too
  root.Shutdown();
  EXPECT_FALSE(root.IsInitialized());
  EXPECT_FALSE(childA.IsInitialized());

  // Clean up: detach from bottom up
  grandchildA1.SetParent(nullptr);
  grandchildA2.SetParent(nullptr);
  grandchildB1.SetParent(nullptr);
  childA.SetParent(nullptr);
  childB.SetParent(nullptr);
}

//=============================================================================
// Test 6: Node removal from hierarchy cleans up sibling links
//=============================================================================
TEST_F(NodeTest, RemovalCleansUpSiblingLinks)
{
  Node parent("Parent", NodeType::Node3D, TableAPI_);
  Node childA("ChildA", NodeType::Node3D, TableAPI_);
  Node childB("ChildB", NodeType::Node3D, TableAPI_);
  Node childC("ChildC", NodeType::Node3D, TableAPI_);

  parent.AddChild(&childA);
  parent.AddChild(&childB);
  parent.AddChild(&childC);

  // Remove the middle child (B)
  parent.RemoveChild(&childB);

  // Parent should now have 2 children: A -> C
  EXPECT_EQ(parent.GetChildCount(), 2);
  EXPECT_EQ(parent.GetFirstChild(), &childA);
  EXPECT_EQ(childA.GetNextSibling(), &childC);
  EXPECT_EQ(childC.GetNextSibling(), nullptr);

  // Removed child should be detached
  EXPECT_EQ(childB.GetParent(), nullptr);
  EXPECT_EQ(childB.GetNextSibling(), nullptr);

  // Remove the first child (A)
  parent.RemoveChild(&childA);
  EXPECT_EQ(parent.GetChildCount(), 1);
  EXPECT_EQ(parent.GetFirstChild(), &childC);
  EXPECT_EQ(childA.GetParent(), nullptr);

  // Remove the last child (C)
  parent.RemoveChild(&childC);
  EXPECT_EQ(parent.GetChildCount(), 0);
  EXPECT_EQ(parent.GetFirstChild(), nullptr);
  EXPECT_EQ(childC.GetParent(), nullptr);
}

//=============================================================================
// Script Slot Tests
//=============================================================================

/**
 * TrackingScript - Records callback invocation order and counts.
 * Used to verify lifecycle ordering guarantees.
 */
struct TrackingScript : public ScriptBase
{
  std::vector<std::string>& Log;
  std::string Id;

  bool* DestroyedFlag = nullptr;  // Points to a stack bool; safe to write on destruction

  int AttachCount      = 0;
  int InitCount        = 0;
  int EnableCount      = 0;
  int DisableCount     = 0;
  int DetachCount      = 0;
  int UpdateCount      = 0;
  int FixedUpdateCount = 0;
  int LateUpdateCount  = 0;

  TrackingScript(std::string_view ScriptId, std::vector<std::string>& EventLog)
    : Log(EventLog)
    , Id(ScriptId)
  {}

  ~TrackingScript() override
  {
    if (DestroyedFlag) {
      *DestroyedFlag = true;
    }
  }

  void OnAttach()  override { ++AttachCount;  Log.push_back(Id + ":OnAttach"); }
  void OnInit()    override { ++InitCount;    Log.push_back(Id + ":OnInit"); }
  void OnEnable()  override { ++EnableCount;  Log.push_back(Id + ":OnEnable"); }
  void OnDisable() override { ++DisableCount; Log.push_back(Id + ":OnDisable"); }
  void OnDetach()  override { ++DetachCount;  Log.push_back(Id + ":OnDetach"); }

  void OnUpdate(float DeltaT)      override { (void)DeltaT; ++UpdateCount; }
  void OnFixedUpdate(float DeltaT) override { (void)DeltaT; ++FixedUpdateCount; }
  void OnLateUpdate(float DeltaT)  override { (void)DeltaT; ++LateUpdateCount; }
};

//=============================================================================
// Script Test 1: AttachScript sets owner and fires OnAttach
//=============================================================================
TEST_F(NodeTest, AttachScript_SetsOwnerAndFiresOnAttach)
{
  // Log must outlive node so that node's destructor (which calls OnDetach)
  // can safely write into the log. Declare Log before node so that
  // node is destroyed first (C++ destroys locals in reverse declaration order).
  std::vector<std::string> Log;
  Node node("TestNode", NodeType::Node3D, TableAPI_);

  auto* Script = new TrackingScript("S", Log);
  node.AttachScript(Script);

  EXPECT_EQ(Script->GetOwner(), &node)
    << "Script owner should be set to the node after AttachScript";

  EXPECT_EQ(Script->AttachCount, 1)
    << "OnAttach should fire exactly once on attachment";

  EXPECT_EQ(Log.size(), 1u);
  EXPECT_EQ(Log[0], "S:OnAttach");

  // GetScript should return the attached script
  EXPECT_EQ(node.GetScript(), Script);
}




//=============================================================================
// Script Test 5: DetachScript when node has no script is a safe no-op
//=============================================================================
TEST_F(NodeTest, DetachScript_NoScript_IsNoOp)
{
  Node node("TestNode", NodeType::Node3D, TableAPI_);

  // Should not crash and should return nullptr
  EXPECT_NO_FATAL_FAILURE({
    ScriptBase* Result = node.DetachScript();
    EXPECT_EQ(Result, nullptr);
  });
}


//=============================================================================
// Script Test 7: Node destructor destroys the attached script.
//
// The destroyed flag is a stack bool pointer passed to the script before
// attaching. When the node destructor deletes the script, the script
// destructor sets *DestroyedFlag = true via the stored pointer.
// The stack bool outlives the node scope, so there is no use-after-free.
//=============================================================================
TEST_F(NodeTest, NodeDestructor_DestroysAttachedScript)
{
  // Log and Destroyed are declared outside the node scope so they outlive it.
  std::vector<std::string> Log;
  bool Destroyed = false;

  {
    Node node("TestNode", NodeType::Node3D, TableAPI_);
    auto* Script = new TrackingScript("S", Log);
    Script->DestroyedFlag = &Destroyed;
    node.AttachScript(Script);
    // Script is owned by node; node destroys it when it goes out of scope
  }

  // After the node destructor ran, the script's destructor was called
  EXPECT_TRUE(Destroyed)
    << "Script should have been destroyed when the node was destroyed";
}



//=============================================================================
// Script Test 10: SetActive does NOT fire OnDisable/OnEnable on an
//                 uninitialized script
//=============================================================================
TEST_F(NodeTest, SetActive_UninitializedScript_NoCallbacks)
{
  std::vector<std::string> Log;
  Node node("TestNode", NodeType::Node3D, TableAPI_);

  auto* Script = new TrackingScript("S", Log);
  node.AttachScript(Script);

  // Script is attached but NOT yet initialized (IsInitialized_ == false)
  ASSERT_FALSE(Script->IsInitialized());

  Log.clear();

  // Deactivate -- should not fire OnDisable because script is not initialized
  node.SetActive(false);
  EXPECT_EQ(Script->DisableCount, 0)
    << "OnDisable should not fire when script is not yet initialized";
  EXPECT_TRUE(Log.empty());

  // Reactivate -- should not fire OnEnable because script is not initialized
  node.SetActive(true);
  EXPECT_EQ(Script->EnableCount, 0)
    << "OnEnable should not fire when script is not yet initialized";
  EXPECT_TRUE(Log.empty());
}

//=============================================================================
// Script Test 11: Detached script has null owner
//=============================================================================
TEST_F(NodeTest, DetachScript_ClearsOwner)
{
  std::vector<std::string> Log;
  Node node("TestNode", NodeType::Node3D, TableAPI_);

  auto* Script = new TrackingScript("S", Log);
  node.AttachScript(Script);

  EXPECT_EQ(Script->GetOwner(), &node);

  ScriptBase* Returned = node.DetachScript();
  EXPECT_EQ(Returned->GetOwner(), nullptr)
    << "DetachScript should clear the script's owner pointer";

  delete Returned;
}

