/**
 * AxSceneTreeTests.cpp - Tests for SceneTree traversal and frame loop methods
 *
 * Tests SceneTree's direct traversal behaviour (no system registry):
 *   - Update dispatches OnUpdate top-down to initialized active scripts
 *   - FixedUpdate dispatches OnFixedUpdate top-down, skips uninitialized scripts
 *   - LateUpdate dispatches OnLateUpdate top-down, skips inactive nodes/subtrees
 *   - Update runs bottom-up init scan before OnUpdate dispatch
 *   - Update calls UpdateNodeTransforms before script dispatch
 *   - Unload when nothing loaded does not crash
 *
 * Uses real Node objects and ScriptBase subclasses (no mocks).
 * Follows the same tracking pattern from AxScriptSystemTests.cpp.
 *
 * NOTE: Script subclass names must be unique across all TUs linked into
 * AxEngineTests to avoid ODR violations. Hence "SceneTreeTrackingScript"
 * and "SceneTreeCountingScript" rather than plain "TrackingScript".
 *
 * ==========================================================================
 * DELETED SYMBOLS (SceneTree Refactor)
 * ==========================================================================
 * The following symbols were intentionally removed as part of the SceneTree
 * refactor that replaced the AxScene + System registry pattern with a single
 * SceneTree class that directly owns traversal methods:
 *
 *   - AxSystem (class)           -- base class for ECS-style systems
 *   - SystemPhase (enum)         -- EarlyUpdate/Update/FixedUpdate/LateUpdate/Render
 *   - SystemFactory              -- factory for runtime system registration
 *   - ScriptSystem (class)       -- per-node script dispatch via system registry
 *   - TransformSystem (class)    -- transform propagation via system registry
 *   - RenderSystem (class)       -- renderable collection via system registry
 *   - AxCoreSystems              -- registration of core systems (Register/Unregister)
 *   - AxSceneManager (class)     -- singleton scene lifecycle manager
 *   - AxScene::RegisterSystem    -- system registration on old AxScene class
 *   - AxScene::UnregisterSystem  -- system unregistration on old AxScene class
 *   - AxScene::UpdateSystems     -- dispatching all registered systems
 *   - AxScene::FixedUpdateSystems -- dispatching fixed-update systems
 *   - AxScene::LateUpdateSystems  -- dispatching late-update systems
 *   - AxScene::GetSystemCount    -- querying number of registered systems
 *   - AX_SCENE_MAX_SYSTEMS       -- constant for max system array size
 *   - AxStubSystems.h            -- stub system declarations
 *
 * These were replaced by SceneTree::Update, SceneTree::FixedUpdate, and
 * SceneTree::LateUpdate which directly perform transform propagation,
 * bottom-up init scan, and top-down script dispatch without indirection
 * through a system registry.
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
// Test 1: Update dispatches OnUpdate top-down to initialized active scripts
//=============================================================================

TEST_F(SceneTreeTest, UpdateDispatchesOnUpdateTopDownToInitializedActiveScripts)
{
  // Build hierarchy: Parent -> Child
  Node* Parent = Tree_->CreateNode("Parent", NodeType::Node3D, nullptr);
  Node* Child  = Tree_->CreateNode("Child",  NodeType::Node3D, Parent);

  Parent->AttachScript(new SceneTreeTrackingScript("P"));
  Child->AttachScript(new SceneTreeTrackingScript("C"));

  gSceneTreeCallLog.clear();

  // First Update: init scan (bottom-up) then OnUpdate (top-down)
  Tree_->Update(0.016f);

  // Verify OnUpdate was dispatched
  int PUpdate = SceneTreeLogIndex("P.OnUpdate");
  int CUpdate = SceneTreeLogIndex("C.OnUpdate");

  ASSERT_GE(PUpdate, 0) << "Parent.OnUpdate should appear in log";
  ASSERT_GE(CUpdate, 0) << "Child.OnUpdate should appear in log";
  EXPECT_LT(PUpdate, CUpdate)
    << "Parent OnUpdate must fire before Child OnUpdate (top-down)";
}

//=============================================================================
// Test 2: FixedUpdate dispatches OnFixedUpdate top-down, skips uninitialized
//=============================================================================

TEST_F(SceneTreeTest, FixedUpdateTopDownSkipsUninitializedScripts)
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

  // Now run FixedUpdate -- scripts are initialized, should fire top-down
  gSceneTreeCallLog.clear();
  GP->AttachScript(new SceneTreeTrackingScript("GP"));
  Parent->AttachScript(new SceneTreeTrackingScript("P"));
  Child->AttachScript(new SceneTreeTrackingScript("C"));

  // Re-initialize the new tracking scripts
  Tree_->Update(0.016f);
  gSceneTreeCallLog.clear();

  Tree_->FixedUpdate(0.02f);

  int GPFixed = SceneTreeLogIndex("GP.OnFixedUpdate");
  int PFixed  = SceneTreeLogIndex("P.OnFixedUpdate");
  int CFixed  = SceneTreeLogIndex("C.OnFixedUpdate");

  ASSERT_GE(GPFixed, 0) << "Grandparent.OnFixedUpdate must appear after init";
  ASSERT_GE(PFixed,  0) << "Parent.OnFixedUpdate must appear after init";
  ASSERT_GE(CFixed,  0) << "Child.OnFixedUpdate must appear after init";

  EXPECT_LT(GPFixed, PFixed) << "OnFixedUpdate: Grandparent before Parent (top-down)";
  EXPECT_LT(PFixed,  CFixed) << "OnFixedUpdate: Parent before Child (top-down)";
}

//=============================================================================
// Test 3: LateUpdate dispatches top-down, skips inactive nodes and subtrees
//=============================================================================

TEST_F(SceneTreeTest, LateUpdateTopDownSkipsInactiveNodesAndSubtrees)
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
// Test 4: Update runs bottom-up init scan before OnUpdate
//         (children initialized before parents)
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

  // First Update: should init bottom-up then OnUpdate top-down
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

  // Verify update order: Grandparent before Parent before Child (top-down)
  EXPECT_LT(GPUpdate, PUpdate) << "OnUpdate: Grandparent before Parent (top-down)";
  EXPECT_LT(PUpdate, CUpdate)  << "OnUpdate: Parent before Child (top-down)";
}

//=============================================================================
// Test 5: Update calls UpdateNodeTransforms before script dispatch --
//         world matrices are valid when OnUpdate fires
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
