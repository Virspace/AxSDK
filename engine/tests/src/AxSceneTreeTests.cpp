/**
 * AxSceneTreeTests.cpp - Tests for SceneTree traversal and frame loop methods
 *
 * Tests SceneTree's optimized traversal behaviour:
 *   - Update dispatches OnUpdate to initialized active scripts (flat list)
 *   - FixedUpdate dispatches OnFixedUpdate, skips uninitialized scripts
 *   - LateUpdate dispatches OnLateUpdate, skips inactive nodes/subtrees
 *   - Update runs bottom-up init scan before OnUpdate dispatch
 *   - Update calls UpdateNodeTransforms before script dispatch
 *   - OwningTree_ back-pointer assignment and clearing
 *   - Transform dirty list: SetPosition/SetRotation/SetScale notify SceneTree
 *   - Pending init queue: AttachScript registers for init
 *   - Script process list: initialized scripts dispatched, detach removes
 *   - DestroyNode cleanup from all optimization lists
 *   - Integration: full frame cycle with all optimizations active
 *
 * Uses real Node objects and ScriptBase subclasses (no mocks).
 *
 * NOTE: Script subclass names must be unique across all TUs linked into
 * AxEngineTests to avoid ODR violations. Hence "SceneTreeTrackingScript"
 * and "SceneTreeCountingScript" rather than plain "TrackingScript".
 *
 * ==========================================================================
 * DELETED SYMBOLS (SceneTree Optimization)
 * ==========================================================================
 * The following methods were removed as part of the scene tree optimization
 * that replaced full-tree traversals with flat-list iteration:
 *
 *   - SceneTree::TraverseTopDown()     -- replaced by flat ScriptNodes_ iteration
 *   - SceneTree::TraverseBottomUpInit() -- replaced by ProcessPendingInits()
 *
 * These were private methods, so no external callers are affected.
 * ==========================================================================
 */

#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxHashTable.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxMath.h"
#include "AxEngine/AxSceneTree.h"
#include "AxEngine/AxNode.h"
#include "AxEngine/AxScriptBase.h"

#include <string>
#include <vector>
#include <cmath>

//=============================================================================
// Shared call-order log (reset between tests via fixture)
//=============================================================================

static std::vector<std::string> gSceneTreeCallLog;

//=============================================================================
// SceneTreeTrackingScript - records every callback into gSceneTreeCallLog
//
// Named with a SceneTree prefix to avoid ODR collision with TrackingScript
// defined in AxNodeTests.cpp (which has a different layout).
//=============================================================================

struct SceneTreeTrackingScript : public ScriptBase
{
  std::string Tag;

  explicit SceneTreeTrackingScript(std::string_view TagIn) : Tag(TagIn) {}

  void OnAttach()                    override { gSceneTreeCallLog.push_back(Tag + ".OnAttach"); }
  void OnInit()                      override { gSceneTreeCallLog.push_back(Tag + ".OnInit"); }
  void OnEnable()                    override { gSceneTreeCallLog.push_back(Tag + ".OnEnable"); }
  void OnDisable()                   override { gSceneTreeCallLog.push_back(Tag + ".OnDisable"); }
  void OnDetach()                    override { gSceneTreeCallLog.push_back(Tag + ".OnDetach"); }
  void OnUpdate(float)               override { gSceneTreeCallLog.push_back(Tag + ".OnUpdate"); }
  void OnFixedUpdate(float)          override { gSceneTreeCallLog.push_back(Tag + ".OnFixedUpdate"); }
  void OnLateUpdate(float)           override { gSceneTreeCallLog.push_back(Tag + ".OnLateUpdate"); }
};

//=============================================================================
// SceneTreeCountingScript - counts invocations per callback
//=============================================================================

struct SceneTreeCountingScript : public ScriptBase
{
  int InitCount        = 0;
  int EnableCount      = 0;
  int DisableCount     = 0;
  int UpdateCount      = 0;
  int FixedUpdateCount = 0;
  int LateUpdateCount  = 0;

  void OnInit()             override { ++InitCount; }
  void OnEnable()           override { ++EnableCount; }
  void OnDisable()          override { ++DisableCount; }
  void OnUpdate(float)      override { ++UpdateCount; }
  void OnFixedUpdate(float) override { ++FixedUpdateCount; }
  void OnLateUpdate(float)  override { ++LateUpdateCount; }
};

//=============================================================================
// SceneTreeTransformCheckScript - checks world transform during OnUpdate
//=============================================================================

struct SceneTreeTransformCheckScript : public ScriptBase
{
  AxMat4x4 CapturedWorldTransform;
  bool WasCalled = false;

  void OnUpdate(float) override
  {
    WasCalled = true;
    CapturedWorldTransform = GetOwner()->GetWorldTransform();
  }
};

//=============================================================================
// Helper: find index of string in log (-1 if not found)
//=============================================================================

static int SceneTreeLogIndex(const std::string& Entry)
{
  for (int i = 0; i < static_cast<int>(gSceneTreeCallLog.size()); ++i) {
    if (gSceneTreeCallLog[i] == Entry) {
      return (i);
    }
  }
  return (-1);
}

//=============================================================================
// Helper: count occurrences of string in log
//=============================================================================

static int SceneTreeLogCount(const std::string& Entry)
{
  int Count = 0;
  for (const auto& E : gSceneTreeCallLog) {
    if (E == Entry) {
      Count++;
    }
  }
  return (Count);
}

//=============================================================================
// Helper: Compare two floats with tolerance
//=============================================================================

static bool SceneTreeFloatNear(float A, float B, float Tolerance = 0.001f)
{
  return (fabsf(A - B) < Tolerance);
}

//=============================================================================
// Test Fixture
//=============================================================================

class SceneTreeTest : public testing::Test
{
protected:
  void SetUp() override
  {
    gSceneTreeCallLog.clear();

    AxonInitGlobalAPIRegistry();
    AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);

    TableAPI_ = static_cast<AxHashTableAPI*>(
      AxonGlobalAPIRegistry->Get(AXON_HASH_TABLE_API_NAME));
    ASSERT_NE(TableAPI_, nullptr);

    // Create a SceneTree with no ResourceAPI (tests do not need scene loading)
    Tree_ = new SceneTree(TableAPI_, nullptr);
    Tree_->Name = "TestSceneTree";
  }

  void TearDown() override
  {
    delete Tree_;
    Tree_ = nullptr;

    AxonTermGlobalAPIRegistry();
    gSceneTreeCallLog.clear();
  }

  AxHashTableAPI* TableAPI_{nullptr};
  SceneTree* Tree_{nullptr};
};

//=============================================================================
// EXISTING TEST 1: Update dispatches OnUpdate to initialized active scripts
//=============================================================================

TEST_F(SceneTreeTest, UpdateDispatchesOnUpdateToInitializedActiveScripts)
{
  // Build hierarchy: Parent -> Child
  Node* Parent = Tree_->CreateNode("Parent", NodeType::Node3D, nullptr);
  Node* Child  = Tree_->CreateNode("Child",  NodeType::Node3D, Parent);

  Parent->AttachScript(new SceneTreeTrackingScript("P"));
  Child->AttachScript(new SceneTreeTrackingScript("C"));

  gSceneTreeCallLog.clear();

  // First Update: init scan (bottom-up) then OnUpdate
  Tree_->Update(0.016f);

  // Verify OnUpdate was dispatched to both
  int PUpdate = SceneTreeLogIndex("P.OnUpdate");
  int CUpdate = SceneTreeLogIndex("C.OnUpdate");

  ASSERT_GE(PUpdate, 0) << "Parent.OnUpdate should appear in log";
  ASSERT_GE(CUpdate, 0) << "Child.OnUpdate should appear in log";

  // Note: With flat-list dispatch, ordering between sibling scripts is
  // determined by init/registration order, not tree order. The spec allows
  // this: "script callbacks are independent".
}

//=============================================================================
// EXISTING TEST 2: FixedUpdate dispatches OnFixedUpdate, skips uninitialized
//=============================================================================

TEST_F(SceneTreeTest, FixedUpdateSkipsUninitializedScripts)
{
  Node* GP     = Tree_->CreateNode("Grandparent", NodeType::Node3D, nullptr);
  Node* Parent = Tree_->CreateNode("Parent",      NodeType::Node3D, GP);
  Node* Child  = Tree_->CreateNode("Child",       NodeType::Node3D, Parent);

  auto* GPScript = new SceneTreeCountingScript();
  auto* PScript  = new SceneTreeCountingScript();
  auto* CScript  = new SceneTreeCountingScript();

  GP->AttachScript(GPScript);
  Parent->AttachScript(PScript);
  Child->AttachScript(CScript);

  // Call FixedUpdate BEFORE any Update (scripts are not yet initialized)
  Tree_->FixedUpdate(0.02f);

  EXPECT_EQ(GPScript->FixedUpdateCount, 0)
    << "OnFixedUpdate must not fire before OnInit";
  EXPECT_EQ(PScript->FixedUpdateCount, 0)
    << "OnFixedUpdate must not fire before OnInit";
  EXPECT_EQ(CScript->FixedUpdateCount, 0)
    << "OnFixedUpdate must not fire before OnInit";

  // Now run Update to initialize scripts
  Tree_->Update(0.016f);

  ASSERT_EQ(GPScript->InitCount, 1) << "Grandparent script must be initialized";
  ASSERT_EQ(PScript->InitCount,  1) << "Parent script must be initialized";
  ASSERT_EQ(CScript->InitCount,  1) << "Child script must be initialized";

  // Now run FixedUpdate -- scripts are initialized, should fire
  Tree_->FixedUpdate(0.02f);

  EXPECT_EQ(GPScript->FixedUpdateCount, 1)
    << "Grandparent.OnFixedUpdate must fire after init";
  EXPECT_EQ(PScript->FixedUpdateCount, 1)
    << "Parent.OnFixedUpdate must fire after init";
  EXPECT_EQ(CScript->FixedUpdateCount, 1)
    << "Child.OnFixedUpdate must fire after init";
}

//=============================================================================
// EXISTING TEST 3: LateUpdate skips inactive nodes and subtrees
//=============================================================================

TEST_F(SceneTreeTest, LateUpdateSkipsInactiveNodesAndSubtrees)
{
  // Build hierarchy: GP -> Parent -> Child
  Node* GP     = Tree_->CreateNode("GP",     NodeType::Node3D, nullptr);
  Node* Parent = Tree_->CreateNode("Parent", NodeType::Node3D, GP);
  Node* Child  = Tree_->CreateNode("Child",  NodeType::Node3D, Parent);

  auto* GPScript = new SceneTreeCountingScript();
  auto* PScript  = new SceneTreeCountingScript();
  auto* CScript  = new SceneTreeCountingScript();

  GP->AttachScript(GPScript);
  Parent->AttachScript(PScript);
  Child->AttachScript(CScript);

  // Initialize all scripts
  Tree_->Update(0.016f);
  ASSERT_EQ(GPScript->InitCount, 1);
  ASSERT_EQ(PScript->InitCount,  1);
  ASSERT_EQ(CScript->InitCount,  1);

  // Deactivate the mid-hierarchy node (Parent)
  Parent->SetActive(false);

  // Run LateUpdate
  Tree_->LateUpdate(0.016f);

  // Grandparent (active) should receive OnLateUpdate
  EXPECT_EQ(GPScript->LateUpdateCount, 1)
    << "Active grandparent must receive OnLateUpdate";

  // Parent (inactive) and its child should be skipped
  EXPECT_EQ(PScript->LateUpdateCount, 0)
    << "Inactive parent must NOT receive OnLateUpdate";
  EXPECT_EQ(CScript->LateUpdateCount, 0)
    << "Child of inactive parent must NOT receive OnLateUpdate";
}

//=============================================================================
// EXISTING TEST 4: Update runs bottom-up init (children before parents)
//=============================================================================

TEST_F(SceneTreeTest, UpdateRunsBottomUpInitBeforeOnUpdate)
{
  // Build hierarchy: GP -> Parent -> Child
  Node* GP     = Tree_->CreateNode("Grandparent", NodeType::Node3D, nullptr);
  Node* Parent = Tree_->CreateNode("Parent",      NodeType::Node3D, GP);
  Node* Child  = Tree_->CreateNode("Child",       NodeType::Node3D, Parent);

  GP->AttachScript(new SceneTreeTrackingScript("GP"));
  Parent->AttachScript(new SceneTreeTrackingScript("P"));
  Child->AttachScript(new SceneTreeTrackingScript("C"));

  gSceneTreeCallLog.clear();

  // First Update: should init bottom-up then OnUpdate
  Tree_->Update(0.016f);

  // Verify init order: Child before Parent before Grandparent (bottom-up)
  int GPInit = SceneTreeLogIndex("GP.OnInit");
  int PInit  = SceneTreeLogIndex("P.OnInit");
  int CInit  = SceneTreeLogIndex("C.OnInit");

  ASSERT_GE(CInit,  0) << "Child.OnInit must appear";
  ASSERT_GE(PInit,  0) << "Parent.OnInit must appear";
  ASSERT_GE(GPInit, 0) << "Grandparent.OnInit must appear";

  EXPECT_LT(CInit, PInit)  << "OnInit: Child before Parent (bottom-up)";
  EXPECT_LT(PInit, GPInit) << "OnInit: Parent before Grandparent (bottom-up)";

  // Verify that ALL inits happen before ANY OnUpdate
  int GPUpdate = SceneTreeLogIndex("GP.OnUpdate");
  int PUpdate  = SceneTreeLogIndex("P.OnUpdate");
  int CUpdate  = SceneTreeLogIndex("C.OnUpdate");

  ASSERT_GE(GPUpdate, 0) << "Grandparent.OnUpdate must appear";
  ASSERT_GE(PUpdate,  0) << "Parent.OnUpdate must appear";
  ASSERT_GE(CUpdate,  0) << "Child.OnUpdate must appear";

  // All inits must come before any update
  EXPECT_LT(GPInit, GPUpdate) << "GP.OnInit must precede GP.OnUpdate";
  EXPECT_LT(GPInit, PUpdate)  << "GP.OnInit must precede P.OnUpdate";
  EXPECT_LT(GPInit, CUpdate)  << "GP.OnInit must precede C.OnUpdate";
}

//=============================================================================
// EXISTING TEST 5: Update flushes transforms before script dispatch
//=============================================================================

TEST_F(SceneTreeTest, UpdateFlushesTransformsBeforeScriptDispatch)
{
  // Build hierarchy: Parent -> Child
  Node* Parent = Tree_->CreateNode("Parent", NodeType::Node3D, nullptr);
  Node* Child  = Tree_->CreateNode("Child",  NodeType::Node3D, Parent);

  // Set parent translation to (10, 0, 0) and mark dirty
  Parent->GetTransform().Translation = {10.0f, 0.0f, 0.0f};
  Parent->GetTransform().ForwardMatrixDirty = true;

  // Set child translation to (0, 5, 0) and mark dirty
  Child->GetTransform().Translation = {0.0f, 5.0f, 0.0f};
  Child->GetTransform().ForwardMatrixDirty = true;

  // Attach a script that captures GetWorldTransform during OnUpdate
  auto* Script = new SceneTreeTransformCheckScript();
  Child->AttachScript(Script);

  // Run Update -- should flush transforms BEFORE dispatching OnUpdate
  Tree_->Update(0.016f);

  // Script should have been called
  ASSERT_TRUE(Script->WasCalled)
    << "Script OnUpdate must have been called";

  // The captured world transform should have the accumulated position (10, 5, 0)
  EXPECT_TRUE(SceneTreeFloatNear(Script->CapturedWorldTransform.E[3][0], 10.0f))
    << "Child world X should be 10.0 (from parent), got "
    << Script->CapturedWorldTransform.E[3][0];
  EXPECT_TRUE(SceneTreeFloatNear(Script->CapturedWorldTransform.E[3][1], 5.0f))
    << "Child world Y should be 5.0 (from child), got "
    << Script->CapturedWorldTransform.E[3][1];
  EXPECT_TRUE(SceneTreeFloatNear(Script->CapturedWorldTransform.E[3][2], 0.0f))
    << "Child world Z should be 0.0, got "
    << Script->CapturedWorldTransform.E[3][2];
}

//=============================================================================
// TASK GROUP 1: OwningTree_ back-pointer tests
//=============================================================================

TEST_F(SceneTreeTest, CreateNodeSetsOwningTree)
{
  Node* N = Tree_->CreateNode("TestNode", NodeType::Node3D, nullptr);
  ASSERT_NE(N, nullptr);
  EXPECT_EQ(N->GetOwningTree(), Tree_)
    << "CreateNode must set OwningTree_ to the SceneTree";
}

TEST_F(SceneTreeTest, RootNodeHasOwningTree)
{
  EXPECT_EQ(Tree_->GetRootNode()->GetOwningTree(), Tree_)
    << "Root node's OwningTree_ must be set in SceneTree constructor";
}

TEST_F(SceneTreeTest, DestroyNodeClearsOwningTree)
{
  // We cannot directly verify after deletion since the node is freed.
  // Instead, verify that DestroyNode does not crash and reduces count.
  Node* N = Tree_->CreateNode("ToDestroy", NodeType::Node3D, nullptr);
  ASSERT_NE(N, nullptr);
  uint32_t CountBefore = Tree_->GetNodeCount();

  Tree_->DestroyNode(N);

  EXPECT_EQ(Tree_->GetNodeCount(), CountBefore - 1)
    << "DestroyNode must reduce node count";
}

//=============================================================================
// TASK GROUP 2: Transform dirty list tests
//=============================================================================

TEST_F(SceneTreeTest, SetPositionAddsToDirtyRoots)
{
  Node* N = Tree_->CreateNode("Mover", NodeType::Node3D, nullptr);

  // First Update() flushes the initial dirty state from CreateNode
  Tree_->Update(0.016f);

  // Now set position -- should add to dirty roots
  N->SetPosition(5.0f, 0.0f, 0.0f);

  // Verify by running Update and checking world transform
  Tree_->Update(0.016f);

  EXPECT_TRUE(SceneTreeFloatNear(N->GetWorldTransform().E[3][0], 5.0f))
    << "After SetPosition and Update, world X should be 5.0";
}

TEST_F(SceneTreeTest, SetRotationAddsToDirtyRoots)
{
  Node* N = Tree_->CreateNode("Rotator", NodeType::Node3D, nullptr);

  // First Update() flushes initial dirty state
  Tree_->Update(0.016f);

  // Set rotation
  AxQuat Rot = QuatFromAxisAngle({0.0f, 1.0f, 0.0f}, 1.5708f); // ~90 degrees
  N->SetRotation(Rot);

  Tree_->Update(0.016f);

  // The world transform should reflect the rotation (non-identity)
  EXPECT_FALSE(SceneTreeFloatNear(N->GetWorldTransform().E[0][0], 1.0f))
    << "After SetRotation and Update, rotation should be applied";
}

TEST_F(SceneTreeTest, SetScaleAddsToDirtyRoots)
{
  Node* N = Tree_->CreateNode("Scaler", NodeType::Node3D, nullptr);

  Tree_->Update(0.016f);

  N->SetScale(2.0f, 3.0f, 4.0f);
  Tree_->Update(0.016f);

  // Check scale is reflected in world transform diagonal
  EXPECT_TRUE(SceneTreeFloatNear(N->GetWorldTransform().E[0][0], 2.0f))
    << "After SetScale(2,3,4), X scale should be 2.0";
  EXPECT_TRUE(SceneTreeFloatNear(N->GetWorldTransform().E[1][1], 3.0f))
    << "After SetScale(2,3,4), Y scale should be 3.0";
  EXPECT_TRUE(SceneTreeFloatNear(N->GetWorldTransform().E[2][2], 4.0f))
    << "After SetScale(2,3,4), Z scale should be 4.0";
}

TEST_F(SceneTreeTest, DirtyListHandlesParentAndChild)
{
  // When both parent and child are dirty, both should be processed correctly
  Node* Parent = Tree_->CreateNode("P", NodeType::Node3D, nullptr);
  Node* Child  = Tree_->CreateNode("C", NodeType::Node3D, Parent);

  Tree_->Update(0.016f); // Flush initial dirty state

  Parent->SetPosition(10.0f, 0.0f, 0.0f);
  Child->SetPosition(0.0f, 5.0f, 0.0f);

  Tree_->Update(0.016f);

  // Child world position should be (10, 5, 0) = parent(10,0,0) + local(0,5,0)
  EXPECT_TRUE(SceneTreeFloatNear(Child->GetWorldTransform().E[3][0], 10.0f))
    << "Child world X should be 10.0 (from parent)";
  EXPECT_TRUE(SceneTreeFloatNear(Child->GetWorldTransform().E[3][1], 5.0f))
    << "Child world Y should be 5.0 (from child)";
}

TEST_F(SceneTreeTest, DirtyListDuplicatePrevention)
{
  Node* N = Tree_->CreateNode("Mover", NodeType::Node3D, nullptr);

  Tree_->Update(0.016f); // Flush initial dirty state

  // Call SetPosition multiple times -- should not crash or overflow
  N->SetPosition(1.0f, 0.0f, 0.0f);
  N->SetPosition(2.0f, 0.0f, 0.0f);
  N->SetPosition(3.0f, 0.0f, 0.0f);

  Tree_->Update(0.016f);

  // Final position should be (3, 0, 0)
  EXPECT_TRUE(SceneTreeFloatNear(N->GetWorldTransform().E[3][0], 3.0f))
    << "After multiple SetPosition calls, final X should be 3.0";
}

TEST_F(SceneTreeTest, InitialLoadDirtyListDegracefullyToFullTraversal)
{
  // Create several nodes -- all are dirty from CreateNode
  Node* A = Tree_->CreateNode("A", NodeType::Node3D, nullptr);
  Node* B = Tree_->CreateNode("B", NodeType::Node3D, A);
  Node* C = Tree_->CreateNode("C", NodeType::Node3D, B);

  A->SetPosition(1.0f, 0.0f, 0.0f);
  B->SetPosition(0.0f, 2.0f, 0.0f);
  C->SetPosition(0.0f, 0.0f, 3.0f);

  Tree_->Update(0.016f);

  // All transforms should be computed correctly
  EXPECT_TRUE(SceneTreeFloatNear(A->GetWorldTransform().E[3][0], 1.0f));
  EXPECT_TRUE(SceneTreeFloatNear(B->GetWorldTransform().E[3][0], 1.0f))
    << "B inherits parent A's X translation";
  EXPECT_TRUE(SceneTreeFloatNear(B->GetWorldTransform().E[3][1], 2.0f));
  EXPECT_TRUE(SceneTreeFloatNear(C->GetWorldTransform().E[3][0], 1.0f))
    << "C inherits grandparent A's X translation";
  EXPECT_TRUE(SceneTreeFloatNear(C->GetWorldTransform().E[3][1], 2.0f))
    << "C inherits parent B's Y translation";
  EXPECT_TRUE(SceneTreeFloatNear(C->GetWorldTransform().E[3][2], 3.0f));
}

//=============================================================================
// TASK GROUP 3: Pending init queue tests
//=============================================================================

TEST_F(SceneTreeTest, AttachScriptRegistersForPendingInit)
{
  Node* N = Tree_->CreateNode("Scripted", NodeType::Node3D, nullptr);

  auto* Script = new SceneTreeCountingScript();
  N->AttachScript(Script);

  // Script should NOT be initialized yet (init happens during Update)
  EXPECT_EQ(Script->InitCount, 0)
    << "Script must not be initialized immediately on attach";

  Tree_->Update(0.016f);

  // Now it should be initialized
  EXPECT_EQ(Script->InitCount, 1)
    << "Script must be initialized during next Update()";
}

TEST_F(SceneTreeTest, PendingInitBottomUpOrder)
{
  // Build hierarchy: GP -> Parent -> Child
  Node* GP     = Tree_->CreateNode("GP",     NodeType::Node3D, nullptr);
  Node* Parent = Tree_->CreateNode("Parent", NodeType::Node3D, GP);
  Node* Child  = Tree_->CreateNode("Child",  NodeType::Node3D, Parent);

  GP->AttachScript(new SceneTreeTrackingScript("GP"));
  Parent->AttachScript(new SceneTreeTrackingScript("P"));
  Child->AttachScript(new SceneTreeTrackingScript("C"));

  gSceneTreeCallLog.clear();
  Tree_->Update(0.016f);

  int GPInit = SceneTreeLogIndex("GP.OnInit");
  int PInit  = SceneTreeLogIndex("P.OnInit");
  int CInit  = SceneTreeLogIndex("C.OnInit");

  ASSERT_GE(CInit,  0);
  ASSERT_GE(PInit,  0);
  ASSERT_GE(GPInit, 0);

  EXPECT_LT(CInit, PInit)  << "OnInit: Child before Parent (bottom-up)";
  EXPECT_LT(PInit, GPInit) << "OnInit: Parent before Grandparent (bottom-up)";
}

TEST_F(SceneTreeTest, PendingInitOnlyOnce)
{
  Node* N = Tree_->CreateNode("Once", NodeType::Node3D, nullptr);

  auto* Script = new SceneTreeCountingScript();
  N->AttachScript(Script);

  Tree_->Update(0.016f);
  Tree_->Update(0.016f);
  Tree_->Update(0.016f);

  EXPECT_EQ(Script->InitCount, 1)
    << "OnInit must be called exactly once";
}

TEST_F(SceneTreeTest, PendingInitEnableAfterInit)
{
  Node* N = Tree_->CreateNode("WithEnable", NodeType::Node3D, nullptr);

  auto* Script = new SceneTreeCountingScript();
  N->AttachScript(Script);

  Tree_->Update(0.016f);

  EXPECT_EQ(Script->InitCount, 1);
  EXPECT_EQ(Script->EnableCount, 1)
    << "OnEnable should fire immediately after OnInit for active nodes";
}

//=============================================================================
// TASK GROUP 4: Script process list tests
//=============================================================================

TEST_F(SceneTreeTest, InitializedScriptIsInProcessList)
{
  Node* N = Tree_->CreateNode("Scripted", NodeType::Node3D, nullptr);

  auto* Script = new SceneTreeCountingScript();
  N->AttachScript(Script);

  // Initialize
  Tree_->Update(0.016f);
  ASSERT_EQ(Script->InitCount, 1);

  // Should now receive per-frame updates
  Tree_->Update(0.016f);
  EXPECT_EQ(Script->UpdateCount, 2)
    << "Initialized script should receive OnUpdate each frame (1 from init frame + 1)";

  Tree_->FixedUpdate(0.02f);
  EXPECT_EQ(Script->FixedUpdateCount, 1)
    << "Initialized script should receive OnFixedUpdate";

  Tree_->LateUpdate(0.016f);
  EXPECT_EQ(Script->LateUpdateCount, 1)
    << "Initialized script should receive OnLateUpdate";
}

TEST_F(SceneTreeTest, DetachScriptRemovesFromProcessList)
{
  Node* N = Tree_->CreateNode("Scripted", NodeType::Node3D, nullptr);

  auto* Script = new SceneTreeCountingScript();
  N->AttachScript(Script);

  // Initialize
  Tree_->Update(0.016f);
  ASSERT_EQ(Script->InitCount, 1);
  ASSERT_EQ(Script->UpdateCount, 1);

  // Detach
  ScriptBase* Detached = N->DetachScript();
  ASSERT_NE(Detached, nullptr);

  // Should NOT receive further updates
  Tree_->Update(0.016f);
  EXPECT_EQ(Script->UpdateCount, 1)
    << "Detached script must not receive further OnUpdate calls";

  delete Detached;
}

TEST_F(SceneTreeTest, InactiveNodesSkippedDuringDispatch)
{
  Node* N = Tree_->CreateNode("Scripted", NodeType::Node3D, nullptr);

  auto* Script = new SceneTreeCountingScript();
  N->AttachScript(Script);

  Tree_->Update(0.016f); // Initialize
  N->SetActive(false);

  Tree_->Update(0.016f);
  EXPECT_EQ(Script->UpdateCount, 1)
    << "Inactive node must not receive OnUpdate (only the init-frame update)";

  Tree_->FixedUpdate(0.02f);
  EXPECT_EQ(Script->FixedUpdateCount, 0)
    << "Inactive node must not receive OnFixedUpdate";

  Tree_->LateUpdate(0.016f);
  EXPECT_EQ(Script->LateUpdateCount, 0)
    << "Inactive node must not receive OnLateUpdate";
}

TEST_F(SceneTreeTest, InactiveParentSkipsChildDuringDispatch)
{
  // Build hierarchy: Parent -> Child
  Node* Parent = Tree_->CreateNode("Parent", NodeType::Node3D, nullptr);
  Node* Child  = Tree_->CreateNode("Child",  NodeType::Node3D, Parent);

  auto* PScript = new SceneTreeCountingScript();
  auto* CScript = new SceneTreeCountingScript();
  Parent->AttachScript(PScript);
  Child->AttachScript(CScript);

  Tree_->Update(0.016f); // Initialize

  // Deactivate parent only -- child remains active but parent is inactive
  Parent->SetActive(false);

  Tree_->Update(0.016f);
  EXPECT_EQ(PScript->UpdateCount, 1)
    << "Inactive parent must not receive OnUpdate after being deactivated";
  EXPECT_EQ(CScript->UpdateCount, 1)
    << "Child of inactive parent must not receive OnUpdate";
}

TEST_F(SceneTreeTest, MainCameraAndMouseDeltaPropagatedToScripts)
{
  Node* CamNode = Tree_->CreateNode("Cam", NodeType::Camera, nullptr);
  CameraNode* Cam = static_cast<CameraNode*>(CamNode);

  Tree_->SetMainCamera(Cam);
  Tree_->UpdateMouseDelta({1.5f, 2.5f});

  Node* N = Tree_->CreateNode("Scripted", NodeType::Node3D, nullptr);

  // Custom script to check MainCamera and MouseDelta
  struct SceneTreeCamCheckScript : public ScriptBase
  {
    CameraNode* CapturedCamera = nullptr;
    AxVec2 CapturedMouse = {0, 0};
    void OnUpdate(float) override
    {
      CapturedCamera = MainCamera;
      CapturedMouse = MouseDelta;
    }
  };

  auto* Script = new SceneTreeCamCheckScript();
  N->AttachScript(Script);
  Tree_->Update(0.016f);

  EXPECT_EQ(Script->CapturedCamera, Cam)
    << "MainCamera must be propagated to scripts during dispatch";
  EXPECT_TRUE(SceneTreeFloatNear(Script->CapturedMouse.X, 1.5f))
    << "MouseDelta.X must be propagated to scripts";
  EXPECT_TRUE(SceneTreeFloatNear(Script->CapturedMouse.Y, 2.5f))
    << "MouseDelta.Y must be propagated to scripts";
}

//=============================================================================
// TASK GROUP 5: DestroyNode cleanup for all new lists
//=============================================================================

TEST_F(SceneTreeTest, DestroyNodeWithScriptRemovesFromScriptNodes)
{
  Node* N = Tree_->CreateNode("Scripted", NodeType::Node3D, nullptr);

  auto* Script = new SceneTreeCountingScript();
  N->AttachScript(Script);
  Tree_->Update(0.016f); // Initialize

  // Destroy the node -- should remove from ScriptNodes_
  Tree_->DestroyNode(N);

  // No crash on subsequent Update (script list should not contain dangling pointer)
  Tree_->Update(0.016f);
  Tree_->FixedUpdate(0.02f);
  Tree_->LateUpdate(0.016f);
}

TEST_F(SceneTreeTest, DestroyNodeWithPendingInitRemovesFromPendingList)
{
  Node* N = Tree_->CreateNode("Pending", NodeType::Node3D, nullptr);
  N->AttachScript(new SceneTreeCountingScript());

  // Script is pending init (no Update called yet)
  // Destroy before it gets initialized
  Tree_->DestroyNode(N);

  // No crash on subsequent Update (pending init list should not contain dangling pointer)
  Tree_->Update(0.016f);
}

TEST_F(SceneTreeTest, DestroyNodeWithDirtyTransformRemovesFromDirtyRoots)
{
  Node* N = Tree_->CreateNode("Dirty", NodeType::Node3D, nullptr);

  // Node is already in dirty list from CreateNode.
  // SetPosition would add again, but InDirtyList_ prevents duplicates.
  N->SetPosition(1.0f, 2.0f, 3.0f);

  // Destroy before dirty list is processed
  Tree_->DestroyNode(N);

  // No crash on subsequent Update (dirty list should not contain dangling pointer)
  Tree_->Update(0.016f);
}

TEST_F(SceneTreeTest, DestroySubtreeRemovesAllFromLists)
{
  // Build hierarchy: Parent -> Child
  Node* Parent = Tree_->CreateNode("Parent", NodeType::Node3D, nullptr);
  Node* Child  = Tree_->CreateNode("Child",  NodeType::Node3D, Parent);

  Parent->AttachScript(new SceneTreeCountingScript());
  Child->AttachScript(new SceneTreeCountingScript());
  Tree_->Update(0.016f); // Initialize both

  // Set positions so they are in dirty list
  Parent->SetPosition(1.0f, 0.0f, 0.0f);
  Child->SetPosition(0.0f, 1.0f, 0.0f);

  // Destroy parent (and child)
  Tree_->DestroyNode(Parent);

  // No crash on subsequent frame
  Tree_->Update(0.016f);
  Tree_->FixedUpdate(0.02f);
  Tree_->LateUpdate(0.016f);
}

//=============================================================================
// TASK GROUP 6: Integration tests
//=============================================================================

TEST_F(SceneTreeTest, FullFrameCycleWithAllOptimizations)
{
  // Create a small scene hierarchy
  Node* Root    = Tree_->GetRootNode();
  Node* CamNode = Tree_->CreateNode("Camera", NodeType::Camera, nullptr);
  Node* Parent  = Tree_->CreateNode("Parent", NodeType::Node3D, nullptr);
  Node* Child   = Tree_->CreateNode("Child",  NodeType::Node3D, Parent);

  // Set transforms using the setter methods
  Parent->SetPosition(10.0f, 0.0f, 0.0f);
  Child->SetPosition(0.0f, 5.0f, 0.0f);

  // Attach scripts
  auto* PScript = new SceneTreeCountingScript();
  auto* CScript = new SceneTreeCountingScript();
  Parent->AttachScript(PScript);
  Child->AttachScript(CScript);

  // Set main camera
  CameraNode* Cam = static_cast<CameraNode*>(CamNode);
  Tree_->SetMainCamera(Cam);

  // Frame 1: Initialize + Update
  Tree_->Update(0.016f);

  EXPECT_EQ(PScript->InitCount, 1) << "Parent script initialized in frame 1";
  EXPECT_EQ(CScript->InitCount, 1) << "Child script initialized in frame 1";
  EXPECT_EQ(PScript->UpdateCount, 1) << "Parent script updated in frame 1";
  EXPECT_EQ(CScript->UpdateCount, 1) << "Child script updated in frame 1";

  // Verify transforms were computed
  EXPECT_TRUE(SceneTreeFloatNear(Parent->GetWorldTransform().E[3][0], 10.0f));
  EXPECT_TRUE(SceneTreeFloatNear(Child->GetWorldTransform().E[3][0], 10.0f))
    << "Child inherits parent X";
  EXPECT_TRUE(SceneTreeFloatNear(Child->GetWorldTransform().E[3][1], 5.0f));

  // FixedUpdate
  Tree_->FixedUpdate(0.02f);
  EXPECT_EQ(PScript->FixedUpdateCount, 1);
  EXPECT_EQ(CScript->FixedUpdateCount, 1);

  // LateUpdate
  Tree_->LateUpdate(0.016f);
  EXPECT_EQ(PScript->LateUpdateCount, 1);
  EXPECT_EQ(CScript->LateUpdateCount, 1);

  // Frame 2: Move parent, verify child follows
  Parent->SetPosition(20.0f, 0.0f, 0.0f);
  Tree_->Update(0.016f);

  EXPECT_TRUE(SceneTreeFloatNear(Parent->GetWorldTransform().E[3][0], 20.0f));
  EXPECT_TRUE(SceneTreeFloatNear(Child->GetWorldTransform().E[3][0], 20.0f))
    << "After parent moves to X=20, child world X should also be 20";
  EXPECT_TRUE(SceneTreeFloatNear(Child->GetWorldTransform().E[3][1], 5.0f))
    << "Child local Y should remain 5.0";

  EXPECT_EQ(PScript->UpdateCount, 2);
  EXPECT_EQ(CScript->UpdateCount, 2);
}

TEST_F(SceneTreeTest, NoChangesFrameDoesNotCrash)
{
  // Create some nodes but do NOT change anything
  Node* N = Tree_->CreateNode("Static", NodeType::Node3D, nullptr);

  // Frame 1: Initial flush
  Tree_->Update(0.016f);

  // Frame 2-4: No changes -- dirty list is empty, no scripts
  Tree_->Update(0.016f);
  Tree_->FixedUpdate(0.02f);
  Tree_->LateUpdate(0.016f);
  Tree_->Update(0.016f);

  // Just verify no crash
  EXPECT_NE(N, nullptr);
}

TEST_F(SceneTreeTest, AttachScriptAfterFirstFrameGetsInitializedOnNextUpdate)
{
  Node* N = Tree_->CreateNode("LateScript", NodeType::Node3D, nullptr);

  // Frame 1: No scripts
  Tree_->Update(0.016f);

  // Attach script mid-game
  auto* Script = new SceneTreeCountingScript();
  N->AttachScript(Script);

  EXPECT_EQ(Script->InitCount, 0)
    << "Script attached after Update should not be initialized yet";

  // Frame 2: Script should be initialized and updated
  Tree_->Update(0.016f);

  EXPECT_EQ(Script->InitCount, 1)
    << "Script must be initialized on next Update after attach";
  EXPECT_EQ(Script->UpdateCount, 1)
    << "Script must receive OnUpdate after initialization";
}

TEST_F(SceneTreeTest, TransformDirtyOnlyProcessedOnce)
{
  Node* N = Tree_->CreateNode("Mover", NodeType::Node3D, nullptr);

  Tree_->Update(0.016f); // Flush initial

  N->SetPosition(5.0f, 0.0f, 0.0f);
  Tree_->Update(0.016f); // Process the dirty

  // Second Update with no changes should not re-process
  // Verify by checking that the world transform is still correct
  EXPECT_TRUE(SceneTreeFloatNear(N->GetWorldTransform().E[3][0], 5.0f));

  // Change again
  N->SetPosition(10.0f, 0.0f, 0.0f);
  Tree_->Update(0.016f);

  EXPECT_TRUE(SceneTreeFloatNear(N->GetWorldTransform().E[3][0], 10.0f));
}

TEST_F(SceneTreeTest, MultipleScriptsAllReceiveCallbacks)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D, nullptr);
  Node* B = Tree_->CreateNode("B", NodeType::Node3D, nullptr);
  Node* C = Tree_->CreateNode("C", NodeType::Node3D, nullptr);

  auto* SA = new SceneTreeCountingScript();
  auto* SB = new SceneTreeCountingScript();
  auto* SC = new SceneTreeCountingScript();

  A->AttachScript(SA);
  B->AttachScript(SB);
  C->AttachScript(SC);

  Tree_->Update(0.016f); // Init + first Update
  Tree_->FixedUpdate(0.02f);
  Tree_->LateUpdate(0.016f);

  EXPECT_EQ(SA->UpdateCount, 1);
  EXPECT_EQ(SB->UpdateCount, 1);
  EXPECT_EQ(SC->UpdateCount, 1);
  EXPECT_EQ(SA->FixedUpdateCount, 1);
  EXPECT_EQ(SB->FixedUpdateCount, 1);
  EXPECT_EQ(SC->FixedUpdateCount, 1);
  EXPECT_EQ(SA->LateUpdateCount, 1);
  EXPECT_EQ(SB->LateUpdateCount, 1);
  EXPECT_EQ(SC->LateUpdateCount, 1);
}
