/**
 * AxSystemFactory.cpp - System Factory Implementation
 *
 * Manages global registration of system instances. Uses a simple fixed-size
 * array since system counts are typically small (< 64).
 *
 * The global SystemFactory is a plain static object — it initializes at
 * DLL load and destructs at DLL unload with no Init/Term calls required.
 */

#include "AxEngine/AxSystemFactory.h"
#include "AxEngine/AxSystem.h"

#include <cstring>

//=============================================================================
// Construction / Destruction
//=============================================================================

SystemFactory::SystemFactory()
  : SystemCount_(0)
{
  memset(Systems_, 0, sizeof(Systems_));
}

SystemFactory::~SystemFactory()
{
  SystemCount_ = 0;
  memset(Systems_, 0, sizeof(Systems_));
}

//=============================================================================
// System Registration
//=============================================================================

bool SystemFactory::RegisterSystem(System* Sys)
{
  if (!Sys) {
    return (false);
  }

  if (SystemCount_ >= AX_MAX_REGISTERED_SYSTEMS) {
    return (false);
  }

  // Check for duplicate registration
  for (uint32_t i = 0; i < SystemCount_; ++i) {
    if (Systems_[i] == Sys) {
      return (false);
    }
  }

  Systems_[SystemCount_] = Sys;
  SystemCount_++;
  return (true);
}

bool SystemFactory::UnregisterSystem(System* Sys)
{
  if (!Sys) {
    return (false);
  }

  for (uint32_t i = 0; i < SystemCount_; ++i) {
    if (Systems_[i] == Sys) {
      // Compact the array by shifting remaining elements
      for (uint32_t j = i; j < SystemCount_ - 1; ++j) {
        Systems_[j] = Systems_[j + 1];
      }
      Systems_[SystemCount_ - 1] = nullptr;
      SystemCount_--;
      return (true);
    }
  }

  return (false);
}

uint32_t SystemFactory::GetSystemCount() const
{
  return (SystemCount_);
}

System* SystemFactory::GetSystemAtIndex(uint32_t Index) const
{
  if (Index >= SystemCount_) {
    return (nullptr);
  }

  return (Systems_[Index]);
}

//=============================================================================
// Global Instance
//=============================================================================

static SystemFactory gSystemFactory;

/**
 * Get the global SystemFactory instance (engine-internal use).
 */
SystemFactory* AxSystemFactory_Get()
{
  return (&gSystemFactory);
}
