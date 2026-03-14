/**
 * AxEditorReadinessTests.cpp - Tests for Editor Readiness features
 *
 * Tests for Task Groups 1, 2, 3, 4, 5, and 6 of the Engine Editor Readiness spec:
 *
 * Task Group 1: Externally-Driven Engine Lifecycle & Window Decoupling
 *   - AxEngineConfig has ExternalWindowHandle, ViewportWidth, ViewportHeight
 *   - AxEngineAPI has Tick, Resize, and SetMode function pointers
 *   - AxEngineMode enum exists with Edit and Play values
 *   - AxInput no-ops on Update() when uninitialized (nullptr Window)
 *
 * Task Group 2: Editor/Play Mode Toggle
 *   - Edit mode runs transforms but skips scripts in SceneTree
 *   - Play mode runs everything (transforms + scripts)
 *   - Mode can be toggled at runtime
 *   - FixedUpdate and LateUpdate skip script dispatch in Edit mode
 *
 * Task Group 3: Play Mode Scene Snapshot & Restore
 *   - AxEngineAPI has EnterPlayMode and ExitPlayMode function pointers
 *   - Snapshot captures scene state via SaveSceneToString
 *   - Restore returns scene to pre-play state via LoadSceneFromString
 *   - Script modifications during play are discarded on restore
 *
 * Task Group 4: Runtime Scene Management API
 *   - AxEngineAPI has LoadScene, UnloadScene, NewScene, SaveScene pointers
 *   - Task 4.4/4.5 verify editor-hosted vs standalone scene loading behavior
 *
 * Task Group 5: Scene Serialization (Save)
 *   - SaveSceneToString serializes a SceneTree to .ats format
 *   - Round-trip: parse -> serialize -> re-parse produces equivalent tree
 *
 * Task Group 6: Editor Camera
 *   - EditorCameraState struct has default values
 *   - AxEngineAPI has SetEditorCamera function pointer
 *   - AxRenderer UseEditorCamera flag concept tested via struct defaults
 *
 * These tests exercise the SceneTree and public API surface directly.
 * Engine-level integration (AxEngine class with real plugins) cannot be
 * tested in the unit test binary since it requires DLL loading and GPU
 * resources.
 */

#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxHashTable.h"
#include "Foundation/AxAllocatorAPI.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxMath.h"
#include "AxEngine/AxEngine.h"
#include "AxEngine/AxSceneTree.h"
#include "AxEngine/AxSceneParser.h"
#include "AxEngine/AxNode.h"
#include "AxEngine/AxTypedNodes.h"
#include "AxEngine/AxScriptBase.h"
#include "AxEngine/AxInput.h"
#include "AxWindow/AxWindow.h"

#include <string>
#include <vector>
#include <cmath>

//=============================================================================
// EditorCountingScript - counts invocations per callback
//
// Named with Editor prefix to avoid ODR collisions with scripts in other TUs.
//=============================================================================

struct EditorCountingScript : public ScriptBase
{
  int InitCount        = 0;
  int EnableCount      = 0;
  int UpdateCount      = 0;
  int FixedUpdateCount = 0;
  int LateUpdateCount  = 0;

  void OnInit()             override { ++InitCount; }
  void OnEnable()           override { ++EnableCount; }
  void OnUpdate(float)      override { ++UpdateCount; }
  void OnFixedUpdate(float) override { ++FixedUpdateCount; }
  void OnLateUpdate(float)  override { ++LateUpdateCount; }
};

//=============================================================================
// EditorModifyingScript - modifies node position during Update
//
// Used to verify that play-time modifications are discarded on restore.
//=============================================================================

struct EditorModifyingScript : public ScriptBase
{
  void OnUpdate(float) override
  {
    if (GetOwner()) {
      GetOwner()->SetPosition({99.0f, 99.0f, 99.0f});
    }
  }
};

//=============================================================================
// Test Fixture: EditorReadiness (SceneTree-level tests)
//=============================================================================

class EditorReadinessTest : public testing::Test
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
    Tree_->Name = "EditorTestScene";
  }

  void TearDown() override
  {
    delete Tree_;
    Tree_ = nullptr;

    AxonTermGlobalAPIRegistry();
  }

  AxHashTableAPI* TableAPI_{nullptr};
  SceneTree* Tree_{nullptr};
};

//=============================================================================
// Task Group 1: AxEngineConfig and AxEngineAPI surface tests
//
// These tests verify the public struct/enum definitions compile correctly
// and have the expected default values.
//=============================================================================

TEST(EngineConfigTest, DefaultExternalWindowHandleIsZero)
{
  AxEngineConfig Config{};
  EXPECT_EQ(Config.ExternalWindowHandle, 0u);
}

TEST(EngineConfigTest, DefaultViewportDimensions)
{
  AxEngineConfig Config{};
  EXPECT_EQ(Config.ViewportWidth, 1280);
  EXPECT_EQ(Config.ViewportHeight, 720);
}

TEST(EngineConfigTest, ExternalWindowHandleCanBeSet)
{
  AxEngineConfig Config{};
  Config.ExternalWindowHandle = 0xDEADBEEF;
  Config.ViewportWidth = 800;
  Config.ViewportHeight = 600;

  EXPECT_EQ(Config.ExternalWindowHandle, 0xDEADBEEFu);
  EXPECT_EQ(Config.ViewportWidth, 800);
  EXPECT_EQ(Config.ViewportHeight, 600);
}

TEST(EngineModeEnumTest, EditAndPlayValuesExist)
{
  EXPECT_EQ(static_cast<int32_t>(AxEngineMode::Edit), 0);
  EXPECT_EQ(static_cast<int32_t>(AxEngineMode::Play), 1);
  EXPECT_NE(AxEngineMode::Edit, AxEngineMode::Play);
}

TEST(EngineAPIStructTest, HasTickFunctionPointer)
{
  AxEngineAPI API{};
  EXPECT_EQ(API.Tick, nullptr);
}

TEST(EngineAPIStructTest, HasResizeFunctionPointer)
{
  AxEngineAPI API{};
  EXPECT_EQ(API.Resize, nullptr);
}

TEST(EngineAPIStructTest, HasSetModeFunctionPointer)
{
  AxEngineAPI API{};
  EXPECT_EQ(API.SetMode, nullptr);
}

TEST(EngineAPIStructTest, AllFunctionPointersPresent)
{
  AxEngineAPI API{};
  EXPECT_EQ(API.Initialize, nullptr);
  EXPECT_EQ(API.Run, nullptr);
  EXPECT_EQ(API.Tick, nullptr);
  EXPECT_EQ(API.Shutdown, nullptr);
  EXPECT_EQ(API.IsRunning, nullptr);
  EXPECT_EQ(API.Resize, nullptr);
  EXPECT_EQ(API.SetMode, nullptr);
  EXPECT_EQ(API.LoadScene, nullptr);
  EXPECT_EQ(API.UnloadScene, nullptr);
  EXPECT_EQ(API.NewScene, nullptr);
  EXPECT_EQ(API.SaveScene, nullptr);
  EXPECT_EQ(API.EnterPlayMode, nullptr);
  EXPECT_EQ(API.ExitPlayMode, nullptr);
  EXPECT_EQ(API.SetEditorCamera, nullptr);
}

//=============================================================================
// Task 1.4: AxInput no-op when initialized with nullptr Window
//=============================================================================

TEST(InputNoOpTest, UpdateDoesNotCrashWithNullWindow)
{
  // Simulate editor-hosted mode: AxInput initialized with nullptr Window.
  // Update() should gracefully no-op and key queries return safe defaults.
  AxonInitGlobalAPIRegistry();
  AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);

  AxInput& Input = AxInput::Get();
  Input.Initialize(AxonGlobalAPIRegistry, nullptr);

  // Should not crash
  Input.Update();

  // Key queries should return safe defaults
  EXPECT_FALSE(Input.IsKeyDown(AX_KEY_ESCAPE));
  EXPECT_FALSE(Input.IsKeyPressed(AX_KEY_ESCAPE));
  EXPECT_FALSE(Input.IsKeyReleased(AX_KEY_ESCAPE));
  EXPECT_FLOAT_EQ(Input.GetAxis(AX_KEY_W, AX_KEY_S), 0.0f);

  AxVec2 MouseDelta = Input.GetMouseDelta();
  EXPECT_FLOAT_EQ(MouseDelta.X, 0.0f);
  EXPECT_FLOAT_EQ(MouseDelta.Y, 0.0f);

  Input.Shutdown();
  AxonTermGlobalAPIRegistry();
}

//=============================================================================
// Task Group 2: Edit/Play Mode Toggle via SceneTree
//=============================================================================

TEST_F(EditorReadinessTest, ScriptsEnabledByDefault)
{
  EXPECT_TRUE(Tree_->AreScriptsEnabled());
}

TEST_F(EditorReadinessTest, SetScriptsEnabledToggle)
{
  Tree_->SetScriptsEnabled(false);
  EXPECT_FALSE(Tree_->AreScriptsEnabled());

  Tree_->SetScriptsEnabled(true);
  EXPECT_TRUE(Tree_->AreScriptsEnabled());
}

TEST_F(EditorReadinessTest, EditModeRunsTransformsButSkipsScripts)
{
  Node* TestNode = Tree_->CreateNode("ScriptNode", NodeType::Node3D, nullptr);
  ASSERT_NE(TestNode, nullptr);

  auto* Script = new EditorCountingScript();
  TestNode->AttachScript(Script);

  TestNode->SetPosition({5.0f, 10.0f, 15.0f});

  // Disable scripts (Edit mode)
  Tree_->SetScriptsEnabled(false);
  Tree_->Update(0.016f);

  // Transform should have been propagated
  const Mat4& WorldTransform = TestNode->GetWorldTransform();
  EXPECT_NEAR(WorldTransform.E[3][0], 5.0f, 0.001f);
  EXPECT_NEAR(WorldTransform.E[3][1], 10.0f, 0.001f);
  EXPECT_NEAR(WorldTransform.E[3][2], 15.0f, 0.001f);

  // Script should NOT have been initialized or dispatched
  EXPECT_EQ(Script->InitCount, 0);
  EXPECT_EQ(Script->UpdateCount, 0);
}

TEST_F(EditorReadinessTest, PlayModeRunsEverything)
{
  Node* TestNode = Tree_->CreateNode("ScriptNode", NodeType::Node3D, nullptr);
  ASSERT_NE(TestNode, nullptr);

  auto* Script = new EditorCountingScript();
  TestNode->AttachScript(Script);
  TestNode->SetPosition({1.0f, 2.0f, 3.0f});

  // Enable scripts (Play mode -- default)
  Tree_->SetScriptsEnabled(true);
  Tree_->Update(0.016f);

  // Transform should have been propagated
  const Mat4& WorldTransform = TestNode->GetWorldTransform();
  EXPECT_NEAR(WorldTransform.E[3][0], 1.0f, 0.001f);
  EXPECT_NEAR(WorldTransform.E[3][1], 2.0f, 0.001f);
  EXPECT_NEAR(WorldTransform.E[3][2], 3.0f, 0.001f);

  // Script should have been initialized and dispatched
  EXPECT_EQ(Script->InitCount, 1);
  EXPECT_EQ(Script->EnableCount, 1);
  EXPECT_EQ(Script->UpdateCount, 1);
}

TEST_F(EditorReadinessTest, EditModeSkipsFixedUpdate)
{
  Node* TestNode = Tree_->CreateNode("ScriptNode", NodeType::Node3D, nullptr);
  auto* Script = new EditorCountingScript();
  TestNode->AttachScript(Script);

  // Initialize scripts in Play mode
  Tree_->SetScriptsEnabled(true);
  Tree_->Update(0.016f);
  EXPECT_EQ(Script->InitCount, 1);

  // Switch to Edit mode
  Tree_->SetScriptsEnabled(false);
  Tree_->FixedUpdate(1.0f / 60.0f);
  EXPECT_EQ(Script->FixedUpdateCount, 0);
}

TEST_F(EditorReadinessTest, PlayModeRunsFixedUpdate)
{
  Node* TestNode = Tree_->CreateNode("ScriptNode", NodeType::Node3D, nullptr);
  auto* Script = new EditorCountingScript();
  TestNode->AttachScript(Script);

  Tree_->SetScriptsEnabled(true);
  Tree_->Update(0.016f);
  EXPECT_EQ(Script->InitCount, 1);

  Tree_->FixedUpdate(1.0f / 60.0f);
  EXPECT_EQ(Script->FixedUpdateCount, 1);
}

TEST_F(EditorReadinessTest, EditModeSkipsLateUpdate)
{
  Node* TestNode = Tree_->CreateNode("ScriptNode", NodeType::Node3D, nullptr);
  auto* Script = new EditorCountingScript();
  TestNode->AttachScript(Script);

  // Initialize in Play mode
  Tree_->SetScriptsEnabled(true);
  Tree_->Update(0.016f);
  EXPECT_EQ(Script->InitCount, 1);

  // Switch to Edit mode
  Tree_->SetScriptsEnabled(false);
  Tree_->LateUpdate(0.016f);
  EXPECT_EQ(Script->LateUpdateCount, 0);
}

TEST_F(EditorReadinessTest, PlayModeRunsLateUpdate)
{
  Node* TestNode = Tree_->CreateNode("ScriptNode", NodeType::Node3D, nullptr);
  auto* Script = new EditorCountingScript();
  TestNode->AttachScript(Script);

  Tree_->SetScriptsEnabled(true);
  Tree_->Update(0.016f);

  Tree_->LateUpdate(0.016f);
  EXPECT_EQ(Script->LateUpdateCount, 1);
}

TEST_F(EditorReadinessTest, ModeCanBeToggledAtRuntime)
{
  Node* TestNode = Tree_->CreateNode("ScriptNode", NodeType::Node3D, nullptr);
  auto* Script = new EditorCountingScript();
  TestNode->AttachScript(Script);

  // Start in Play mode -- init and first update
  Tree_->SetScriptsEnabled(true);
  Tree_->Update(0.016f);
  EXPECT_EQ(Script->InitCount, 1);
  EXPECT_EQ(Script->UpdateCount, 1);

  // Switch to Edit mode -- update count should not increment
  Tree_->SetScriptsEnabled(false);
  Tree_->Update(0.016f);
  EXPECT_EQ(Script->UpdateCount, 1);

  // Switch back to Play mode -- update resumes
  Tree_->SetScriptsEnabled(true);
  Tree_->Update(0.016f);
  EXPECT_EQ(Script->UpdateCount, 2);
}

TEST_F(EditorReadinessTest, EditModeTransformPropagationWithHierarchy)
{
  // Create a hierarchy: Root -> Parent -> Child
  Node* Parent = Tree_->CreateNode("Parent", NodeType::Node3D, nullptr);
  Node* Child  = Tree_->CreateNode("Child",  NodeType::Node3D, Parent);

  Parent->SetPosition({10.0f, 0.0f, 0.0f});
  Child->SetPosition({0.0f, 5.0f, 0.0f});

  // Edit mode: transform propagation still runs
  Tree_->SetScriptsEnabled(false);
  Tree_->Update(0.016f);

  // Child's world position should be parent + child
  const Mat4& ChildWorld = Child->GetWorldTransform();
  EXPECT_NEAR(ChildWorld.E[3][0], 10.0f, 0.001f);
  EXPECT_NEAR(ChildWorld.E[3][1], 5.0f, 0.001f);
  EXPECT_NEAR(ChildWorld.E[3][2], 0.0f, 0.001f);
}

TEST_F(EditorReadinessTest, EditModePendingInitsNotProcessed)
{
  // Scripts attached while in Edit mode should NOT be initialized
  // until Play mode is enabled
  Tree_->SetScriptsEnabled(false);

  Node* TestNode = Tree_->CreateNode("ScriptNode", NodeType::Node3D, nullptr);
  auto* Script = new EditorCountingScript();
  TestNode->AttachScript(Script);

  Tree_->Update(0.016f);
  EXPECT_EQ(Script->InitCount, 0);
  EXPECT_FALSE(Script->IsInitialized());

  // Switch to Play mode -- script should now initialize
  Tree_->SetScriptsEnabled(true);
  Tree_->Update(0.016f);

  EXPECT_EQ(Script->InitCount, 1);
  EXPECT_TRUE(Script->IsInitialized());
  EXPECT_EQ(Script->UpdateCount, 1);
}

TEST_F(EditorReadinessTest, FullEditPlayCycleWithAllUpdatePhases)
{
  // Test a complete cycle: Edit -> Play -> Edit
  Node* TestNode = Tree_->CreateNode("ScriptNode", NodeType::Node3D, nullptr);
  auto* Script = new EditorCountingScript();
  TestNode->AttachScript(Script);

  // Edit mode: nothing dispatched
  Tree_->SetScriptsEnabled(false);
  Tree_->Update(0.016f);
  Tree_->FixedUpdate(1.0f / 60.0f);
  Tree_->LateUpdate(0.016f);
  EXPECT_EQ(Script->InitCount, 0);
  EXPECT_EQ(Script->UpdateCount, 0);
  EXPECT_EQ(Script->FixedUpdateCount, 0);
  EXPECT_EQ(Script->LateUpdateCount, 0);

  // Play mode: everything dispatched
  Tree_->SetScriptsEnabled(true);
  Tree_->Update(0.016f);
  Tree_->FixedUpdate(1.0f / 60.0f);
  Tree_->LateUpdate(0.016f);
  EXPECT_EQ(Script->InitCount, 1);
  EXPECT_EQ(Script->UpdateCount, 1);
  EXPECT_EQ(Script->FixedUpdateCount, 1);
  EXPECT_EQ(Script->LateUpdateCount, 1);

  // Back to Edit mode: counts should not change
  Tree_->SetScriptsEnabled(false);
  Tree_->Update(0.016f);
  Tree_->FixedUpdate(1.0f / 60.0f);
  Tree_->LateUpdate(0.016f);
  EXPECT_EQ(Script->UpdateCount, 1);
  EXPECT_EQ(Script->FixedUpdateCount, 1);
  EXPECT_EQ(Script->LateUpdateCount, 1);
}

//=============================================================================
// Task Group 3: Play Mode Scene Snapshot & Restore
//
// These tests verify the snapshot/restore mechanism using SceneParser
// directly (since AxEngine requires DLL loading for full integration).
// API surface tests verify the new function pointers exist.
//=============================================================================

TEST(EngineAPIPlayMode, HasEnterPlayModeFunctionPointer)
{
  AxEngineAPI API{};
  EXPECT_EQ(API.EnterPlayMode, nullptr);
}

TEST(EngineAPIPlayMode, HasExitPlayModeFunctionPointer)
{
  AxEngineAPI API{};
  EXPECT_EQ(API.ExitPlayMode, nullptr);
}

// Snapshot & Restore test fixture using SceneParser for serialization
class SnapshotRestoreTest : public testing::Test
{
protected:
  void SetUp() override
  {
    AxonInitGlobalAPIRegistry();
    AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);

    TableAPI_ = static_cast<AxHashTableAPI*>(
      AxonGlobalAPIRegistry->Get(AXON_HASH_TABLE_API_NAME));
    ASSERT_NE(TableAPI_, nullptr);

    Parser_.Init(AxonGlobalAPIRegistry);
  }

  void TearDown() override
  {
    Parser_.Term();
    AxonTermGlobalAPIRegistry();
  }

  AxHashTableAPI* TableAPI_{nullptr};
  SceneParser Parser_;
};

TEST_F(SnapshotRestoreTest, SnapshotCapturesSceneState)
{
  // Build a scene with known state
  SceneTree* Tree = new SceneTree(TableAPI_, nullptr);
  Tree->Name = "SnapshotTestScene";

  Node* NodeA = Tree->CreateNode("NodeA", NodeType::Node3D, nullptr);
  NodeA->SetPosition({10.0f, 20.0f, 30.0f});

  Node* Light = Tree->CreateNode("TestLight", NodeType::Light, nullptr);
  static_cast<LightNode*>(Light)->SetIntensity(2.5f);
  Light->SetPosition({5.0f, 5.0f, 5.0f});

  // Take a snapshot (serialize to string)
  std::string Snapshot = Parser_.SaveSceneToString(Tree);
  ASSERT_FALSE(Snapshot.empty());

  // Verify the snapshot contains our scene data
  EXPECT_NE(Snapshot.find("SnapshotTestScene"), std::string::npos);
  EXPECT_NE(Snapshot.find("NodeA"), std::string::npos);
  EXPECT_NE(Snapshot.find("TestLight"), std::string::npos);

  delete Tree;
}

TEST_F(SnapshotRestoreTest, RestoreReturnsSceneToPrePlayState)
{
  // Build a scene with known state
  SceneTree* Original = new SceneTree(TableAPI_, nullptr);
  Original->Name = "RestoreTestScene";

  Node* NodeA = Original->CreateNode("NodeA", NodeType::Node3D, nullptr);
  NodeA->SetPosition({10.0f, 20.0f, 30.0f});

  Node* Cam = Original->CreateNode("TestCam", NodeType::Camera, nullptr);
  static_cast<CameraNode*>(Cam)->SetFOV(60.0f);
  Cam->SetPosition({0.0f, 5.0f, 10.0f});

  // Take a snapshot
  std::string Snapshot = Parser_.SaveSceneToString(Original);
  ASSERT_FALSE(Snapshot.empty());

  // Simulate play-time modifications
  NodeA->SetPosition({99.0f, 99.0f, 99.0f});

  // Verify the modification took effect
  const Transform& ModifiedT = NodeA->GetTransform();
  EXPECT_NEAR(ModifiedT.Translation.X, 99.0f, 0.01f);

  // Delete the modified scene (simulates UnloadScene)
  delete Original;

  // Restore from snapshot (simulates RestoreSnapshot)
  SceneTree* Restored = Parser_.LoadSceneFromString(Snapshot.c_str());
  ASSERT_NE(Restored, nullptr);

  // Verify the restored scene has the original positions
  Node* RestoredNodeA = Restored->FindNode("NodeA");
  ASSERT_NE(RestoredNodeA, nullptr);
  const Transform& RestoredT = RestoredNodeA->GetTransform();
  EXPECT_NEAR(RestoredT.Translation.X, 10.0f, 0.01f);
  EXPECT_NEAR(RestoredT.Translation.Y, 20.0f, 0.01f);
  EXPECT_NEAR(RestoredT.Translation.Z, 30.0f, 0.01f);

  // Verify camera was restored
  Node* RestoredCam = Restored->FindNode("TestCam");
  ASSERT_NE(RestoredCam, nullptr);
  EXPECT_EQ(RestoredCam->GetType(), NodeType::Camera);
  CameraNode* RestoredCN = static_cast<CameraNode*>(RestoredCam);
  EXPECT_NEAR(RestoredCN->GetFOV(), 60.0f, 0.01f);

  delete Restored;
}

TEST_F(SnapshotRestoreTest, ScriptModificationsDuringPlayDiscardedOnRestore)
{
  // Build a scene with a node at a known position
  SceneTree* Original = new SceneTree(TableAPI_, nullptr);
  Original->Name = "ScriptModScene";

  Node* Target = Original->CreateNode("Target", NodeType::Node3D, nullptr);
  Target->SetPosition({1.0f, 2.0f, 3.0f});

  // Take a snapshot BEFORE any play-time modifications
  std::string Snapshot = Parser_.SaveSceneToString(Original);
  ASSERT_FALSE(Snapshot.empty());

  // Simulate what a script would do during play: modify the node's position
  // (In real EnterPlayMode, scripts would call OnUpdate which modifies nodes)
  auto* Script = new EditorModifyingScript();
  Target->AttachScript(Script);

  // Enable scripts and run an update cycle (simulates Play mode)
  Original->SetScriptsEnabled(true);
  Original->Update(0.016f);

  // Verify the script modified the position
  const Transform& PlayT = Target->GetTransform();
  EXPECT_NEAR(PlayT.Translation.X, 99.0f, 0.01f);
  EXPECT_NEAR(PlayT.Translation.Y, 99.0f, 0.01f);
  EXPECT_NEAR(PlayT.Translation.Z, 99.0f, 0.01f);

  // Delete the play-modified scene (simulates UnloadScene in ExitPlayMode)
  delete Original;

  // Restore from snapshot (simulates RestoreSnapshot in ExitPlayMode)
  SceneTree* Restored = Parser_.LoadSceneFromString(Snapshot.c_str());
  ASSERT_NE(Restored, nullptr);

  // Verify the restored scene has the ORIGINAL position, not the script-modified one
  Node* RestoredTarget = Restored->FindNode("Target");
  ASSERT_NE(RestoredTarget, nullptr);
  const Transform& RestoredT = RestoredTarget->GetTransform();
  EXPECT_NEAR(RestoredT.Translation.X, 1.0f, 0.01f);
  EXPECT_NEAR(RestoredT.Translation.Y, 2.0f, 0.01f);
  EXPECT_NEAR(RestoredT.Translation.Z, 3.0f, 0.01f);

  // Verify the restored node does NOT have a script (scripts are not serialized)
  EXPECT_EQ(RestoredTarget->GetScript(), nullptr);

  delete Restored;
}

TEST_F(SnapshotRestoreTest, SnapshotPreservesHierarchy)
{
  // Build a scene with hierarchy
  SceneTree* Original = new SceneTree(TableAPI_, nullptr);
  Original->Name = "HierarchySnapshot";

  Node* Parent = Original->CreateNode("Parent", NodeType::Node3D, nullptr);
  Node* Child  = Original->CreateNode("Child",  NodeType::MeshInstance, Parent);
  Parent->SetPosition({10.0f, 0.0f, 0.0f});
  Child->SetPosition({0.0f, 5.0f, 0.0f});
  static_cast<MeshInstance*>(Child)->SetMeshPath("models/test.glb");

  // Snapshot
  std::string Snapshot = Parser_.SaveSceneToString(Original);
  ASSERT_FALSE(Snapshot.empty());

  // Modify during "play"
  Parent->SetPosition({50.0f, 50.0f, 50.0f});
  delete Original;

  // Restore
  SceneTree* Restored = Parser_.LoadSceneFromString(Snapshot.c_str());
  ASSERT_NE(Restored, nullptr);

  Node* RestoredParent = Restored->FindNode("Parent");
  Node* RestoredChild = Restored->FindNode("Child");
  ASSERT_NE(RestoredParent, nullptr);
  ASSERT_NE(RestoredChild, nullptr);

  // Verify hierarchy preserved
  EXPECT_EQ(RestoredChild->GetParent(), RestoredParent);

  // Verify original positions restored
  EXPECT_NEAR(RestoredParent->GetTransform().Translation.X, 10.0f, 0.01f);
  EXPECT_NEAR(RestoredChild->GetTransform().Translation.Y, 5.0f, 0.01f);

  // Verify typed node data preserved
  EXPECT_EQ(RestoredChild->GetType(), NodeType::MeshInstance);
  EXPECT_EQ(static_cast<MeshInstance*>(RestoredChild)->GetMeshPath(), "models/test.glb");

  delete Restored;
}

//=============================================================================
// Task Group 4: Runtime Scene Management API surface tests
//
// These tests verify the AxEngineAPI struct has the new function pointers.
// Engine-level integration cannot be tested without DLL loading and GPU.
//=============================================================================

TEST(EngineAPISceneManagement, HasLoadSceneFunctionPointer)
{
  AxEngineAPI API{};
  EXPECT_EQ(API.LoadScene, nullptr);
}

TEST(EngineAPISceneManagement, HasUnloadSceneFunctionPointer)
{
  AxEngineAPI API{};
  EXPECT_EQ(API.UnloadScene, nullptr);
}

TEST(EngineAPISceneManagement, HasNewSceneFunctionPointer)
{
  AxEngineAPI API{};
  EXPECT_EQ(API.NewScene, nullptr);
}

TEST(EngineAPISceneManagement, HasSaveSceneFunctionPointer)
{
  AxEngineAPI API{};
  EXPECT_EQ(API.SaveScene, nullptr);
}

//=============================================================================
// Task Group 5: Scene Serialization (Save) tests
//
// These tests exercise SaveSceneToString and round-trip parsing using
// the SceneParser class directly.
//=============================================================================

class SceneSerializationTest : public testing::Test
{
protected:
  void SetUp() override
  {
    AxonInitGlobalAPIRegistry();
    AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);

    TableAPI_ = static_cast<AxHashTableAPI*>(
      AxonGlobalAPIRegistry->Get(AXON_HASH_TABLE_API_NAME));
    ASSERT_NE(TableAPI_, nullptr);

    AllocatorAPI_ = static_cast<AxAllocatorAPI*>(
      AxonGlobalAPIRegistry->Get(AXON_ALLOCATOR_API_NAME));
    ASSERT_NE(AllocatorAPI_, nullptr);

    Parser_.Init(AxonGlobalAPIRegistry);
  }

  void TearDown() override
  {
    Parser_.Term();
    AxonTermGlobalAPIRegistry();
  }

  AxHashTableAPI* TableAPI_{nullptr};
  AxAllocatorAPI* AllocatorAPI_{nullptr};
  SceneParser Parser_;
};

TEST_F(SceneSerializationTest, SaveEmptySceneToString)
{
  SceneTree* Tree = new SceneTree(TableAPI_, nullptr);
  Tree->Name = "EmptyScene";

  std::string Output = Parser_.SaveSceneToString(Tree);
  EXPECT_FALSE(Output.empty());

  // Should contain the scene header
  EXPECT_NE(Output.find("scene \"EmptyScene\""), std::string::npos);
  EXPECT_NE(Output.find("{"), std::string::npos);
  EXPECT_NE(Output.find("}"), std::string::npos);

  delete Tree;
}

TEST_F(SceneSerializationTest, SaveNullSceneReturnsEmpty)
{
  std::string Output = Parser_.SaveSceneToString(nullptr);
  EXPECT_TRUE(Output.empty());
}

TEST_F(SceneSerializationTest, SaveSceneWithLightNode)
{
  SceneTree* Tree = new SceneTree(TableAPI_, nullptr);
  Tree->Name = "LightScene";

  Node* Light = Tree->CreateNode("TestLight", NodeType::Light, nullptr);
  ASSERT_NE(Light, nullptr);

  LightNode* LN = static_cast<LightNode*>(Light);
  LN->SetLightType(AX_LIGHT_TYPE_POINT);
  LN->SetColor({1.0f, 0.8f, 0.6f});
  LN->SetIntensity(2.5f);
  LN->Light.Range = 100.0f;
  Light->SetPosition({10.0f, 20.0f, 30.0f});

  std::string Output = Parser_.SaveSceneToString(Tree);
  EXPECT_NE(Output.find("node \"TestLight\" Light"), std::string::npos);
  EXPECT_NE(Output.find("type: point"), std::string::npos);
  EXPECT_NE(Output.find("intensity:"), std::string::npos);
  EXPECT_NE(Output.find("range:"), std::string::npos);

  delete Tree;
}

TEST_F(SceneSerializationTest, SaveSceneWithCameraNode)
{
  SceneTree* Tree = new SceneTree(TableAPI_, nullptr);
  Tree->Name = "CameraScene";

  Node* Cam = Tree->CreateNode("TestCam", NodeType::Camera, nullptr);
  ASSERT_NE(Cam, nullptr);

  CameraNode* CN = static_cast<CameraNode*>(Cam);
  CN->SetFOV(75.0f);
  CN->SetNear(0.5f);
  CN->SetFar(500.0f);
  Cam->SetPosition({1.0f, 2.0f, 3.0f});

  std::string Output = Parser_.SaveSceneToString(Tree);
  EXPECT_NE(Output.find("node \"TestCam\" Camera"), std::string::npos);
  EXPECT_NE(Output.find("fov:"), std::string::npos);
  EXPECT_NE(Output.find("near:"), std::string::npos);
  EXPECT_NE(Output.find("far:"), std::string::npos);

  delete Tree;
}

TEST_F(SceneSerializationTest, SaveSceneWithMeshInstance)
{
  SceneTree* Tree = new SceneTree(TableAPI_, nullptr);
  Tree->Name = "MeshScene";

  Node* Mesh = Tree->CreateNode("TestMesh", NodeType::MeshInstance, nullptr);
  ASSERT_NE(Mesh, nullptr);

  MeshInstance* MI = static_cast<MeshInstance*>(Mesh);
  MI->SetMeshPath("models/test.glb");
  MI->SetMaterialName("TestMaterial");
  Mesh->SetPosition({5.0f, 0.0f, -3.0f});

  std::string Output = Parser_.SaveSceneToString(Tree);
  EXPECT_NE(Output.find("node \"TestMesh\" MeshInstance"), std::string::npos);
  EXPECT_NE(Output.find("mesh: \"models/test.glb\""), std::string::npos);
  EXPECT_NE(Output.find("material: \"TestMaterial\""), std::string::npos);

  delete Tree;
}

TEST_F(SceneSerializationTest, SaveSceneWithHierarchy)
{
  SceneTree* Tree = new SceneTree(TableAPI_, nullptr);
  Tree->Name = "HierarchyScene";

  Node* Parent = Tree->CreateNode("Parent", NodeType::Node3D, nullptr);
  Node* Child  = Tree->CreateNode("Child",  NodeType::Node3D, Parent);
  ASSERT_NE(Parent, nullptr);
  ASSERT_NE(Child, nullptr);

  Parent->SetPosition({10.0f, 0.0f, 0.0f});
  Child->SetPosition({0.0f, 5.0f, 0.0f});

  std::string Output = Parser_.SaveSceneToString(Tree);

  // Both parent and child should be in the output
  EXPECT_NE(Output.find("node \"Parent\" Node3D"), std::string::npos);
  EXPECT_NE(Output.find("node \"Child\" Node3D"), std::string::npos);

  delete Tree;
}

TEST_F(SceneSerializationTest, RoundTripLightNode)
{
  // Build a scene programmatically
  SceneTree* Original = new SceneTree(TableAPI_, nullptr);
  Original->Name = "RoundTripTest";

  Node* Light = Original->CreateNode("MainLight", NodeType::Light, nullptr);
  LightNode* LN = static_cast<LightNode*>(Light);
  LN->SetLightType(AX_LIGHT_TYPE_POINT);
  LN->SetColor({1.0f, 0.8f, 0.6f});
  LN->SetIntensity(1.5f);
  LN->Light.Range = 150.0f;
  Light->SetPosition({50.0f, 80.0f, 30.0f});

  // Serialize to string
  std::string Serialized = Parser_.SaveSceneToString(Original);
  ASSERT_FALSE(Serialized.empty());

  // Re-parse
  SceneTree* Restored = Parser_.LoadSceneFromString(Serialized.c_str());
  ASSERT_NE(Restored, nullptr);

  // Verify scene name
  EXPECT_EQ(Restored->Name, "RoundTripTest");

  // Find the light node
  Node* RestoredLight = Restored->FindNode("MainLight");
  ASSERT_NE(RestoredLight, nullptr);
  EXPECT_EQ(RestoredLight->GetType(), NodeType::Light);

  LightNode* RestoredLN = static_cast<LightNode*>(RestoredLight);
  EXPECT_EQ(RestoredLN->GetLightType(), AX_LIGHT_TYPE_POINT);
  EXPECT_NEAR(RestoredLN->Light.Color.X, 1.0f, 0.01f);
  EXPECT_NEAR(RestoredLN->Light.Color.Y, 0.8f, 0.01f);
  EXPECT_NEAR(RestoredLN->Light.Color.Z, 0.6f, 0.01f);
  EXPECT_NEAR(RestoredLN->GetIntensity(), 1.5f, 0.01f);
  EXPECT_NEAR(RestoredLN->Light.Range, 150.0f, 0.01f);

  // Verify transform
  const Transform& T = RestoredLight->GetTransform();
  EXPECT_NEAR(T.Translation.X, 50.0f, 0.01f);
  EXPECT_NEAR(T.Translation.Y, 80.0f, 0.01f);
  EXPECT_NEAR(T.Translation.Z, 30.0f, 0.01f);

  delete Original;
  delete Restored;
}

TEST_F(SceneSerializationTest, RoundTripCameraNode)
{
  SceneTree* Original = new SceneTree(TableAPI_, nullptr);
  Original->Name = "CamRoundTrip";

  Node* Cam = Original->CreateNode("TestCam", NodeType::Camera, nullptr);
  CameraNode* CN = static_cast<CameraNode*>(Cam);
  CN->SetFOV(75.0f);
  CN->SetNear(0.5f);
  CN->SetFar(500.0f);
  Cam->SetPosition({1.0f, 2.0f, 3.0f});
  Cam->SetRotation({-0.1f, 0.0f, 0.0f, 0.995f});

  std::string Serialized = Parser_.SaveSceneToString(Original);
  ASSERT_FALSE(Serialized.empty());

  SceneTree* Restored = Parser_.LoadSceneFromString(Serialized.c_str());
  ASSERT_NE(Restored, nullptr);

  Node* RestoredCam = Restored->FindNode("TestCam");
  ASSERT_NE(RestoredCam, nullptr);
  EXPECT_EQ(RestoredCam->GetType(), NodeType::Camera);

  CameraNode* RestoredCN = static_cast<CameraNode*>(RestoredCam);
  EXPECT_NEAR(RestoredCN->GetFOV(), 75.0f, 0.01f);
  EXPECT_NEAR(RestoredCN->GetNear(), 0.5f, 0.01f);
  EXPECT_NEAR(RestoredCN->GetFar(), 500.0f, 0.01f);

  const Transform& T = RestoredCam->GetTransform();
  EXPECT_NEAR(T.Translation.X, 1.0f, 0.01f);
  EXPECT_NEAR(T.Translation.Y, 2.0f, 0.01f);
  EXPECT_NEAR(T.Translation.Z, 3.0f, 0.01f);
  EXPECT_NEAR(T.Rotation.X, -0.1f, 0.01f);
  EXPECT_NEAR(T.Rotation.W, 0.995f, 0.01f);

  delete Original;
  delete Restored;
}

TEST_F(SceneSerializationTest, RoundTripMeshInstance)
{
  SceneTree* Original = new SceneTree(TableAPI_, nullptr);
  Original->Name = "MeshRoundTrip";

  Node* Mesh = Original->CreateNode("Sponza", NodeType::MeshInstance, nullptr);
  MeshInstance* MI = static_cast<MeshInstance*>(Mesh);
  MI->SetMeshPath("models/sponza.glb");
  MI->SetMaterialName("DefaultMat");
  Mesh->SetPosition({0.0f, 0.0f, 0.0f});
  Mesh->SetScale({2.0f, 2.0f, 2.0f});

  std::string Serialized = Parser_.SaveSceneToString(Original);
  ASSERT_FALSE(Serialized.empty());

  SceneTree* Restored = Parser_.LoadSceneFromString(Serialized.c_str());
  ASSERT_NE(Restored, nullptr);

  Node* RestoredMesh = Restored->FindNode("Sponza");
  ASSERT_NE(RestoredMesh, nullptr);
  EXPECT_EQ(RestoredMesh->GetType(), NodeType::MeshInstance);

  MeshInstance* RestoredMI = static_cast<MeshInstance*>(RestoredMesh);
  EXPECT_EQ(RestoredMI->GetMeshPath(), "models/sponza.glb");
  EXPECT_EQ(RestoredMI->GetMaterialName(), "DefaultMat");

  const Transform& T = RestoredMesh->GetTransform();
  EXPECT_NEAR(T.Scale.X, 2.0f, 0.01f);
  EXPECT_NEAR(T.Scale.Y, 2.0f, 0.01f);
  EXPECT_NEAR(T.Scale.Z, 2.0f, 0.01f);

  delete Original;
  delete Restored;
}

TEST_F(SceneSerializationTest, RoundTripHierarchy)
{
  SceneTree* Original = new SceneTree(TableAPI_, nullptr);
  Original->Name = "HierarchyRoundTrip";

  Node* Parent = Original->CreateNode("Parent", NodeType::Node3D, nullptr);
  Node* ChildA = Original->CreateNode("ChildA", NodeType::Node3D, Parent);
  Node* ChildB = Original->CreateNode("ChildB", NodeType::Light, Parent);

  Parent->SetPosition({10.0f, 0.0f, 0.0f});
  ChildA->SetPosition({0.0f, 5.0f, 0.0f});
  ChildB->SetPosition({0.0f, 0.0f, 3.0f});

  LightNode* LN = static_cast<LightNode*>(ChildB);
  LN->SetLightType(AX_LIGHT_TYPE_DIRECTIONAL);
  LN->SetIntensity(0.7f);

  std::string Serialized = Parser_.SaveSceneToString(Original);
  ASSERT_FALSE(Serialized.empty());

  SceneTree* Restored = Parser_.LoadSceneFromString(Serialized.c_str());
  ASSERT_NE(Restored, nullptr);

  // Verify hierarchy
  Node* RestoredParent = Restored->FindNode("Parent");
  ASSERT_NE(RestoredParent, nullptr);

  Node* RestoredChildA = Restored->FindNode("ChildA");
  ASSERT_NE(RestoredChildA, nullptr);
  EXPECT_EQ(RestoredChildA->GetParent(), RestoredParent);

  Node* RestoredChildB = Restored->FindNode("ChildB");
  ASSERT_NE(RestoredChildB, nullptr);
  EXPECT_EQ(RestoredChildB->GetParent(), RestoredParent);
  EXPECT_EQ(RestoredChildB->GetType(), NodeType::Light);

  LightNode* RestoredLN = static_cast<LightNode*>(RestoredChildB);
  EXPECT_EQ(RestoredLN->GetLightType(), AX_LIGHT_TYPE_DIRECTIONAL);
  EXPECT_NEAR(RestoredLN->GetIntensity(), 0.7f, 0.01f);

  // Verify positions
  const Transform& PT = RestoredParent->GetTransform();
  EXPECT_NEAR(PT.Translation.X, 10.0f, 0.01f);

  const Transform& CAT = RestoredChildA->GetTransform();
  EXPECT_NEAR(CAT.Translation.Y, 5.0f, 0.01f);

  const Transform& CBT = RestoredChildB->GetTransform();
  EXPECT_NEAR(CBT.Translation.Z, 3.0f, 0.01f);

  delete Original;
  delete Restored;
}

TEST_F(SceneSerializationTest, RoundTripMultipleNodeTypes)
{
  // Create a scene with multiple node types and verify round-trip
  SceneTree* Original = new SceneTree(TableAPI_, nullptr);
  Original->Name = "MultiTypeScene";

  Node* Light = Original->CreateNode("Light1", NodeType::Light, nullptr);
  static_cast<LightNode*>(Light)->SetIntensity(2.0f);
  Light->SetPosition({0.0f, 10.0f, 0.0f});

  Node* Cam = Original->CreateNode("Cam1", NodeType::Camera, nullptr);
  static_cast<CameraNode*>(Cam)->SetFOV(90.0f);
  Cam->SetPosition({5.0f, 1.7f, 0.0f});

  Node* Mesh = Original->CreateNode("Floor", NodeType::MeshInstance, nullptr);
  static_cast<MeshInstance*>(Mesh)->SetMeshPath("models/floor.glb");

  std::string Serialized = Parser_.SaveSceneToString(Original);
  ASSERT_FALSE(Serialized.empty());

  SceneTree* Restored = Parser_.LoadSceneFromString(Serialized.c_str());
  ASSERT_NE(Restored, nullptr);

  // Count typed nodes
  uint32_t LightCount = 0;
  uint32_t CamCount = 0;
  uint32_t MeshCount = 0;
  Restored->GetNodesByType(NodeType::Light, &LightCount);
  Restored->GetNodesByType(NodeType::Camera, &CamCount);
  Restored->GetNodesByType(NodeType::MeshInstance, &MeshCount);

  EXPECT_EQ(LightCount, 1u);
  EXPECT_EQ(CamCount, 1u);
  EXPECT_EQ(MeshCount, 1u);

  // Verify scene name preserved
  EXPECT_EQ(Restored->Name, "MultiTypeScene");

  delete Original;
  delete Restored;
}

//=============================================================================
// Task Group 6: Editor Camera
//
// These tests verify the EditorCameraState struct defaults and API surface.
// AxEngine and AxRenderer integration cannot be tested in the unit test
// binary since they require DLL loading and GPU resources.
//=============================================================================

TEST(EditorCameraStateTest, DefaultValues)
{
  EditorCameraState State{};
  EXPECT_NEAR(State.Position.X, 0.0f, 0.001f);
  EXPECT_NEAR(State.Position.Y, 5.0f, 0.001f);
  EXPECT_NEAR(State.Position.Z, 10.0f, 0.001f);
  EXPECT_NEAR(State.Target.X, 0.0f, 0.001f);
  EXPECT_NEAR(State.Target.Y, 0.0f, 0.001f);
  EXPECT_NEAR(State.Target.Z, 0.0f, 0.001f);
  EXPECT_NEAR(State.FOV, 60.0f, 0.001f);
  EXPECT_NEAR(State.Near, 0.1f, 0.001f);
  EXPECT_NEAR(State.Far, 1000.0f, 0.001f);
}

TEST(EditorCameraStateTest, CanBeModified)
{
  EditorCameraState State{};
  State.Position = {1.0f, 2.0f, 3.0f};
  State.Target = {4.0f, 5.0f, 6.0f};
  State.FOV = 90.0f;
  State.Near = 0.5f;
  State.Far = 500.0f;

  EXPECT_NEAR(State.Position.X, 1.0f, 0.001f);
  EXPECT_NEAR(State.Target.Y, 5.0f, 0.001f);
  EXPECT_NEAR(State.FOV, 90.0f, 0.001f);
  EXPECT_NEAR(State.Near, 0.5f, 0.001f);
  EXPECT_NEAR(State.Far, 500.0f, 0.001f);
}

TEST(EngineAPIEditorCamera, HasSetEditorCameraFunctionPointer)
{
  AxEngineAPI API{};
  EXPECT_EQ(API.SetEditorCamera, nullptr);
}

TEST(EngineAPIEditorCamera, AllPlayModeAndCameraPointersPresent)
{
  // Comprehensive check that all new function pointers from Task Groups 3 and 6
  // are present alongside existing ones
  AxEngineAPI API{};
  EXPECT_EQ(API.EnterPlayMode, nullptr);
  EXPECT_EQ(API.ExitPlayMode, nullptr);
  EXPECT_EQ(API.SetEditorCamera, nullptr);
}
