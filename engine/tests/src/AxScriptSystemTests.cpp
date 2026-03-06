/**
 * AxScriptSystemTests.cpp - Tests for SceneTree traversal behaviour
 *
 * Tests SceneTree traversal behaviour (migrated from old ScriptSystem tests):
 *   - Bottom-up OnInit ordering (children before parents)
 *   - OnUpdate dispatched to all initialized active scripts
 *   - OnLateUpdate fires after all OnUpdate calls
 *   - OnFixedUpdate dispatched to all initialized active scripts
 *   - Per-frame callbacks suppressed for uninitialized scripts
 *   - Per-frame callbacks suppressed for inactive nodes
 *   - Nodes without scripts are traversed without error
 *   - Runtime attachment deferred init semantics
 *
 * Uses real Node and SceneTree objects (no mocks per the test spec).
 * ScriptBase subclasses record call order into a shared vector.
 *
 * NOTE: With the scene tree optimization (dirty lists, flat script process
 * list), per-frame dispatch order is flat-list order (insertion order from
 * ProcessPendingInits), not tree depth-first order. The spec permits this:
 * "script callbacks are independent". Tests verify callbacks FIRE but do
 * not assert parent-before-child ordering on per-frame callbacks.
 * Bottom-up init ordering IS still guaranteed and asserted.
 */

#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxHashTable.h"
#include "Foundation/AxAPIRegistry.h"
#include "AxEngine/AxSceneTree.h"
#include "AxEngine/AxNode.h"
#include "AxEngine/AxScriptBase.h"

#include <string>
#include <vector>

//=============================================================================
// Shared call-order log (reset between tests via fixture)
//=============================================================================

static std::vector<std::string> gCallLog;

//=============================================================================
// OrderedScript - records every callback into gCallLog with a tag
//=============================================================================

struct OrderedScript : public ScriptBase
{
  std::string Tag;

  explicit OrderedScript(std::string_view TagIn) : Tag(TagIn) {}

  void OnAttach()                    override { gCallLog.push_back(Tag + ".OnAttach"); }
  void OnInit()                      override { gCallLog.push_back(Tag + ".OnInit"); }
  void OnEnable()                    override { gCallLog.push_back(Tag + ".OnEnable"); }
  void OnDisable()                   override { gCallLog.push_back(Tag + ".OnDisable"); }
  void OnDetach()                    override { gCallLog.push_back(Tag + ".OnDetach"); }
  void OnUpdate(float)               override { gCallLog.push_back(Tag + ".OnUpdate"); }
  void OnFixedUpdate(float)          override { gCallLog.push_back(Tag + ".OnFixedUpdate"); }
  void OnLateUpdate(float)           override { gCallLog.push_back(Tag + ".OnLateUpdate"); }
};

//=============================================================================
// CountingScript - counts invocations per callback (no tag needed)
//=============================================================================

struct CountingScript : public ScriptBase
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
// Helper: find index of string in log (-1 if not found)
//=============================================================================

static int LogIndex(const std::string& Entry)
{
  for (int i = 0; i < static_cast<int>(gCallLog.size()); ++i) {
    if (gCallLog[i] == Entry) {
      return (i);
    }
  }
  return (-1);
}

//=============================================================================
// Test Fixture
//=============================================================================

class ScriptSystemTest : public testing::Test
{
protected:
  void SetUp() override
  {
    gCallLog.clear();

    AxonInitGlobalAPIRegistry();
    AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);

    TableAPI_ = static_cast<AxHashTableAPI*>(
      AxonGlobalAPIRegistry->Get(AXON_HASH_TABLE_API_NAME));
    ASSERT_NE(TableAPI_, nullptr);

    // Create a SceneTree (no ResourceAPI needed for traversal tests)
    Tree_ = new SceneTree(TableAPI_, nullptr);
    Tree_->Name = "TestScene";
  }

  void TearDown() override
  {
    delete Tree_;
    Tree_ = nullptr;

    AxonTermGlobalAPIRegistry();
    gCallLog.clear();
  }

  /**
   * Run one full variable-rate frame: dirty transform flush + pending init + OnUpdate.
   */
  void RunUpdate(float DeltaT = 0.016f)
  {
    Tree_->Update(DeltaT);
  }

  /**
   * Run one fixed-update pass: OnFixedUpdate to all scripted nodes.
   */
  void RunFixedUpdate(float FixedDeltaT = 0.02f)
  {
    Tree_->FixedUpdate(FixedDeltaT);
  }

  /**
   * Run one late-update pass: OnLateUpdate to all scripted nodes.
   */
  void RunLateUpdate(float DeltaT = 0.016f)
  {
    Tree_->LateUpdate(DeltaT);
  }

  AxHashTableAPI* TableAPI_{nullptr};
  SceneTree* Tree_{nullptr};
};

//=============================================================================
// Task 4.1 Tests -- Lifecycle ordering and suppression
//=============================================================================

// --- OnInit bottom-up: child before parent (two nodes) ---

TEST_F(ScriptSystemTest, OnInitBottomUp_ChildBeforeParent)
{
  Node* Parent = Tree_->CreateNode("Parent", NodeType::Node3D, nullptr);
  Node* Child  = Tree_->CreateNode("Child",  NodeType::Node3D, Parent);

  Parent->AttachScript(new OrderedScript("P"));
  Child->AttachScript(new OrderedScript("C"));

  gCallLog.clear(); // clear OnAttach entries

  RunUpdate();

  int PInit = LogIndex("P.OnInit");
  int CInit = LogIndex("C.OnInit");

  ASSERT_GE(CInit, 0) << "Child.OnInit should appear in log";
  ASSERT_GE(PInit, 0) << "Parent.OnInit should appear in log";
  EXPECT_LT(CInit, PInit) << "Child OnInit must fire before Parent OnInit";
}

// --- OnInit bottom-up: three-level hierarchy ---

TEST_F(ScriptSystemTest, OnInitBottomUp_ThreeLevelHierarchy)
{
  Node* GP     = Tree_->CreateNode("Grandparent", NodeType::Node3D, nullptr);
  Node* Parent = Tree_->CreateNode("Parent",      NodeType::Node3D, GP);
  Node* Child  = Tree_->CreateNode("Child",       NodeType::Node3D, Parent);

  GP->AttachScript(new OrderedScript("GP"));
  Parent->AttachScript(new OrderedScript("P"));
  Child->AttachScript(new OrderedScript("C"));

  gCallLog.clear();
  RunUpdate();

  int GPInit = LogIndex("GP.OnInit");
  int PInit  = LogIndex("P.OnInit");
  int CInit  = LogIndex("C.OnInit");

  ASSERT_GE(CInit,  0);
  ASSERT_GE(PInit,  0);
  ASSERT_GE(GPInit, 0);

  EXPECT_LT(CInit, PInit)  << "Child OnInit before Parent OnInit";
  EXPECT_LT(PInit, GPInit) << "Parent OnInit before Grandparent OnInit";
}

// --- OnUpdate dispatched to parent and child ---

TEST_F(ScriptSystemTest, OnUpdateDispatchedToParentAndChild)
{
  Node* Parent = Tree_->CreateNode("Parent", NodeType::Node3D, nullptr);
  Node* Child  = Tree_->CreateNode("Child",  NodeType::Node3D, Parent);

  Parent->AttachScript(new OrderedScript("P"));
  Child->AttachScript(new OrderedScript("C"));

  gCallLog.clear();
  RunUpdate(); // OnInit fires first (bottom-up), then OnUpdate

  int PUpdate = LogIndex("P.OnUpdate");
  int CUpdate = LogIndex("C.OnUpdate");

  ASSERT_GE(PUpdate, 0) << "Parent.OnUpdate should appear in log";
  ASSERT_GE(CUpdate, 0) << "Child.OnUpdate should appear in log";

  // Note: flat-list dispatch order is not guaranteed to be tree-order.
  // The spec says "script callbacks are independent" and flat order is acceptable.
}

// --- OnUpdate dispatched to three-level hierarchy ---

TEST_F(ScriptSystemTest, OnUpdateDispatchedToThreeLevelHierarchy)
{
  Node* GP     = Tree_->CreateNode("Grandparent", NodeType::Node3D, nullptr);
  Node* Parent = Tree_->CreateNode("Parent",      NodeType::Node3D, GP);
  Node* Child  = Tree_->CreateNode("Child",       NodeType::Node3D, Parent);

  GP->AttachScript(new OrderedScript("GP"));
  Parent->AttachScript(new OrderedScript("P"));
  Child->AttachScript(new OrderedScript("C"));

  gCallLog.clear();
  RunUpdate();

  int GPUpdate = LogIndex("GP.OnUpdate");
  int PUpdate  = LogIndex("P.OnUpdate");
  int CUpdate  = LogIndex("C.OnUpdate");

  ASSERT_GE(GPUpdate, 0) << "Grandparent.OnUpdate must appear";
  ASSERT_GE(PUpdate,  0) << "Parent.OnUpdate must appear";
  ASSERT_GE(CUpdate,  0) << "Child.OnUpdate must appear";
}

// --- OnLateUpdate fires after all OnUpdate calls complete ---

TEST_F(ScriptSystemTest, OnLateUpdateAfterAllOnUpdate)
{
  Node* NodeA = Tree_->CreateNode("A", NodeType::Node3D, nullptr);
  Node* NodeB = Tree_->CreateNode("B", NodeType::Node3D, nullptr);

  NodeA->AttachScript(new OrderedScript("A"));
  NodeB->AttachScript(new OrderedScript("B"));

  gCallLog.clear();

  // Simulate what the frame loop does: Update then LateUpdate
  RunUpdate();
  RunLateUpdate();

  // Every OnUpdate entry must appear before every OnLateUpdate entry
  int AUpdate     = LogIndex("A.OnUpdate");
  int BUpdate     = LogIndex("B.OnUpdate");
  int ALateUpdate = LogIndex("A.OnLateUpdate");
  int BLateUpdate = LogIndex("B.OnLateUpdate");

  ASSERT_GE(AUpdate,     0);
  ASSERT_GE(BUpdate,     0);
  ASSERT_GE(ALateUpdate, 0);
  ASSERT_GE(BLateUpdate, 0);

  EXPECT_LT(AUpdate,  ALateUpdate) << "A.OnUpdate must be before A.OnLateUpdate";
  EXPECT_LT(BUpdate,  BLateUpdate) << "B.OnUpdate must be before B.OnLateUpdate";
  EXPECT_LT(AUpdate,  BLateUpdate) << "A.OnUpdate must be before B.OnLateUpdate";
  EXPECT_LT(BUpdate,  ALateUpdate) << "B.OnUpdate must be before A.OnLateUpdate";
}

// --- OnLateUpdate dispatched to parent and child ---

TEST_F(ScriptSystemTest, OnLateUpdateDispatchedToParentAndChild)
{
  Node* Parent = Tree_->CreateNode("Parent", NodeType::Node3D, nullptr);
  Node* Child  = Tree_->CreateNode("Child",  NodeType::Node3D, Parent);

  Parent->AttachScript(new OrderedScript("P"));
  Child->AttachScript(new OrderedScript("C"));

  gCallLog.clear();
  RunUpdate();
  RunLateUpdate();

  int PLate = LogIndex("P.OnLateUpdate");
  int CLate = LogIndex("C.OnLateUpdate");

  ASSERT_GE(PLate, 0) << "Parent.OnLateUpdate must appear";
  ASSERT_GE(CLate, 0) << "Child.OnLateUpdate must appear";
}

// --- OnFixedUpdate dispatched to all scripted nodes ---

TEST_F(ScriptSystemTest, OnFixedUpdateDispatchedToAll)
{
  Node* GP     = Tree_->CreateNode("Grandparent", NodeType::Node3D, nullptr);
  Node* Parent = Tree_->CreateNode("Parent",      NodeType::Node3D, GP);
  Node* Child  = Tree_->CreateNode("Child",       NodeType::Node3D, Parent);

  GP->AttachScript(new OrderedScript("GP"));
  Parent->AttachScript(new OrderedScript("P"));
  Child->AttachScript(new OrderedScript("C"));

  // Initialize scripts via the Update pass first
  gCallLog.clear();
  RunUpdate();
  gCallLog.clear();

  // Now run a fixed update
  RunFixedUpdate();

  int GPFixed = LogIndex("GP.OnFixedUpdate");
  int PFixed  = LogIndex("P.OnFixedUpdate");
  int CFixed  = LogIndex("C.OnFixedUpdate");

  ASSERT_GE(GPFixed, 0) << "Grandparent.OnFixedUpdate must appear";
  ASSERT_GE(PFixed,  0) << "Parent.OnFixedUpdate must appear";
  ASSERT_GE(CFixed,  0) << "Child.OnFixedUpdate must appear";
}

// --- Per-frame callbacks suppressed for uninitialized scripts ---

TEST_F(ScriptSystemTest, PerFrameCallbacksSuppressedForUninitializedScript)
{
  Node* N = Tree_->CreateNode("Node", NodeType::Node3D, nullptr);
  auto* Script = new CountingScript();
  N->AttachScript(Script);

  // Do NOT call RunUpdate (which would run ProcessPendingInits).
  // Call only the per-frame passes directly.
  RunFixedUpdate();
  RunLateUpdate();

  EXPECT_EQ(Script->FixedUpdateCount, 0)
    << "OnFixedUpdate must not fire before OnInit";
  EXPECT_EQ(Script->LateUpdateCount, 0)
    << "OnLateUpdate must not fire before OnInit";
  EXPECT_EQ(Script->UpdateCount, 0)
    << "OnUpdate must not fire before OnInit";
}

// --- Per-frame callbacks suppressed for inactive nodes ---

TEST_F(ScriptSystemTest, PerFrameCallbacksSuppressedForInactiveNode)
{
  Node* N = Tree_->CreateNode("Node", NodeType::Node3D, nullptr);
  auto* Script = new CountingScript();
  N->AttachScript(Script);

  // Initialize the script first
  RunUpdate();
  ASSERT_EQ(Script->InitCount, 1);
  ASSERT_EQ(Script->UpdateCount, 1);

  // Deactivate the node
  N->SetActive(false);
  ASSERT_EQ(Script->DisableCount, 1);

  int UpdateBefore      = Script->UpdateCount;
  int FixedUpdateBefore = Script->FixedUpdateCount;
  int LateUpdateBefore  = Script->LateUpdateCount;

  // Run a full frame
  RunUpdate();
  RunFixedUpdate();
  RunLateUpdate();

  EXPECT_EQ(Script->UpdateCount,      UpdateBefore)
    << "OnUpdate must be suppressed while node is inactive";
  EXPECT_EQ(Script->FixedUpdateCount, FixedUpdateBefore)
    << "OnFixedUpdate must be suppressed while node is inactive";
  EXPECT_EQ(Script->LateUpdateCount,  LateUpdateBefore)
    << "OnLateUpdate must be suppressed while node is inactive";
}

// --- Node with no script is traversed without error ---

TEST_F(ScriptSystemTest, NodeWithNoScriptTraversedWithoutError)
{
  // Create nodes without any scripts
  Tree_->CreateNode("NodeA", NodeType::Node3D, nullptr);
  Node* Parent = Tree_->CreateNode("Parent", NodeType::Node3D, nullptr);
  Tree_->CreateNode("Child", NodeType::Node3D, Parent);

  EXPECT_NO_FATAL_FAILURE(RunUpdate());
  EXPECT_NO_FATAL_FAILURE(RunFixedUpdate());
  EXPECT_NO_FATAL_FAILURE(RunLateUpdate());
}

// --- A scene with no scripted nodes produces no callbacks and no crashes ---

TEST_F(ScriptSystemTest, SceneWithNoScriptedNodesNoCallbacksNoCrash)
{
  Tree_->CreateNode("A", NodeType::Node3D, nullptr);
  Tree_->CreateNode("B", NodeType::Node3D, nullptr);

  EXPECT_NO_FATAL_FAILURE(RunUpdate());
  EXPECT_NO_FATAL_FAILURE(RunFixedUpdate());
  EXPECT_NO_FATAL_FAILURE(RunLateUpdate());

  EXPECT_TRUE(gCallLog.empty()) << "No callbacks expected for scene with no scripts";
}

// --- Inactive parent suppresses children ---

TEST_F(ScriptSystemTest, InactiveParentSuppressesChildrenCallbacks)
{
  Node* Parent = Tree_->CreateNode("Parent", NodeType::Node3D, nullptr);
  Node* Child  = Tree_->CreateNode("Child",  NodeType::Node3D, Parent);

  auto* PScript = new CountingScript();
  auto* CScript = new CountingScript();
  Parent->AttachScript(PScript);
  Child->AttachScript(CScript);

  // Initialize both scripts
  RunUpdate();
  ASSERT_EQ(PScript->InitCount, 1);
  ASSERT_EQ(CScript->InitCount, 1);

  // Deactivate the parent
  Parent->SetActive(false);

  int PUpdateBefore = PScript->UpdateCount;
  int CUpdateBefore = CScript->UpdateCount;

  RunUpdate();

  EXPECT_EQ(PScript->UpdateCount, PUpdateBefore)
    << "Inactive parent script must not receive OnUpdate";

  EXPECT_EQ(CScript->UpdateCount, CUpdateBefore)
    << "Child of inactive parent must not receive OnUpdate";
}

//=============================================================================
// Task 4.2 Tests -- Runtime attachment deferred init
//=============================================================================

// --- Script attached mid-frame has IsInitialized_ == false ---

TEST_F(ScriptSystemTest, ScriptAttachedMidFrameNotInitialized)
{
  Node* N = Tree_->CreateNode("Node", NodeType::Node3D, nullptr);
  auto* Script = new CountingScript();
  N->AttachScript(Script);

  // Script should not be initialized just from attachment
  EXPECT_FALSE(Script->IsInitialized())
    << "Script should not be initialized immediately after AttachScript";
}

// --- After next Update pass, OnInit fires bottom-up then OnEnable fires ---

TEST_F(ScriptSystemTest, DeferredInitFiresOnNextUpdate)
{
  Node* N = Tree_->CreateNode("Node", NodeType::Node3D, nullptr);
  auto* Script = new CountingScript();
  N->AttachScript(Script);

  ASSERT_FALSE(Script->IsInitialized());
  ASSERT_EQ(Script->InitCount, 0);
  ASSERT_EQ(Script->EnableCount, 0);

  // First Update pass should initialize the script
  RunUpdate();

  EXPECT_TRUE(Script->IsInitialized())
    << "Script should be initialized after first Update pass";
  EXPECT_EQ(Script->InitCount, 1)
    << "OnInit should fire exactly once";
  EXPECT_EQ(Script->EnableCount, 1)
    << "OnEnable should fire after OnInit (node is active)";
}

// --- Per-frame callbacks do not fire until after OnInit ---

TEST_F(ScriptSystemTest, PerFrameCallbacksNotUntilAfterOnInit)
{
  Node* N = Tree_->CreateNode("Node", NodeType::Node3D, nullptr);
  auto* Script = new CountingScript();
  N->AttachScript(Script);

  // Simulate mid-frame: fixed/late update called before the init Update pass
  RunFixedUpdate();
  RunLateUpdate();

  EXPECT_EQ(Script->FixedUpdateCount, 0) << "FixedUpdate before OnInit";
  EXPECT_EQ(Script->LateUpdateCount,  0) << "LateUpdate before OnInit";

  // Now run Update, which triggers OnInit and then the first OnUpdate
  RunUpdate();

  EXPECT_EQ(Script->InitCount,   1) << "OnInit should fire";
  EXPECT_EQ(Script->UpdateCount, 1) << "First OnUpdate fires in same pass as OnInit";
}

// --- After deferred OnInit, per-frame callbacks begin normally ---

TEST_F(ScriptSystemTest, AfterDeferredInitPerFrameCallbacksBeginNormally)
{
  Node* N = Tree_->CreateNode("Node", NodeType::Node3D, nullptr);
  auto* Script = new CountingScript();
  N->AttachScript(Script);

  RunUpdate(); // frame 1: init + first update
  ASSERT_EQ(Script->InitCount,   1);
  ASSERT_EQ(Script->UpdateCount, 1);

  RunUpdate(); // frame 2
  EXPECT_EQ(Script->InitCount,   1) << "OnInit must not fire again";
  EXPECT_EQ(Script->UpdateCount, 2) << "OnUpdate fires each frame";

  RunFixedUpdate();
  EXPECT_EQ(Script->FixedUpdateCount, 1);

  RunLateUpdate();
  EXPECT_EQ(Script->LateUpdateCount, 1);
}

// --- OnEnable fires after deferred OnInit; not at attachment time ---

TEST_F(ScriptSystemTest, OnEnableFiredAfterDeferredInit)
{
  Node* N = Tree_->CreateNode("Node", NodeType::Node3D, nullptr);
  auto* Script = new CountingScript();
  N->AttachScript(Script);

  // Immediately after attachment: no OnEnable yet (not initialized)
  EXPECT_EQ(Script->EnableCount, 0)
    << "OnEnable must not fire at attachment time";

  RunUpdate(); // triggers OnInit then OnEnable (node is active)
  EXPECT_EQ(Script->EnableCount, 1)
    << "OnEnable must fire after deferred OnInit";
}

// --- If node is inactive when OnInit is processed, OnEnable does not fire ---

TEST_F(ScriptSystemTest, OnEnableNotFiredIfNodeInactiveAtInit)
{
  Node* N = Tree_->CreateNode("Node", NodeType::Node3D, nullptr);
  auto* Script = new CountingScript();
  N->AttachScript(Script);

  // Deactivate the node before the init pass
  N->SetActive(false);

  RunUpdate(); // ProcessPendingInits runs; node is inactive so OnEnable skipped

  EXPECT_EQ(Script->InitCount,   1) << "OnInit should still fire";
  EXPECT_EQ(Script->EnableCount, 0) << "OnEnable must not fire for inactive node";
}

// --- Multiple runtime attachments in one frame get deferred OnInit bottom-up ---

TEST_F(ScriptSystemTest, MultipleRuntimeAttachmentsGetDeferredInitBottomUp)
{
  Node* Parent = Tree_->CreateNode("Parent", NodeType::Node3D, nullptr);
  Node* Child  = Tree_->CreateNode("Child",  NodeType::Node3D, Parent);

  Parent->AttachScript(new OrderedScript("P"));
  Child->AttachScript(new OrderedScript("C"));

  gCallLog.clear();
  RunUpdate(); // Should init child before parent (bottom-up)

  int PInit = LogIndex("P.OnInit");
  int CInit = LogIndex("C.OnInit");

  ASSERT_GE(PInit, 0);
  ASSERT_GE(CInit, 0);
  EXPECT_LT(CInit, PInit) << "Child OnInit before Parent OnInit (bottom-up)";
}

//=============================================================================
// Task 5.1 -- End-to-end lifecycle sequence across a three-level hierarchy
//=============================================================================

TEST_F(ScriptSystemTest, E2E_FullLifecycleSequenceThreeLevelHierarchy)
{
  // Build hierarchy: Grandparent -> Parent -> Child
  Node* GP     = Tree_->CreateNode("GP",     NodeType::Node3D, nullptr);
  Node* Parent = Tree_->CreateNode("Parent", NodeType::Node3D, GP);
  Node* Child  = Tree_->CreateNode("Child",  NodeType::Node3D, Parent);

  GP->AttachScript(new OrderedScript("GP"));
  Parent->AttachScript(new OrderedScript("P"));
  Child->AttachScript(new OrderedScript("C"));

  gCallLog.clear();

  // Run fixed update first (scripts not yet initialized -- should produce no
  // per-frame callbacks since init only happens in Update).
  RunFixedUpdate(0.02f);

  // Run variable update: triggers init (bottom-up) then OnUpdate.
  RunUpdate(0.016f);

  // --- Assert OnInit bottom-up ordering ---
  int GPInit = LogIndex("GP.OnInit");
  int PInit  = LogIndex("P.OnInit");
  int CInit  = LogIndex("C.OnInit");

  ASSERT_GE(CInit,  0) << "Child.OnInit must appear";
  ASSERT_GE(PInit,  0) << "Parent.OnInit must appear";
  ASSERT_GE(GPInit, 0) << "Grandparent.OnInit must appear";

  EXPECT_LT(CInit, PInit)  << "OnInit: Child before Parent (bottom-up)";
  EXPECT_LT(PInit, GPInit) << "OnInit: Parent before Grandparent (bottom-up)";

  // --- Assert OnUpdate fired for all three ---
  int GPUpdate = LogIndex("GP.OnUpdate");
  int PUpdate  = LogIndex("P.OnUpdate");
  int CUpdate  = LogIndex("C.OnUpdate");

  ASSERT_GE(GPUpdate, 0) << "Grandparent.OnUpdate must appear";
  ASSERT_GE(PUpdate,  0) << "Parent.OnUpdate must appear";
  ASSERT_GE(CUpdate,  0) << "Child.OnUpdate must appear";

  // --- Assert OnFixedUpdate did NOT fire before init (scripts uninitialized) ---
  int GPFixed = LogIndex("GP.OnFixedUpdate");
  int PFixed  = LogIndex("P.OnFixedUpdate");
  int CFixed  = LogIndex("C.OnFixedUpdate");

  EXPECT_LT(GPFixed, 0) << "OnFixedUpdate must not fire before OnInit";
  EXPECT_LT(PFixed,  0) << "OnFixedUpdate must not fire before OnInit";
  EXPECT_LT(CFixed,  0) << "OnFixedUpdate must not fire before OnInit";

  // --- Run a second fixed update (scripts now initialized) and verify ---
  gCallLog.clear();
  RunFixedUpdate(0.02f);

  GPFixed = LogIndex("GP.OnFixedUpdate");
  PFixed  = LogIndex("P.OnFixedUpdate");
  CFixed  = LogIndex("C.OnFixedUpdate");

  ASSERT_GE(GPFixed, 0) << "Grandparent.OnFixedUpdate must appear after init";
  ASSERT_GE(PFixed,  0) << "Parent.OnFixedUpdate must appear after init";
  ASSERT_GE(CFixed,  0) << "Child.OnFixedUpdate must appear after init";

  // --- Run a second variable update and assert OnFixedUpdate precedes OnUpdate ---
  gCallLog.clear();
  RunFixedUpdate(0.02f);
  RunUpdate(0.016f);

  // In this combined log, all FixedUpdate entries should come before all Update entries
  int GPFixed2 = LogIndex("GP.OnFixedUpdate");
  int CFixed2  = LogIndex("C.OnFixedUpdate");
  int GPUpdate2 = LogIndex("GP.OnUpdate");
  int CUpdate2  = LogIndex("C.OnUpdate");

  ASSERT_GE(GPFixed2, 0);
  ASSERT_GE(CFixed2,  0);
  ASSERT_GE(GPUpdate2, 0);
  ASSERT_GE(CUpdate2,  0);

  EXPECT_LT(GPFixed2, GPUpdate2) << "Grandparent.OnFixedUpdate fires before Grandparent.OnUpdate";
  EXPECT_LT(CFixed2,  CUpdate2)  << "Child.OnFixedUpdate fires before Child.OnUpdate";
}

//=============================================================================
// Task 5.2 -- Active/inactive suppression across a full frame
//=============================================================================

TEST_F(ScriptSystemTest, E2E_InactiveNodeMidHierarchySuppressesSubtree)
{
  // Build hierarchy: Grandparent -> Parent -> Child
  Node* GP     = Tree_->CreateNode("GP",     NodeType::Node3D, nullptr);
  Node* Parent = Tree_->CreateNode("Parent", NodeType::Node3D, GP);
  Node* Child  = Tree_->CreateNode("Child",  NodeType::Node3D, Parent);

  auto* GPScript = new CountingScript();
  auto* PScript  = new CountingScript();
  auto* CScript  = new CountingScript();

  GP->AttachScript(GPScript);
  Parent->AttachScript(PScript);
  Child->AttachScript(CScript);

  // Frame 1: initialize all scripts
  RunUpdate(0.016f);

  ASSERT_EQ(GPScript->InitCount, 1) << "Grandparent script must be initialized";
  ASSERT_EQ(PScript->InitCount,  1) << "Parent script must be initialized";
  ASSERT_EQ(CScript->InitCount,  1) << "Child script must be initialized";

  // Deactivate the mid-hierarchy node (Parent).
  Parent->SetActive(false);
  ASSERT_EQ(PScript->DisableCount, 1) << "Parent script must receive OnDisable";

  // Run a full frame: FixedUpdate + Update + LateUpdate
  RunFixedUpdate(0.02f);
  RunUpdate(0.016f);
  RunLateUpdate(0.016f);

  // Grandparent (active, initialized) must receive all per-frame callbacks
  EXPECT_EQ(GPScript->FixedUpdateCount, 1) << "Grandparent must receive OnFixedUpdate";
  EXPECT_EQ(GPScript->UpdateCount,      2) << "Grandparent must receive OnUpdate (frame 1 + frame 2)";
  EXPECT_EQ(GPScript->LateUpdateCount,  1) << "Grandparent must receive OnLateUpdate";

  // Parent (inactive) must receive NO additional per-frame callbacks
  EXPECT_EQ(PScript->FixedUpdateCount, 0) << "Inactive Parent must NOT receive OnFixedUpdate";
  EXPECT_EQ(PScript->UpdateCount,      1) << "Inactive Parent must NOT receive additional OnUpdate (only frame 1)";
  EXPECT_EQ(PScript->LateUpdateCount,  0) << "Inactive Parent must NOT receive OnLateUpdate";

  // Child (child of inactive parent) must receive NO per-frame callbacks
  EXPECT_EQ(CScript->FixedUpdateCount, 0) << "Child of inactive parent must NOT receive OnFixedUpdate";
  EXPECT_EQ(CScript->UpdateCount,      1) << "Child of inactive parent must NOT receive additional OnUpdate";
  EXPECT_EQ(CScript->LateUpdateCount,  0) << "Child of inactive parent must NOT receive OnLateUpdate";
}
