/**
 * AxScriptLogTests.cpp - Tests for Script Log Buffer
 *
 * Tests LogBuffer ring buffer, Log:: namespace integration,
 * ScriptBase log methods, and engine failure path warnings.
 */

#include "gtest/gtest.h"
#include "AxEngine/AxScriptLog.h"
#include "Foundation/AxHashTable.h"
#include "Foundation/AxAPIRegistry.h"
#include "AxEngine/AxNode.h"
#include "AxEngine/AxSceneTree.h"
#include "AxEngine/AxScriptBase.h"

//=============================================================================
// LogBuffer Tests
//=============================================================================

class LogBufferTest : public testing::Test
{
protected:
  void SetUp() override
  {
    LogBuffer::Get().Clear();
  }
};

TEST_F(LogBufferTest, InitialState_Empty)
{
  EXPECT_EQ(LogBuffer::Get().GetCount(), 0u);
}

TEST_F(LogBufferTest, Push_SingleEntry_CountIsOne)
{
  LogBuffer::Get().Push(AX_LOG_LEVEL_WARNING, "TestNode", "hello");
  EXPECT_EQ(LogBuffer::Get().GetCount(), 1u);
}

TEST_F(LogBufferTest, Push_EntryFields_Correct)
{
  LogBuffer::Get().Push(AX_LOG_LEVEL_ERROR, "PlayerNode", "health below zero");

  const LogEntry& E = LogBuffer::Get().GetEntry(0);
  EXPECT_EQ(E.Level, AX_LOG_LEVEL_ERROR);
  EXPECT_EQ(E.NodeName, "PlayerNode");
  EXPECT_EQ(E.Message, "health below zero");
}

TEST_F(LogBufferTest, Push_MultipleEntries_PreservesOrder)
{
  LogBuffer::Get().Push(AX_LOG_LEVEL_INFO, "", "first");
  LogBuffer::Get().Push(AX_LOG_LEVEL_WARNING, "", "second");
  LogBuffer::Get().Push(AX_LOG_LEVEL_ERROR, "", "third");

  EXPECT_EQ(LogBuffer::Get().GetCount(), 3u);
  EXPECT_EQ(LogBuffer::Get().GetEntry(0).Message, "first");
  EXPECT_EQ(LogBuffer::Get().GetEntry(1).Message, "second");
  EXPECT_EQ(LogBuffer::Get().GetEntry(2).Message, "third");
}

TEST_F(LogBufferTest, Push_EmptyNodeName_Allowed)
{
  LogBuffer::Get().Push(AX_LOG_LEVEL_WARNING, "", "engine warning");
  EXPECT_TRUE(LogBuffer::Get().GetEntry(0).NodeName.empty());
}

TEST_F(LogBufferTest, Clear_ResetsCount)
{
  LogBuffer::Get().Push(AX_LOG_LEVEL_INFO, "", "msg");
  LogBuffer::Get().Push(AX_LOG_LEVEL_INFO, "", "msg2");
  LogBuffer::Get().Clear();
  EXPECT_EQ(LogBuffer::Get().GetCount(), 0u);
}

TEST_F(LogBufferTest, RingOverflow_OldestOverwritten)
{
  // Fill the buffer past capacity
  for (uint32_t I = 0; I < LogBuffer::kMaxEntries + 5; I++) {
    LogBuffer::Get().Push(AX_LOG_LEVEL_INFO, "", "msg" + std::to_string(I));
  }

  // Count should be capped at max
  EXPECT_EQ(LogBuffer::Get().GetCount(), LogBuffer::kMaxEntries);

  // Oldest entry should be entry #5 (first 5 were overwritten)
  EXPECT_EQ(LogBuffer::Get().GetEntry(0).Message, "msg5");

  // Newest entry should be the last one pushed
  EXPECT_EQ(LogBuffer::Get().GetEntry(LogBuffer::kMaxEntries - 1).Message,
            "msg" + std::to_string(LogBuffer::kMaxEntries + 4));
}

//=============================================================================
// Log:: Namespace -> LogBuffer Integration
//=============================================================================

TEST_F(LogBufferTest, LogWarn_PushesEntryToBuffer)
{
  Log::Warn("test warning");
  ASSERT_EQ(LogBuffer::Get().GetCount(), 1u);
  EXPECT_EQ(LogBuffer::Get().GetEntry(0).Level, AX_LOG_LEVEL_WARNING);
  EXPECT_EQ(LogBuffer::Get().GetEntry(0).Message, "test warning");
  EXPECT_TRUE(LogBuffer::Get().GetEntry(0).NodeName.empty());
}

TEST_F(LogBufferTest, LogError_PushesEntryToBuffer)
{
  Log::Error("test error");
  ASSERT_EQ(LogBuffer::Get().GetCount(), 1u);
  EXPECT_EQ(LogBuffer::Get().GetEntry(0).Level, AX_LOG_LEVEL_ERROR);
}

TEST_F(LogBufferTest, LogInfo_PushesEntryToBuffer)
{
  Log::Info("test info");
  ASSERT_EQ(LogBuffer::Get().GetCount(), 1u);
  EXPECT_EQ(LogBuffer::Get().GetEntry(0).Level, AX_LOG_LEVEL_INFO);
}

TEST_F(LogBufferTest, LogDebug_PushesEntryToBuffer)
{
  Log::Debug("test debug");
  ASSERT_EQ(LogBuffer::Get().GetCount(), 1u);
  EXPECT_EQ(LogBuffer::Get().GetEntry(0).Level, AX_LOG_LEVEL_DEBUG);
}

//=============================================================================
// ScriptBase Log Methods
//=============================================================================

struct LogTestScript : public ScriptBase
{
  void OnInit() override
  {
    LogWarn("init warning");
    LogError("init error");
  }
};

class ScriptLogTest : public testing::Test
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
    LogBuffer::Get().Clear();
  }

  void TearDown() override
  {
    delete Tree_;
    AxonTermGlobalAPIRegistry();
  }

  AxHashTableAPI* TableAPI_{nullptr};
  SceneTree* Tree_{nullptr};
};

TEST_F(ScriptLogTest, ScriptLogWarn_IncludesNodeName)
{
  Node* A = Tree_->CreateNode("PlayerNode", NodeType::Node3D);
  auto* Script = new LogTestScript();
  A->AttachScript(Script);
  Tree_->Update(0.016f);

  // Find the warning entry
  bool Found = false;
  for (uint32_t I = 0; I < LogBuffer::Get().GetCount(); I++) {
    const LogEntry& E = LogBuffer::Get().GetEntry(I);
    if (E.Message == "init warning" && E.Level == AX_LOG_LEVEL_WARNING) {
      EXPECT_EQ(E.NodeName, "PlayerNode");
      Found = true;
      break;
    }
  }
  EXPECT_TRUE(Found);
}

TEST_F(ScriptLogTest, ScriptLogError_IncludesNodeName)
{
  Node* A = Tree_->CreateNode("EnemyNode", NodeType::Node3D);
  auto* Script = new LogTestScript();
  A->AttachScript(Script);
  Tree_->Update(0.016f);

  bool Found = false;
  for (uint32_t I = 0; I < LogBuffer::Get().GetCount(); I++) {
    const LogEntry& E = LogBuffer::Get().GetEntry(I);
    if (E.Message == "init error" && E.Level == AX_LOG_LEVEL_ERROR) {
      EXPECT_EQ(E.NodeName, "EnemyNode");
      Found = true;
      break;
    }
  }
  EXPECT_TRUE(Found);
}

//=============================================================================
// Engine Failure Path Warnings -> LogBuffer
//=============================================================================

TEST_F(ScriptLogTest, CreateNode_EmptyName_PushesWarning)
{
  LogBuffer::Get().Clear();
  Node* N = Tree_->CreateNode("", NodeType::Node3D);
  EXPECT_EQ(N, nullptr);
  ASSERT_GE(LogBuffer::Get().GetCount(), 1u);
  EXPECT_EQ(LogBuffer::Get().GetEntry(0).Level, AX_LOG_LEVEL_WARNING);
}

TEST_F(ScriptLogTest, GetNode_NotFound_PushesWarning)
{
  Node* A = Tree_->CreateNode("A", NodeType::Node3D);
  LogBuffer::Get().Clear();

  Node* Result = A->GetNode("DoesNotExist");
  EXPECT_EQ(Result, nullptr);
  ASSERT_GE(LogBuffer::Get().GetCount(), 1u);
  EXPECT_EQ(LogBuffer::Get().GetEntry(0).Level, AX_LOG_LEVEL_WARNING);
}
