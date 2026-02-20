/**
 * AxCoreSystems.cpp - Core System Registration Implementation
 *
 * Creates singleton instances of all core systems and registers
 * them with the global SystemFactory during plugin load.
 */

#include "AxCoreSystems.h"
#include "AxTransformSystem.h"
#include "AxRenderSystem.h"
#include "AxScriptSystem.h"
#include "AxStubSystems.h"
#include "AxEngine/AxSystemFactory.h"

//=============================================================================
// Global System Instances
//=============================================================================

static TransformSystem* gTransformSystem = nullptr;
static RenderSystem* gRenderSystem = nullptr;
static ScriptSystem* gScriptSystem = nullptr;
static PhysicsSystem* gPhysicsSystem = nullptr;
static AnimationSystem* gAnimationSystem = nullptr;
static AudioSystem* gAudioSystem = nullptr;
static ParticleSystem* gParticleSystem = nullptr;

//=============================================================================
// Registration
//=============================================================================

void RegisterCoreSystems(void)
{
  SystemFactory* Factory = AxSystemFactory_Get();

  // Create core system instances
  gTransformSystem = new TransformSystem();
  gRenderSystem    = new RenderSystem();
  gScriptSystem    = new ScriptSystem();
  gPhysicsSystem   = new PhysicsSystem();
  gAnimationSystem = new AnimationSystem();
  gAudioSystem     = new AudioSystem();
  gParticleSystem  = new ParticleSystem();

  // Register with SystemFactory
  if (Factory) {
    Factory->RegisterSystem(static_cast<System*>(gTransformSystem));
    Factory->RegisterSystem(static_cast<System*>(gRenderSystem));
    Factory->RegisterSystem(static_cast<System*>(gScriptSystem));
    Factory->RegisterSystem(static_cast<System*>(gPhysicsSystem));
    Factory->RegisterSystem(static_cast<System*>(gAnimationSystem));
    Factory->RegisterSystem(static_cast<System*>(gAudioSystem));
    Factory->RegisterSystem(static_cast<System*>(gParticleSystem));
  }
}

void UnregisterCoreSystems(void)
{
  delete gTransformSystem;
  gTransformSystem = nullptr;

  delete gRenderSystem;
  gRenderSystem = nullptr;

  delete gScriptSystem;
  gScriptSystem = nullptr;

  delete gPhysicsSystem;
  gPhysicsSystem = nullptr;

  delete gAnimationSystem;
  gAnimationSystem = nullptr;

  delete gAudioSystem;
  gAudioSystem = nullptr;

  delete gParticleSystem;
  gParticleSystem = nullptr;
}
