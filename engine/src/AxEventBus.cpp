/**
 * AxEventBus.cpp - EventBus Implementation
 *
 * Implements the scene-scoped publish/subscribe event system.
 * Each scene owns one EventBus instance. Subscribers register
 * callbacks for specific event types. Publishing with no
 * subscribers is a safe no-op.
 */

#include "AxEngine/AxEventBus.h"

#include <cstring>

//=============================================================================
// EventBus Construction / Destruction
//=============================================================================

EventBus::EventBus()
  : ListCount_(0)
{
  memset(Lists_, 0, sizeof(Lists_));
}

EventBus::~EventBus()
{
  // All subscription lists are inline arrays, nothing to free.
  ListCount_ = 0;
}

//=============================================================================
// Subscribe / Unsubscribe / Publish
//=============================================================================

void EventBus::Subscribe(AxEventType Type, EventCallback Callback, void* UserData)
{
  if (!Callback) {
    return;
  }

  SubscriptionList* List = GetOrCreateList(Type);
  if (!List) {
    return;
  }

  if (List->Count >= AX_EVENT_BUS_MAX_SUBSCRIBERS_PER_TYPE) {
    return;
  }

  // Add the subscription
  EventSubscription& Sub = List->Subscriptions[List->Count];
  Sub.Callback = Callback;
  Sub.UserData = UserData;
  List->Count++;
}

void EventBus::Unsubscribe(AxEventType Type, EventCallback Callback, void* UserData)
{
  if (!Callback) {
    return;
  }

  // Find the list for this event type
  for (uint32_t i = 0; i < ListCount_; ++i) {
    if (Lists_[i].Type == Type) {
      SubscriptionList& List = Lists_[i];

      // Find matching subscription (both Callback and UserData must match)
      for (uint32_t j = 0; j < List.Count; ++j) {
        if (List.Subscriptions[j].Callback == Callback &&
            List.Subscriptions[j].UserData == UserData) {
          // Shift remaining subscriptions to compact the array
          for (uint32_t k = j; k < List.Count - 1; ++k) {
            List.Subscriptions[k] = List.Subscriptions[k + 1];
          }
          List.Count--;
          return;
        }
      }
      return;
    }
  }
}

void EventBus::Publish(const AxEvent& Event)
{
  // Find the subscription list for this event type
  const SubscriptionList* List = FindList(Event.Type);
  if (!List) {
    // No subscribers - safe no-op
    return;
  }

  // Iterate all subscribers and invoke their callbacks
  for (uint32_t i = 0; i < List->Count; ++i) {
    const EventSubscription& Sub = List->Subscriptions[i];
    if (Sub.Callback) {
      Sub.Callback(&Event, Sub.UserData);
    }
  }
}

uint32_t EventBus::GetSubscriberCount(AxEventType Type) const
{
  const SubscriptionList* List = FindList(Type);
  if (!List) {
    return (0);
  }

  return (List->Count);
}

//=============================================================================
// Internal Helpers
//=============================================================================

SubscriptionList* EventBus::GetOrCreateList(AxEventType Type)
{
  // Look for existing list
  for (uint32_t i = 0; i < ListCount_; ++i) {
    if (Lists_[i].Type == Type) {
      return (&Lists_[i]);
    }
  }

  // Create new list if room
  if (ListCount_ >= AX_EVENT_BUS_MAX_TYPES) {
    return (nullptr);
  }

  SubscriptionList* NewList = &Lists_[ListCount_];
  NewList->Type = Type;
  NewList->Count = 0;
  ListCount_++;

  return (NewList);
}

const SubscriptionList* EventBus::FindList(AxEventType Type) const
{
  for (uint32_t i = 0; i < ListCount_; ++i) {
    if (Lists_[i].Type == Type) {
      return (&Lists_[i]);
    }
  }

  return (nullptr);
}
