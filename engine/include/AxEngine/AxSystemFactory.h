#pragma once

/**
 * AxSystemFactory.h - System Factory for Plugin Registration of Systems
 *
 * Provides a registry for systems that plugins can use to register and
 * unregister system instances. Engine-internal C++ only — no C API wrapper.
 * Access via AxSystemFactory_Get().
 */

#include <cstdint>

// Forward declarations
class System;

/**
 * Maximum number of systems that can be registered globally.
 */
#define AX_MAX_REGISTERED_SYSTEMS 64

/**
 * SystemFactory - Global registry of system instances.
 *
 * Systems are registered here so that scenes can discover and use them.
 * This is a simple array-based registry (no hash table needed since
 * system counts are small and iteration is the primary use case).
 */
class SystemFactory
{
public:
  SystemFactory();
  ~SystemFactory();

  // Non-copyable
  SystemFactory(const SystemFactory&) = delete;
  SystemFactory& operator=(const SystemFactory&) = delete;

  /**
   * Register a system instance.
   * @param Sys System to register (must not be nullptr).
   * @return true if registered, false if nullptr or registry full.
   */
  bool RegisterSystem(System* Sys);

  /**
   * Unregister a previously registered system instance.
   * @param Sys System to unregister.
   * @return true if found and removed, false otherwise.
   */
  bool UnregisterSystem(System* Sys);

  /**
   * Get the number of registered systems.
   */
  uint32_t GetSystemCount() const;

  /**
   * Get a system by index.
   * @param Index Zero-based index (0 to GetSystemCount()-1).
   * @return Pointer to the system, or nullptr if out of range.
   */
  System* GetSystemAtIndex(uint32_t Index) const;

private:
  System* Systems_[AX_MAX_REGISTERED_SYSTEMS];
  uint32_t SystemCount_;
};

//=============================================================================
// Global Accessor
//=============================================================================

/**
 * Get the global SystemFactory instance (engine-internal use).
 * The factory is a static object initialized at DLL load; no Init/Term needed.
 */
SystemFactory* AxSystemFactory_Get();
