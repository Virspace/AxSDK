/**
 * AxStandardComponents.cpp - Factory Registration for Standard Components
 *
 * Implements create/destroy factory functions for all standard component types
 * and registers them with the ComponentFactory during AxScene plugin load.
 *
 * Also contains the ScriptComponent delegate method implementations, which
 * require the full AxScript definition (AxScript.h has a heavy include chain
 * that we do not want to expose through the header).
 */

#include "AxStandardComponents.h"
#include "AxEngine/AxComponentFactory.h"
#include "AxEngine/AxScript.h"

#include <new>
#include <cstdio>

//=============================================================================
// ScriptComponent Delegate Method Implementations
//=============================================================================

void ScriptComponent::DelegateInit()
{
  if (ScriptPtr) {
    ScriptPtr->Init();
  }
}

void ScriptComponent::DelegateStart()
{
  if (ScriptPtr) {
    ScriptPtr->Start();
  }
}

void ScriptComponent::DelegateFixedTick(float FixedDeltaT)
{
  if (ScriptPtr) {
    ScriptPtr->FixedTick(FixedDeltaT);
  }
}

void ScriptComponent::DelegateTick(float Alpha, float DeltaT)
{
  if (ScriptPtr) {
    ScriptPtr->Tick(Alpha, DeltaT);
  }
}

void ScriptComponent::DelegateLateTick()
{
  if (ScriptPtr) {
    ScriptPtr->LateTick();
  }
}

void ScriptComponent::DelegateStop()
{
  if (ScriptPtr) {
    ScriptPtr->Stop();
  }
}

void ScriptComponent::DelegateTerm()
{
  if (ScriptPtr) {
    ScriptPtr->Term();
  }
}

//=============================================================================
// Rendering Component Factory Functions
//=============================================================================

static Component* CreateMeshFilterComponent(void* Memory)
{
  return (new (Memory) MeshFilter());
}

static void DestroyMeshFilterComponent(Component* Comp)
{
  static_cast<MeshFilter*>(Comp)->~MeshFilter();
}

static Component* CreateMeshRendererComponent(void* Memory)
{
  return (new (Memory) MeshRenderer());
}

static void DestroyMeshRendererComponent(Component* Comp)
{
  static_cast<MeshRenderer*>(Comp)->~MeshRenderer();
}

static Component* CreateCameraComponentFactory(void* Memory)
{
  return (new (Memory) CameraComponent());
}

static void DestroyCameraComponentFactory(Component* Comp)
{
  static_cast<CameraComponent*>(Comp)->~CameraComponent();
}

static Component* CreateLightComponentFactory(void* Memory)
{
  return (new (Memory) LightComponent());
}

static void DestroyLightComponentFactory(Component* Comp)
{
  static_cast<LightComponent*>(Comp)->~LightComponent();
}

static Component* CreateSpriteRendererComponent(void* Memory)
{
  return (new (Memory) SpriteRenderer());
}

static void DestroySpriteRendererComponent(Component* Comp)
{
  static_cast<SpriteRenderer*>(Comp)->~SpriteRenderer();
}

//=============================================================================
// Physics Component Factory Functions
//=============================================================================

static Component* CreateRigidBodyComponent(void* Memory)
{
  return (new (Memory) RigidBody());
}

static void DestroyRigidBodyComponent(Component* Comp)
{
  static_cast<RigidBody*>(Comp)->~RigidBody();
}

static Component* CreateColliderComponent(void* Memory)
{
  return (new (Memory) Collider());
}

static void DestroyColliderComponent(Component* Comp)
{
  static_cast<Collider*>(Comp)->~Collider();
}

//=============================================================================
// Audio Component Factory Functions
//=============================================================================

static Component* CreateAudioSourceComponent(void* Memory)
{
  return (new (Memory) AudioSource());
}

static void DestroyAudioSourceComponent(Component* Comp)
{
  static_cast<AudioSource*>(Comp)->~AudioSource();
}

static Component* CreateAudioListenerComponent(void* Memory)
{
  return (new (Memory) AudioListener());
}

static void DestroyAudioListenerComponent(Component* Comp)
{
  static_cast<AudioListener*>(Comp)->~AudioListener();
}

//=============================================================================
// Gameplay Component Factory Functions
//=============================================================================

static Component* CreateScriptComponentFactory(void* Memory)
{
  return (new (Memory) ScriptComponent());
}

static void DestroyScriptComponentFactory(Component* Comp)
{
  static_cast<ScriptComponent*>(Comp)->~ScriptComponent();
}

static Component* CreateAnimatorComponent(void* Memory)
{
  return (new (Memory) Animator());
}

static void DestroyAnimatorComponent(Component* Comp)
{
  static_cast<Animator*>(Comp)->~Animator();
}

static Component* CreateParticleEmitterComponent(void* Memory)
{
  return (new (Memory) ParticleEmitter());
}

static void DestroyParticleEmitterComponent(Component* Comp)
{
  static_cast<ParticleEmitter*>(Comp)->~ParticleEmitter();
}

//=============================================================================
// Registration
//=============================================================================

void RegisterStandardComponents(ComponentFactory* Factory)
{
  if (!Factory) {
    return;
  }

  fprintf(stderr, "[COMP] Registering MeshFilter...\n");
  Factory->RegisterType("MeshFilter",
    CreateMeshFilterComponent, DestroyMeshFilterComponent,
    sizeof(MeshFilter));

  fprintf(stderr, "[COMP] Registering MeshRenderer...\n");
  Factory->RegisterType("MeshRenderer",
    CreateMeshRendererComponent, DestroyMeshRendererComponent,
    sizeof(MeshRenderer));

  fprintf(stderr, "[COMP] Registering CameraComponent...\n");
  Factory->RegisterType("CameraComponent",
    CreateCameraComponentFactory, DestroyCameraComponentFactory,
    sizeof(CameraComponent));

  fprintf(stderr, "[COMP] Registering LightComponent...\n");
  Factory->RegisterType("LightComponent",
    CreateLightComponentFactory, DestroyLightComponentFactory,
    sizeof(LightComponent));

  fprintf(stderr, "[COMP] Registering SpriteRenderer...\n");
  Factory->RegisterType("SpriteRenderer",
    CreateSpriteRendererComponent, DestroySpriteRendererComponent,
    sizeof(SpriteRenderer));

  fprintf(stderr, "[COMP] Registering RigidBody...\n");
  Factory->RegisterType("RigidBody",
    CreateRigidBodyComponent, DestroyRigidBodyComponent,
    sizeof(RigidBody));

  fprintf(stderr, "[COMP] Registering Collider...\n");
  Factory->RegisterType("Collider",
    CreateColliderComponent, DestroyColliderComponent,
    sizeof(Collider));

  fprintf(stderr, "[COMP] Registering AudioSource...\n");
  Factory->RegisterType("AudioSource",
    CreateAudioSourceComponent, DestroyAudioSourceComponent,
    sizeof(AudioSource));

  fprintf(stderr, "[COMP] Registering AudioListener...\n");
  Factory->RegisterType("AudioListener",
    CreateAudioListenerComponent, DestroyAudioListenerComponent,
    sizeof(AudioListener));

  fprintf(stderr, "[COMP] Registering ScriptComponent...\n");
  Factory->RegisterType("ScriptComponent",
    CreateScriptComponentFactory, DestroyScriptComponentFactory,
    sizeof(ScriptComponent));

  fprintf(stderr, "[COMP] Registering Animator...\n");
  Factory->RegisterType("Animator",
    CreateAnimatorComponent, DestroyAnimatorComponent,
    sizeof(Animator));

  fprintf(stderr, "[COMP] Registering ParticleEmitter...\n");
  Factory->RegisterType("ParticleEmitter",
    CreateParticleEmitterComponent, DestroyParticleEmitterComponent,
    sizeof(ParticleEmitter));

  fprintf(stderr, "[COMP] All components registered\n");
}
