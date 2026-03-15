/**
 * AxInputActionTests.cpp - Tests for Input Action Mapping
 *
 * Tests MapAction, UnmapAction, IsActionDown/Pressed/Released,
 * MapAxis, UnmapAxis, GetAxis(string_view), and integration.
 */

#include "gtest/gtest.h"
#include "AxEngine/AxInput.h"
#include "AxWindow/AxWindow.h"

#include <cstring>

//=============================================================================
// Test Fixture — manipulates AxInput key state directly
//=============================================================================

class InputActionTest : public testing::Test
{
protected:
  void SetUp() override
  {
    Input_ = &AxInput::Get();

    // Access internal state for test simulation
    Current_ = const_cast<AxInputState*>(Input_->GetCurrentState());
    Prev_ = const_cast<AxInputState*>(Input_->GetPreviousState());
    ASSERT_NE(Current_, nullptr);
    ASSERT_NE(Prev_, nullptr);

    // Clear all key state
    memset(Current_->Keys, 0, sizeof(Current_->Keys));
    memset(Prev_->Keys, 0, sizeof(Prev_->Keys));

    // Clear any mappings from previous tests
    Input_->UnmapAction("jump");
    Input_->UnmapAction("attack");
    Input_->UnmapAction("interact");
    Input_->UnmapAxis("move_right");
    Input_->UnmapAxis("move_forward");
  }

  void TearDown() override
  {
    // Restore clean state so other test suites aren't affected
    if (Current_) memset(Current_->Keys, 0, sizeof(Current_->Keys));
    if (Prev_) memset(Prev_->Keys, 0, sizeof(Prev_->Keys));
  }

  void SimulateKeyDown(int Key)
  {
    Current_->Keys[Key] = true;
  }

  void SimulateKeyUp(int Key)
  {
    Current_->Keys[Key] = false;
  }

  void SimulatePrevKeyDown(int Key)
  {
    Prev_->Keys[Key] = true;
  }

  AxInput* Input_{nullptr};
  AxInputState* Current_{nullptr};
  AxInputState* Prev_{nullptr};
};

//=============================================================================
// Action Mapping Tests
//=============================================================================

TEST_F(InputActionTest, MapAction_SingleKey_Binds)
{
  Input_->MapAction("jump", AX_KEY_SPACE);
  SimulateKeyDown(AX_KEY_SPACE);
  EXPECT_TRUE(Input_->IsActionDown("jump"));
}

TEST_F(InputActionTest, MapAction_MultipleKeys_AnyTriggers)
{
  Input_->MapAction("jump", AX_KEY_SPACE);
  Input_->MapAction("jump", AX_KEY_W);

  SimulateKeyDown(AX_KEY_W);
  EXPECT_TRUE(Input_->IsActionDown("jump"));
}

TEST_F(InputActionTest, MapAction_DuplicateKey_NoDuplicate)
{
  Input_->MapAction("jump", AX_KEY_SPACE);
  Input_->MapAction("jump", AX_KEY_SPACE);

  SimulateKeyDown(AX_KEY_SPACE);
  EXPECT_TRUE(Input_->IsActionDown("jump"));
}

TEST_F(InputActionTest, UnmapAction_RemovesBinding)
{
  Input_->MapAction("jump", AX_KEY_SPACE);
  Input_->UnmapAction("jump");

  SimulateKeyDown(AX_KEY_SPACE);
  EXPECT_FALSE(Input_->IsActionDown("jump"));
}

TEST_F(InputActionTest, UnmapAction_NonExistent_NoOp)
{
  Input_->UnmapAction("nonexistent"); // should not crash
}

TEST_F(InputActionTest, IsActionDown_NotMapped_ReturnsFalse)
{
  EXPECT_FALSE(Input_->IsActionDown("nonexistent"));
}

TEST_F(InputActionTest, IsActionDown_KeyHeld_ReturnsTrue)
{
  Input_->MapAction("attack", AX_KEY_A);
  SimulateKeyDown(AX_KEY_A);
  EXPECT_TRUE(Input_->IsActionDown("attack"));
}

TEST_F(InputActionTest, IsActionDown_KeyNotHeld_ReturnsFalse)
{
  Input_->MapAction("attack", AX_KEY_A);
  EXPECT_FALSE(Input_->IsActionDown("attack"));
}

TEST_F(InputActionTest, IsActionPressed_FirstFrame_ReturnsTrue)
{
  Input_->MapAction("jump", AX_KEY_SPACE);
  SimulateKeyDown(AX_KEY_SPACE);
  // Prev is not down -> pressed this frame
  EXPECT_TRUE(Input_->IsActionPressed("jump"));
}

TEST_F(InputActionTest, IsActionPressed_HeldSecondFrame_ReturnsFalse)
{
  Input_->MapAction("jump", AX_KEY_SPACE);
  SimulateKeyDown(AX_KEY_SPACE);
  SimulatePrevKeyDown(AX_KEY_SPACE);
  // Both current and prev are down -> not "pressed" this frame
  EXPECT_FALSE(Input_->IsActionPressed("jump"));
}

TEST_F(InputActionTest, IsActionPressed_NotMapped_ReturnsFalse)
{
  EXPECT_FALSE(Input_->IsActionPressed("nonexistent"));
}

TEST_F(InputActionTest, IsActionReleased_JustReleased_ReturnsTrue)
{
  Input_->MapAction("jump", AX_KEY_SPACE);
  SimulateKeyUp(AX_KEY_SPACE);
  SimulatePrevKeyDown(AX_KEY_SPACE);
  EXPECT_TRUE(Input_->IsActionReleased("jump"));
}

TEST_F(InputActionTest, IsActionReleased_NeverPressed_ReturnsFalse)
{
  Input_->MapAction("jump", AX_KEY_SPACE);
  EXPECT_FALSE(Input_->IsActionReleased("jump"));
}

TEST_F(InputActionTest, IsActionReleased_NotMapped_ReturnsFalse)
{
  EXPECT_FALSE(Input_->IsActionReleased("nonexistent"));
}

//=============================================================================
// Axis Mapping Tests
//=============================================================================

TEST_F(InputActionTest, MapAxis_PositiveKey_ReturnsPositive)
{
  Input_->MapAxis("move_right", AX_KEY_D, AX_KEY_A);
  SimulateKeyDown(AX_KEY_D);
  EXPECT_FLOAT_EQ(Input_->GetAxis("move_right"), 1.0f);
}

TEST_F(InputActionTest, MapAxis_NegativeKey_ReturnsNegative)
{
  Input_->MapAxis("move_right", AX_KEY_D, AX_KEY_A);
  SimulateKeyDown(AX_KEY_A);
  EXPECT_FLOAT_EQ(Input_->GetAxis("move_right"), -1.0f);
}

TEST_F(InputActionTest, MapAxis_BothKeys_ReturnsZero)
{
  Input_->MapAxis("move_right", AX_KEY_D, AX_KEY_A);
  SimulateKeyDown(AX_KEY_D);
  SimulateKeyDown(AX_KEY_A);
  EXPECT_FLOAT_EQ(Input_->GetAxis("move_right"), 0.0f);
}

TEST_F(InputActionTest, MapAxis_NeitherKey_ReturnsZero)
{
  Input_->MapAxis("move_right", AX_KEY_D, AX_KEY_A);
  EXPECT_FLOAT_EQ(Input_->GetAxis("move_right"), 0.0f);
}

TEST_F(InputActionTest, UnmapAxis_RemovesBinding)
{
  Input_->MapAxis("move_right", AX_KEY_D, AX_KEY_A);
  Input_->UnmapAxis("move_right");
  SimulateKeyDown(AX_KEY_D);
  EXPECT_FLOAT_EQ(Input_->GetAxis("move_right"), 0.0f);
}

TEST_F(InputActionTest, UnmapAxis_NonExistent_NoOp)
{
  Input_->UnmapAxis("nonexistent"); // should not crash
}

TEST_F(InputActionTest, GetAxis_NotMapped_ReturnsZero)
{
  EXPECT_FLOAT_EQ(Input_->GetAxis("nonexistent"), 0.0f);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(InputActionTest, RawAPI_StillWorks_AlongsideNamedAPI)
{
  Input_->MapAction("jump", AX_KEY_SPACE);
  SimulateKeyDown(AX_KEY_SPACE);

  // Named API works
  EXPECT_TRUE(Input_->IsActionDown("jump"));
  // Raw API still works
  EXPECT_TRUE(Input_->IsKeyDown(AX_KEY_SPACE));
}

TEST_F(InputActionTest, RawGetAxis_StillWorks_AlongsideNamedAxis)
{
  Input_->MapAxis("move_right", AX_KEY_D, AX_KEY_A);
  SimulateKeyDown(AX_KEY_D);

  // Named API works
  EXPECT_FLOAT_EQ(Input_->GetAxis("move_right"), 1.0f);
  // Raw API still works
  EXPECT_FLOAT_EQ(Input_->GetAxis(AX_KEY_D, AX_KEY_A), 1.0f);
}

TEST_F(InputActionTest, RuntimeRemap_ChangesBinding)
{
  Input_->MapAction("jump", AX_KEY_SPACE);
  Input_->UnmapAction("jump");
  Input_->MapAction("jump", AX_KEY_W);

  SimulateKeyDown(AX_KEY_SPACE);
  EXPECT_FALSE(Input_->IsActionDown("jump"));

  SimulateKeyDown(AX_KEY_W);
  EXPECT_TRUE(Input_->IsActionDown("jump"));
}
