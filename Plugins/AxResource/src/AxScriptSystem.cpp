/**
 * AxScriptSystem.cpp - Script System Implementation
 *
 * Iterates all ScriptComponents and delegates lifecycle calls to
 * their hosted AxScript instances. Uses a simple flag approach:
 * scripts track their own init/start state internally.
 *
 * The system calls DelegateTick on every frame for all active scripts.
 * Init and Start are handled by the ScriptComponent's OnInitialize
 * (called when the node is initialized).
 */

#include "AxScriptSystem.h"
#include "AxStandardComponents.h"

ScriptSystem::ScriptSystem()
  : System("ScriptSystem", SystemPhase::Update, 0, 0)
  , FirstFrame_(true)
{
}

void ScriptSystem::Init()
{
  FirstFrame_ = true;
}

void ScriptSystem::Shutdown()
{
  // Nothing to clean up
}

void ScriptSystem::Update(float DeltaT, Component** Components, uint32_t Count)
{
  if (!Components || Count == 0) {
    return;
  }

  for (uint32_t i = 0; i < Count; ++i) {
    Component* Comp = Components[i];
    if (!Comp || !Comp->IsEnabled()) {
      continue;
    }

    ScriptComponent* SC = static_cast<ScriptComponent*>(Comp);

    if (!SC->GetScript()) {
      continue;
    }

    // Delegate tick to the hosted script
    SC->DelegateTick(1.0f, DeltaT);
  }

  FirstFrame_ = false;
}
