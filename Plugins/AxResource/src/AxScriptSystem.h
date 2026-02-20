#pragma once

/**
 * AxScriptSystem.h - Script System for Hybrid Node Tree + Component Architecture
 *
 * Queries the scene for all ScriptComponents and calls their hosted
 * AxScript lifecycle methods in the correct order:
 * - Init/Start called once
 * - Tick/LateTick called each frame
 *
 * Registered at Update phase.
 *
 * This is C++ code in the AxScene plugin layer.
 */

#include "AxEngine/AxSystem.h"
#include "AxEngine/AxComponent.h"

#include <cstdint>

/**
 * ScriptSystem - Manages AxScript lifecycle for all ScriptComponents.
 *
 * Tracks which scripts have been initialized and started so that
 * Init/Start are only called once per script instance.
 */
class ScriptSystem : public System
{
public:
  ScriptSystem();

  void Init() override;
  void Shutdown() override;

  /**
   * Processes all ScriptComponents, calling lifecycle methods:
   * - DelegateInit() on first encounter
   * - DelegateStart() on second encounter (after init)
   * - DelegateTick() every subsequent frame
   */
  void Update(float DeltaT, Component** Components, uint32_t Count) override;

private:
  bool FirstFrame_;
};
