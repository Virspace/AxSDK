/**
 * AxSignalTests.cpp - Tests for Node-Scoped Signal System
 *
 * Tests SignalArgs, signal emission, connection, disconnection,
 * automatic cleanup, and ScriptBase convenience wrappers.
 */

#include "gtest/gtest.h"
#include "AxEngine/AxSignal.h"
#include "AxEngine/AxNode.h"
#include "AxEngine/AxSceneTree.h"
#include "AxEngine/AxScriptBase.h"
#include "Foundation/AxHashTable.h"
#include "Foundation/AxAPIRegistry.h"

//=============================================================================
// Task 1: SignalArgs Tests
//=============================================================================

TEST(SignalArgsTest, GetFloat_ReturnsCorrectValue)
{
  SignalArgs A;
  A.Args[0].ArgType = SignalArg::Type::Float;
  A.Args[0].AsFloat = 3.14f;
  A.Count = 1;
  EXPECT_FLOAT_EQ(A.Get<float>(0), 3.14f);
}

TEST(SignalArgsTest, GetInt_ReturnsCorrectValue)
{
  SignalArgs A;
  A.Args[0].ArgType = SignalArg::Type::Int;
  A.Args[0].AsInt = 42;
  A.Count = 1;
  EXPECT_EQ(A.Get<int32_t>(0), 42);
}

TEST(SignalArgsTest, GetBool_ReturnsCorrectValue)
{
  SignalArgs A;
  A.Args[0].ArgType = SignalArg::Type::Bool;
  A.Args[0].AsBool = true;
  A.Count = 1;
  EXPECT_TRUE(A.Get<bool>(0));
}

TEST(SignalArgsTest, GetStringView_ReturnsCorrectValue)
{
  SignalArgs A;
  A.Args[0].ArgType = SignalArg::Type::String;
  A.Args[0].AsString = "hello";
  A.Count = 1;
  EXPECT_EQ(A.Get<std::string_view>(0), "hello");
}

TEST(SignalArgsTest, GetPointer_ReturnsCorrectValue)
{
  int Dummy = 99;
  SignalArgs A;
  A.Args[0].ArgType = SignalArg::Type::Pointer;
  A.Args[0].AsPointer = &Dummy;
  A.Count = 1;
  EXPECT_EQ(A.Get<void*>(0), &Dummy);
}

TEST(SignalArgsTest, IndexOutOfBounds_ReturnsDefault)
{
  SignalArgs A;
  A.Count = 0;
  EXPECT_FLOAT_EQ(A.Get<float>(0), 0.0f);
  EXPECT_EQ(A.Get<int32_t>(0), 0);
  EXPECT_FALSE(A.Get<bool>(0));
  EXPECT_EQ(A.Get<std::string_view>(0), "");
  EXPECT_EQ(A.Get<void*>(0), nullptr);
}

//=============================================================================
// Test Fixture — creates a SceneTree for Node tests
//=============================================================================

class SignalTest : public testing::Test
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
// Task 2: Signal Emission Tests
//=============================================================================

TEST_F(SignalTest, EmitSignal_NoSubscribers_NoOp)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  A->EmitSignal("something"); // should not crash
}

TEST_F(SignalTest, EmitSignal_OneSubscriber_CallbackFires)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  bool Fired = false;
  A->Connect("test", [&](const SignalArgs&) { Fired = true; });
  A->EmitSignal("test");
  EXPECT_TRUE(Fired);
}

TEST_F(SignalTest, EmitSignal_MultipleSubscribers_AllFire)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  int Count = 0;
  A->Connect("test", [&](const SignalArgs&) { Count++; });
  A->Connect("test", [&](const SignalArgs&) { Count++; });
  A->Connect("test", [&](const SignalArgs&) { Count++; });
  A->EmitSignal("test");
  EXPECT_EQ(Count, 3);
}

TEST_F(SignalTest, EmitSignal_WithFloatArg_CallbackReceivesValue)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  float Received = 0.0f;
  A->Connect("hp_changed", [&](const SignalArgs& Args) {
    Received = Args.Get<float>(0);
  });
  A->EmitSignal("hp_changed", 75.5f);
  EXPECT_FLOAT_EQ(Received, 75.5f);
}

TEST_F(SignalTest, EmitSignal_WithMultipleArgs_AllArgsAccessible)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  float F = 0.0f;
  int32_t I = 0;
  A->Connect("multi", [&](const SignalArgs& Args) {
    F = Args.Get<float>(0);
    I = Args.Get<int32_t>(1);
  });

  SignalArgs Args;
  Args.Args[0].ArgType = SignalArg::Type::Float;
  Args.Args[0].AsFloat = 1.5f;
  Args.Args[1].ArgType = SignalArg::Type::Int;
  Args.Args[1].AsInt = 10;
  Args.Count = 2;
  A->EmitSignalArgs("multi", Args);

  EXPECT_FLOAT_EQ(F, 1.5f);
  EXPECT_EQ(I, 10);
}

TEST_F(SignalTest, EmitSignal_UnknownSignal_NoOp)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  bool Fired = false;
  A->Connect("known", [&](const SignalArgs&) { Fired = true; });
  A->EmitSignal("unknown");
  EXPECT_FALSE(Fired);
}

//=============================================================================
// Task 2: Signal Connection Tests
//=============================================================================

TEST_F(SignalTest, Connect_ReturnsUniqueID)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  uint32_t ID1 = A->Connect("test", [](const SignalArgs&) {});
  uint32_t ID2 = A->Connect("test", [](const SignalArgs&) {});
  EXPECT_NE(ID1, ID2);
  EXPECT_NE(ID1, 0u);
  EXPECT_NE(ID2, 0u);
}

TEST_F(SignalTest, Connect_ImplicitSignalCreation)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  // Connecting to a signal that has never been emitted should work
  bool Fired = false;
  A->Connect("new_signal", [&](const SignalArgs&) { Fired = true; });
  A->EmitSignal("new_signal");
  EXPECT_TRUE(Fired);
}

TEST_F(SignalTest, Connect_LambdaCapture_Works)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  std::string Captured = "initial";
  A->Connect("test", [&Captured](const SignalArgs&) { Captured = "changed"; });
  A->EmitSignal("test");
  EXPECT_EQ(Captured, "changed");
}

TEST_F(SignalTest, Connect_MultipleSignals_Independent)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  bool AFired = false, BFired = false;
  A->Connect("a", [&](const SignalArgs&) { AFired = true; });
  A->Connect("b", [&](const SignalArgs&) { BFired = true; });

  A->EmitSignal("a");
  EXPECT_TRUE(AFired);
  EXPECT_FALSE(BFired);
}

//=============================================================================
// Task 3: Signal Disconnection Tests
//=============================================================================

TEST_F(SignalTest, Disconnect_ValidID_StopsCallback)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  bool Fired = false;
  uint32_t ID = A->Connect("test", [&](const SignalArgs&) { Fired = true; });
  A->Disconnect("test", ID);
  A->EmitSignal("test");
  EXPECT_FALSE(Fired);
}

TEST_F(SignalTest, Disconnect_InvalidID_NoOp)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  A->Connect("test", [](const SignalArgs&) {});
  A->Disconnect("test", 9999); // should not crash
}

TEST_F(SignalTest, Disconnect_DuringEmission_Safe)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  uint32_t ID = 0;
  int Count = 0;

  ID = A->Connect("test", [&](const SignalArgs&) {
    Count++;
    A->Disconnect("test", ID);
  });
  A->Connect("test", [&](const SignalArgs&) { Count++; });

  A->EmitSignal("test");
  // First callback fires and disconnects itself. Second still fires.
  EXPECT_GE(Count, 2);

  // Emit again — first should no longer fire
  Count = 0;
  A->EmitSignal("test");
  EXPECT_EQ(Count, 1);
}

//=============================================================================
// Task 4: Automatic Cleanup Tests
//=============================================================================

TEST_F(SignalTest, DestroyEmitter_CleansUpReceiverOutgoingList)
{
  Node* Emitter = Tree_->CreateNode("Emitter", NodeType::Node3D);
  Node* Receiver = Tree_->CreateNode("Receiver", NodeType::Node3D);

  Emitter->Connect("died", [](const SignalArgs&) {}, Receiver);

  // Destroy emitter — receiver's outgoing list should be cleaned
  Tree_->DestroyNode(Emitter);

  // Receiver should still be valid and its outgoing connections cleared
  // (Verified by not crashing when Receiver is later destroyed)
}

TEST_F(SignalTest, DestroyReceiver_DisconnectsFromEmitter)
{
  Node* Emitter = Tree_->CreateNode("Emitter", NodeType::Node3D);
  Node* Receiver = Tree_->CreateNode("Receiver", NodeType::Node3D);

  bool Fired = false;
  Emitter->Connect("test", [&](const SignalArgs&) { Fired = true; }, Receiver);

  // Destroy receiver — its callback should be removed from emitter
  Tree_->DestroyNode(Receiver);

  Emitter->EmitSignal("test");
  EXPECT_FALSE(Fired);
}

TEST_F(SignalTest, DestroyBothNodes_NoDoubleFree)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  Node* B = Tree_->CreateNode("B", NodeType::Node3D);

  A->Connect("test", [](const SignalArgs&) {}, B);
  B->Connect("test", [](const SignalArgs&) {}, A);

  // Destroy both in either order — should not crash
  Tree_->DestroyNode(A);
  Tree_->DestroyNode(B);
}

//=============================================================================
// Task 5: ScriptBase Convenience and Integration Tests
//=============================================================================

struct EmitterScript : public ScriptBase
{
  void OnInit() override
  {
    EmitSignal("ready");
    EmitSignal("hp_changed", 100.0f);
  }
};

struct ReceiverScript : public ScriptBase
{
  bool ReadyReceived{false};
  float HPReceived{0.0f};

  void OnInit() override
  {
    Node* Emitter = GetNode("../Emitter");
    if (Emitter) {
      Connect(Emitter, "ready", [this](const SignalArgs&) {
        ReadyReceived = true;
      });
      Connect(Emitter, "hp_changed", [this](const SignalArgs& Args) {
        HPReceived = Args.Get<float>(0);
      });
    }
  }
};

TEST_F(SignalTest, Script_EmitAndConnect_EndToEnd)
{
  Node* E = Tree_->CreateNode("Emitter", NodeType::Node3D);
  Node* R = Tree_->CreateNode("Receiver", NodeType::Node3D);

  auto* RScript = new ReceiverScript();
  R->AttachScript(RScript);

  auto* EScript = new EmitterScript();
  E->AttachScript(EScript);

  // First update initializes scripts (bottom-up: Receiver first, then Emitter)
  Tree_->Update(0.016f);

  EXPECT_TRUE(RScript->ReadyReceived);
  EXPECT_FLOAT_EQ(RScript->HPReceived, 100.0f);
}

TEST_F(SignalTest, Script_ConnectViaGetNode_RealisticPattern)
{
  // Create hierarchy: Root -> Player, Root -> HUD
  Node* Player = Tree_->CreateNode("Player", NodeType::Node3D);
  Node* HUD = Tree_->CreateNode("HUD", NodeType::Node3D);

  bool Died = false;

  struct HUDScript : public ScriptBase
  {
    bool* DiedPtr;
    HUDScript(bool* D) : DiedPtr(D) {}

    void OnInit() override
    {
      Node* P = GetNode("../Player");
      if (P) {
        Connect(P, "died", [this](const SignalArgs&) {
          *DiedPtr = true;
        });
      }
    }
  };

  auto* Script = new HUDScript(&Died);
  HUD->AttachScript(Script);
  Tree_->Update(0.016f);

  // Now emit from Player
  Player->EmitSignal("died");
  EXPECT_TRUE(Died);
}
