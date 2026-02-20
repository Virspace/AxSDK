#pragma once

/**
 * AxSystem.h - System Base Class for Hybrid Node Tree + Component Architecture
 *
 * Systems iterate over components by type for batch processing each frame.
 * Each system has a defined execution phase and priority for ordering within
 * that phase. The Scene manages system registration and calls Update in the
 * correct order.
 *
 * Follows the existing AxScript lifecycle pattern (Init, Start, FixedTick,
 * Tick, LateTick, Stop, Term) adapted for system-level processing.
 *
 * This is engine-level code (C++).
 */

#include "Foundation/AxTypes.h"
#include <string>
#include <string_view>

// Forward declaration
class Component;

/**
 * SystemPhase defines when during the frame a system executes.
 * Systems within the same phase are ordered by Priority (lower runs first).
 */
enum class SystemPhase : uint32_t
{
  EarlyUpdate = 0,  // Before main update (e.g., TransformSystem)
  FixedUpdate,      // Fixed timestep (e.g., PhysicsSystem)
  Update,           // Main update (e.g., ScriptSystem, AnimationSystem)
  LateUpdate,       // After main update (e.g., AudioSystem)
  Render            // Render pass (e.g., RenderSystem)
};

/**
 * System - Base class for all systems that process components each frame.
 *
 * Systems are registered with a Scene and called in order of Phase then
 * Priority. Each system declares which component type it processes via
 * ComponentTypeID, and the Scene passes the matching flat component list
 * to the Update method.
 */
class System
{
public:
  System(std::string_view Name, SystemPhase Phase, int32_t Priority, uint32_t ComponentTypeID)
    : Name_(Name)
    , Phase_(Phase)
    , Priority_(Priority)
    , ComponentTypeID_(ComponentTypeID)
  {}

  virtual ~System() {}

  // Non-copyable
  System(const System&) = delete;
  System& operator=(const System&) = delete;

  //=========================================================================
  // Lifecycle Methods
  //=========================================================================

  /**
   * Called once when the system is registered with a scene.
   * Use for one-time resource setup.
   */
  virtual void Init() {}

  /**
   * Called once when the system is unregistered from a scene.
   * Use for resource cleanup.
   */
  virtual void Shutdown() {}

  /**
   * Called each frame (or fixed step) with the array of matching components.
   * @param DeltaT Time elapsed since last call (seconds).
   * @param Components Array of Component pointers matching this system's type.
   * @param Count Number of components in the array.
   */
  virtual void Update(float DeltaT, Component** Components, uint32_t Count) = 0;

  //=========================================================================
  // Accessors
  //=========================================================================

  std::string_view GetName() const { return (Name_); }
  SystemPhase GetPhase() const { return (Phase_); }
  int32_t GetPriority() const { return (Priority_); }
  uint32_t GetComponentTypeID() const { return (ComponentTypeID_); }

protected:
  std::string Name_;
  SystemPhase Phase_;
  int32_t Priority_;
  uint32_t ComponentTypeID_;
};
