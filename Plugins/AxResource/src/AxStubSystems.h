#pragma once

/**
 * AxStubSystems.h - Stub Systems for Future Implementation
 *
 * Defines placeholder system classes for Physics, Animation, Audio,
 * and Particle systems. These stubs are registered at the correct
 * execution phase and priority but perform no processing.
 *
 * This is C++ code in the AxScene plugin layer.
 */

#include "AxEngine/AxSystem.h"
#include "AxEngine/AxComponent.h"

/**
 * PhysicsSystem stub - Registered at FixedUpdate phase.
 * Future implementation will process RigidBody and Collider components.
 */
class PhysicsSystem : public System
{
public:
  PhysicsSystem()
    : System("PhysicsSystem", SystemPhase::FixedUpdate, 0, 0) {}

  void Update(float DeltaT, Component** Components, uint32_t Count) override
  {
    (void)DeltaT;
    (void)Components;
    (void)Count;
    // Stub: no processing
  }
};

/**
 * AnimationSystem stub - Registered at Update phase (after ScriptSystem).
 * Future implementation will process Animator components.
 */
class AnimationSystem : public System
{
public:
  AnimationSystem()
    : System("AnimationSystem", SystemPhase::Update, 100, 0) {}

  void Update(float DeltaT, Component** Components, uint32_t Count) override
  {
    (void)DeltaT;
    (void)Components;
    (void)Count;
    // Stub: no processing
  }
};

/**
 * AudioSystem stub - Registered at LateUpdate phase.
 * Future implementation will process AudioSource and AudioListener components.
 */
class AudioSystem : public System
{
public:
  AudioSystem()
    : System("AudioSystem", SystemPhase::LateUpdate, 0, 0) {}

  void Update(float DeltaT, Component** Components, uint32_t Count) override
  {
    (void)DeltaT;
    (void)Components;
    (void)Count;
    // Stub: no processing
  }
};

/**
 * ParticleSystem stub - Registered at Update phase.
 * Future implementation will process ParticleEmitter components.
 */
class ParticleSystem : public System
{
public:
  ParticleSystem()
    : System("ParticleSystem", SystemPhase::Update, 200, 0) {}

  void Update(float DeltaT, Component** Components, uint32_t Count) override
  {
    (void)DeltaT;
    (void)Components;
    (void)Count;
    // Stub: no processing
  }
};
