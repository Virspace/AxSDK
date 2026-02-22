#pragma once

/**
 * AxEventBus.h - Scene-Scoped Publish/Subscribe Event System
 *
 * Provides a scene-scoped publish/subscribe event system for decoupled
 * communication between nodes. Each scene owns its own EventBus instance,
 * ensuring events are scoped to a single scene.
 *
 * Built-in event types fire automatically during node lifecycle operations.
 * User-defined event types start from AX_EVENT_USER_BASE.
 *
 * This is engine-level code (C++).
 */

#include <cstdint>
#include <cstddef>

// Forward declarations
class Node;

//=============================================================================
// Event Type Constants
//=============================================================================

/** Event type identifier. */
typedef uint32_t AxEventType;

/** Built-in event types for node lifecycle. */
#define AX_EVENT_NODE_CREATED        1
#define AX_EVENT_NODE_DESTROYED      2

/** Starting value for user-defined event IDs. */
#define AX_EVENT_USER_BASE           1000

//=============================================================================
// Event Struct
//=============================================================================

/**
 * AxEvent - Data passed to subscribers when an event is published.
 *
 * Contains the event type, the sender node (if applicable), and an
 * optional payload with its size.
 */
struct AxEvent
{
  AxEventType Type;     // Event type identifier
  Node* Sender;         // Node that sent the event (may be nullptr)
  void* Data;           // Optional event-specific payload
  size_t DataSize;      // Size of the payload in bytes
};

//=============================================================================
// Callback Types
//=============================================================================

/**
 * EventCallback - Function pointer type for event subscribers.
 *
 * @param Event The event that was published.
 * @param UserData User-provided context pointer from the subscription.
 */
typedef void (*EventCallback)(const AxEvent* Event, void* UserData);

/**
 * EventSubscription - Represents a single subscription to an event type.
 *
 * Stores the callback function pointer and user data together so
 * that both are needed to match for unsubscription.
 */
struct EventSubscription
{
  EventCallback Callback;
  void* UserData;
};

//=============================================================================
// EventBus Class
//=============================================================================

/**
 * Maximum number of different event types tracked per EventBus.
 */
#define AX_EVENT_BUS_MAX_TYPES 64

/**
 * Maximum number of subscribers per event type.
 */
#define AX_EVENT_BUS_MAX_SUBSCRIBERS_PER_TYPE 64

/**
 * SubscriptionList - Array of subscriptions for a single event type.
 */
struct SubscriptionList
{
  AxEventType Type;
  EventSubscription Subscriptions[AX_EVENT_BUS_MAX_SUBSCRIBERS_PER_TYPE];
  uint32_t Count;
};

/**
 * EventBus - Scene-scoped publish/subscribe event system.
 *
 * Each scene owns one EventBus. Subscribers register callbacks for
 * specific event types. When an event is published, all matching
 * subscribers are notified in registration order.
 *
 * Publishing an event with no subscribers is a safe no-op.
 */
class EventBus
{
public:
  EventBus();
  ~EventBus();

  // Non-copyable
  EventBus(const EventBus&) = delete;
  EventBus& operator=(const EventBus&) = delete;

  /**
   * Subscribe to an event type.
   * @param Type The event type to subscribe to.
   * @param Callback Function to call when the event is published.
   * @param UserData User-provided context pointer (passed to callback).
   */
  void Subscribe(AxEventType Type, EventCallback Callback, void* UserData);

  /**
   * Unsubscribe from an event type.
   * Both Callback and UserData must match the original subscription.
   * @param Type The event type to unsubscribe from.
   * @param Callback The callback that was registered.
   * @param UserData The user data that was registered.
   */
  void Unsubscribe(AxEventType Type, EventCallback Callback, void* UserData);

  /**
   * Publish an event to all subscribers of the matching type.
   * If no subscribers are registered for the event type, this is a no-op.
   * @param Event The event to publish.
   */
  void Publish(const AxEvent& Event);

  /**
   * Get the number of subscribers for a given event type.
   * @param Type The event type to query.
   * @return Number of active subscribers.
   */
  uint32_t GetSubscriberCount(AxEventType Type) const;

private:
  /**
   * Find or create a subscription list for the given event type.
   * @param Type The event type.
   * @return Pointer to the list, or nullptr if at capacity.
   */
  SubscriptionList* GetOrCreateList(AxEventType Type);

  /**
   * Find an existing subscription list for the given event type.
   * @param Type The event type.
   * @return Pointer to the list, or nullptr if not found.
   */
  const SubscriptionList* FindList(AxEventType Type) const;

  SubscriptionList Lists_[AX_EVENT_BUS_MAX_TYPES];
  uint32_t ListCount_;
};
