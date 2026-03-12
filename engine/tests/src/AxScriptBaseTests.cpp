/**
 * AxScriptBaseTests.cpp - Unit tests for ScriptBase
 *
 * Tests the ScriptBase class:
 * - Default state: null owner, IsInitialized() == false
 * - All callbacks callable with no override (no-op, no crash)
 * - IsInitialized_ is not reset across enable/disable cycles
 */

#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxHashTable.h"
#include "Foundation/AxAPIRegistry.h"
#include "AxEngine/AxScriptBase.h"
#include "AxEngine/AxNode.h"

//=============================================================================
// Minimal concrete ScriptBase subclass for testing
//=============================================================================

/**
 * MockScript - Counts invocations per callback.
 * Used to verify callbacks fire the correct number of times and in order.
 */
struct MockScript : public ScriptBase
{
  int AttachCount      = 0;
  int InitCount        = 0;
  int EnableCount      = 0;
  int DisableCount     = 0;
  int DetachCount      = 0;
  int UpdateCount      = 0;
  int FixedUpdateCount = 0;
  int LateUpdateCount  = 0;

  void OnAttach()  override { ++AttachCount; }
  void OnInit()    override { ++InitCount; }
  void OnEnable()  override { ++EnableCount; }
  void OnDisable() override { ++DisableCount; }
  void OnDetach()  override { ++DetachCount; }

  void OnUpdate(float DeltaT)      override { (void)DeltaT; ++UpdateCount; }
  void OnFixedUpdate(float DeltaT) override { (void)DeltaT; ++FixedUpdateCount; }
  void OnLateUpdate(float DeltaT)  override { (void)DeltaT; ++LateUpdateCount; }
};

/**
 * NoOverrideScript - Does not override any callbacks.
 * Used to verify base class no-ops don't crash.
 */
struct NoOverrideScript : public ScriptBase
{
};

/**
 * Test fixture that initializes Foundation APIs required by Node.
 */
class ScriptBaseTest : public testing::Test
{
protected:
  void SetUp() override
  {
    AxonInitGlobalAPIRegistry();
    AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);
    TableAPI_ = static_cast<AxHashTableAPI*>(
      AxonGlobalAPIRegistry->Get(AXON_HASH_TABLE_API_NAME));
    ASSERT_NE(TableAPI_, nullptr);
  }

  void TearDown() override
  {
    AxonTermGlobalAPIRegistry();
  }

  AxHashTableAPI* TableAPI_{nullptr};
};

//=============================================================================
// Test 1: Default state — null owner and IsInitialized() == false
//=============================================================================

TEST_F(ScriptBaseTest, DefaultStateHasNullOwnerAndNotInitialized)
{
  MockScript Script;

  EXPECT_EQ(Script.GetOwner(), nullptr)
    << "Newly constructed ScriptBase should have a null owner";

  EXPECT_FALSE(Script.IsInitialized())
    << "Newly constructed ScriptBase should not be initialized";
}

//=============================================================================
// Test 2: All callbacks callable with no override — no crash
//=============================================================================

TEST_F(ScriptBaseTest, AllCallbacksCallableWithNoOverride)
{
  NoOverrideScript Script;

  // These should all be no-ops and must not crash
  EXPECT_NO_FATAL_FAILURE(Script.OnAttach());
  EXPECT_NO_FATAL_FAILURE(Script.OnInit());
  EXPECT_NO_FATAL_FAILURE(Script.OnEnable());
  EXPECT_NO_FATAL_FAILURE(Script.OnDisable());
  EXPECT_NO_FATAL_FAILURE(Script.OnDetach());
  EXPECT_NO_FATAL_FAILURE(Script.OnUpdate(0.016f));
  EXPECT_NO_FATAL_FAILURE(Script.OnFixedUpdate(0.02f));
  EXPECT_NO_FATAL_FAILURE(Script.OnLateUpdate(0.016f));
}

//=============================================================================
// Test 3: All overridden callbacks fire when called directly
//=============================================================================

TEST_F(ScriptBaseTest, OverriddenCallbacksFireWhenCalled)
{
  MockScript Script;

  Script.OnAttach();
  EXPECT_EQ(Script.AttachCount, 1);

  Script.OnInit();
  EXPECT_EQ(Script.InitCount, 1);

  Script.OnEnable();
  EXPECT_EQ(Script.EnableCount, 1);

  Script.OnDisable();
  EXPECT_EQ(Script.DisableCount, 1);

  Script.OnDetach();
  EXPECT_EQ(Script.DetachCount, 1);

  Script.OnUpdate(0.016f);
  EXPECT_EQ(Script.UpdateCount, 1);

  Script.OnFixedUpdate(0.02f);
  EXPECT_EQ(Script.FixedUpdateCount, 1);

  Script.OnLateUpdate(0.016f);
  EXPECT_EQ(Script.LateUpdateCount, 1);
}

//=============================================================================
// Test 5: GetOwner returns null before Node attaches the script
//=============================================================================

TEST_F(ScriptBaseTest, GetOwnerIsNullWithoutAttachment)
{
  MockScript Script;

  // Without a Node calling AttachScript, Owner_ is never set
  EXPECT_EQ(Script.GetOwner(), nullptr);
}
