/**
 * AxEventBusTests.cpp - Tests for EventBus System
 *
 * Tests the scene-scoped publish/subscribe event system:
 * - Subscribe and receive events
 * - Multiple subscribers
 * - Unsubscribe stops delivery
 * - Scene-scoped isolation
 * - Safe no-op on publish with no subscribers
 */

#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxHashTable.h"
#include "Foundation/AxAllocator.h"
#include "Foundation/AxAllocatorAPI.h"
#include "Foundation/AxAPIRegistry.h"
#include "AxEngine/AxEventBus.h"
#include "AxEngine/AxNode.h"

#include <cstring>

//=============================================================================
// Test helpers
//=============================================================================

struct EventTestData
{
  int CallCount;
  AxEventType LastEventType;
  void* LastSender;
  void* LastData;
};

static void TestEventCallback(const AxEvent* Event, void* UserData)
{
  EventTestData* Data = static_cast<EventTestData*>(UserData);
  Data->CallCount++;
  Data->LastEventType = Event->Type;
  Data->LastSender = static_cast<void*>(Event->Sender);
  Data->LastData = Event->Data;
}

static void TestEventCallback2(const AxEvent* Event, void* UserData)
{
  EventTestData* Data = static_cast<EventTestData*>(UserData);
  Data->CallCount += 10; // Different increment to distinguish
  Data->LastEventType = Event->Type;
}

//=============================================================================
// Test Fixture
//=============================================================================

class EventBusTest : public testing::Test
{
protected:
  void SetUp() override
  {
    AxonInitGlobalAPIRegistry();
    AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);
  }

  void TearDown() override
  {
    AxonTermGlobalAPIRegistry();
  }
};

//=============================================================================
// Test 1: Subscribe to an event type and receive published events
//=============================================================================
TEST_F(EventBusTest, SubscribeAndReceiveEvent)
{
  EventBus Bus;

  EventTestData Data = {0, 0, nullptr, nullptr};
  Bus.Subscribe(AX_EVENT_USER_BASE, TestEventCallback, &Data);

  // Publish an event with a dummy sender (cast int as Node* for testing)
  int DummySender = 42;
  AxEvent Evt;
  Evt.Type = AX_EVENT_USER_BASE;
  Evt.Sender = reinterpret_cast<Node*>(&DummySender);
  Evt.Data = nullptr;
  Evt.DataSize = 0;

  Bus.Publish(Evt);

  EXPECT_EQ(Data.CallCount, 1);
  EXPECT_EQ(Data.LastEventType, AX_EVENT_USER_BASE);
  EXPECT_EQ(Data.LastSender, static_cast<void*>(&DummySender));
}

//=============================================================================
// Test 2: Multiple subscribers receive the same published event
//=============================================================================
TEST_F(EventBusTest, MultipleSubscribersReceiveSameEvent)
{
  EventBus Bus;

  EventTestData Data1 = {0, 0, nullptr, nullptr};
  EventTestData Data2 = {0, 0, nullptr, nullptr};

  Bus.Subscribe(AX_EVENT_USER_BASE, TestEventCallback, &Data1);
  Bus.Subscribe(AX_EVENT_USER_BASE, TestEventCallback2, &Data2);

  AxEvent Evt;
  Evt.Type = AX_EVENT_USER_BASE;
  Evt.Sender = nullptr;
  Evt.Data = nullptr;
  Evt.DataSize = 0;

  Bus.Publish(Evt);

  EXPECT_EQ(Data1.CallCount, 1);
  EXPECT_EQ(Data2.CallCount, 10); // TestEventCallback2 increments by 10
}

//=============================================================================
// Test 3: Unsubscribing stops event delivery
//=============================================================================
TEST_F(EventBusTest, UnsubscribeStopsDelivery)
{
  EventBus Bus;

  EventTestData Data = {0, 0, nullptr, nullptr};
  Bus.Subscribe(AX_EVENT_USER_BASE, TestEventCallback, &Data);

  // Publish once - should receive
  AxEvent Evt;
  Evt.Type = AX_EVENT_USER_BASE;
  Evt.Sender = nullptr;
  Evt.Data = nullptr;
  Evt.DataSize = 0;

  Bus.Publish(Evt);
  EXPECT_EQ(Data.CallCount, 1);

  // Unsubscribe
  Bus.Unsubscribe(AX_EVENT_USER_BASE, TestEventCallback, &Data);

  // Publish again - should NOT receive
  Bus.Publish(Evt);
  EXPECT_EQ(Data.CallCount, 1); // Still 1, not incremented
}

//=============================================================================
// Test 4: Events are scene-scoped (two EventBus instances are independent)
//=============================================================================
TEST_F(EventBusTest, EventsAreSceneScoped)
{
  EventBus BusA;
  EventBus BusB;

  EventTestData DataA = {0, 0, nullptr, nullptr};
  EventTestData DataB = {0, 0, nullptr, nullptr};

  BusA.Subscribe(AX_EVENT_USER_BASE, TestEventCallback, &DataA);
  BusB.Subscribe(AX_EVENT_USER_BASE, TestEventCallback, &DataB);

  // Publish only on BusA
  AxEvent Evt;
  Evt.Type = AX_EVENT_USER_BASE;
  Evt.Sender = nullptr;
  Evt.Data = nullptr;
  Evt.DataSize = 0;

  BusA.Publish(Evt);

  // Only DataA should have received the event
  EXPECT_EQ(DataA.CallCount, 1);
  EXPECT_EQ(DataB.CallCount, 0);
}

//=============================================================================
// Test 5: Publishing with no subscribers is a safe no-op
//=============================================================================
TEST_F(EventBusTest, PublishWithNoSubscribersIsSafeNoOp)
{
  EventBus Bus;

  // Publish with no subscribers - should not crash
  AxEvent Evt;
  Evt.Type = AX_EVENT_USER_BASE + 42;
  Evt.Sender = nullptr;
  Evt.Data = nullptr;
  Evt.DataSize = 0;

  EXPECT_NO_THROW(Bus.Publish(Evt));
  EXPECT_EQ(Bus.GetSubscriberCount(AX_EVENT_USER_BASE + 42), 0u);
}
