#pragma once

/**
 * AxStandardComponents.h - Standard Component Types for AxScene Plugin
 *
 * Concrete component implementations that wrap existing Foundation types
 * (AxCamera, AxLight, AxMaterialDesc) and define interface-only components
 * for systems not yet implemented (Physics, Audio).
 *
 * All components extend the Component base class and follow the
 * one-component-per-type-per-node rule enforced by Node::AddComponent.
 *
 * This is C++ code in the AxScene plugin layer.
 */

#include "AxEngine/AxComponent.h"
#include "AxEngine/AxSceneTypes.h"
#include "AxResource/AxResourceTypes.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxMath.h"

#include <cstdint>
#include <cstring>

// Forward declarations
class AxScript;

//=============================================================================
// Rendering Components
//=============================================================================

/**
 * MeshFilter - References a mesh asset for rendering.
 *
 * Holds a path to the mesh file and an optional runtime handle for the
 * loaded mesh resource. Paired with MeshRenderer for visible geometry.
 */
class MeshFilter : public Component
{
public:
  static constexpr uint32_t TypeID = 0; // Assigned by ComponentFactory at registration

  char MeshPath[256];
  AxModelHandle ModelHandle;

  MeshFilter()
    : ModelHandle{0, 0}
  {
    MeshPath[0] = '\0';
  }

  void SetMeshPath(const char* Path)
  {
    if (Path) {
      strncpy(MeshPath, Path, sizeof(MeshPath) - 1);
      MeshPath[sizeof(MeshPath) - 1] = '\0';
    }
  }

  size_t GetSize() const override { return (sizeof(MeshFilter)); }
};

/**
 * MeshRenderer - Controls how a mesh is rendered.
 *
 * Binds a material by name and holds a runtime handle for the resolved
 * material. RenderLayer controls draw ordering.
 */
class MeshRenderer : public Component
{
public:
  static constexpr uint32_t TypeID = 0; // Assigned by ComponentFactory at registration

  char MaterialName[64];
  uint32_t MaterialHandle;
  int32_t RenderLayer;

  MeshRenderer()
    : MaterialHandle(0)
    , RenderLayer(0)
  {
    MaterialName[0] = '\0';
  }

  void SetMaterialName(const char* Name)
  {
    if (Name) {
      strncpy(MaterialName, Name, sizeof(MaterialName) - 1);
      MaterialName[sizeof(MaterialName) - 1] = '\0';
    }
  }

  size_t GetSize() const override { return (sizeof(MeshRenderer)); }
};

/**
 * CameraComponent - Wraps AxCamera struct for node-based camera control.
 *
 * Exposes FOV, near/far clip planes, and projection mode.
 * The underlying AxCamera struct provides the full camera state.
 */
class CameraComponent : public Component
{
public:
  static constexpr uint32_t TypeID = 0; // Assigned by ComponentFactory at registration

  AxCamera Camera;

  CameraComponent()
  {
    memset(&Camera, 0, sizeof(AxCamera));
    Camera.FieldOfView = 60.0f;
    Camera.NearClipPlane = 0.1f;
    Camera.FarClipPlane = 1000.0f;
    Camera.AspectRatio = 16.0f / 9.0f;
    Camera.IsOrthographic = false;
    Camera.ZoomLevel = 1.0f;
    Camera.Transform = TransformIdentity();
  }

  float GetFOV() const { return (Camera.FieldOfView); }
  void SetFOV(float FOV) { Camera.FieldOfView = FOV; }

  float GetNear() const { return (Camera.NearClipPlane); }
  void SetNear(float Near) { Camera.NearClipPlane = Near; }

  float GetFar() const { return (Camera.FarClipPlane); }
  void SetFar(float Far) { Camera.FarClipPlane = Far; }

  bool IsOrthographic() const { return (Camera.IsOrthographic); }
  void SetOrthographic(bool Ortho) { Camera.IsOrthographic = Ortho; }

  size_t GetSize() const override { return (sizeof(CameraComponent)); }
};

/**
 * LightComponent - Wraps AxLight struct for node-based lighting.
 *
 * Exposes light type, color, intensity, and other properties.
 * The underlying AxLight struct provides the full light state.
 */
class LightComponent : public Component
{
public:
  static constexpr uint32_t TypeID = 0; // Assigned by ComponentFactory at registration

  AxLight Light;

  LightComponent()
  {
    memset(&Light, 0, sizeof(AxLight));
    Light.Type = AX_LIGHT_TYPE_POINT;
    Light.Color = {1.0f, 1.0f, 1.0f};
    Light.Intensity = 1.0f;
    Light.Direction = {0.0f, -1.0f, 0.0f};
    Light.Range = 0.0f;
    Light.InnerConeAngle = 0.5f;
    Light.OuterConeAngle = 1.0f;
  }

  AxLightType GetLightType() const { return (Light.Type); }
  void SetLightType(AxLightType Type) { Light.Type = Type; }

  AxVec3 GetColor() const { return (Light.Color); }
  void SetColor(AxVec3 Color) { Light.Color = Color; }

  float GetIntensity() const { return (Light.Intensity); }
  void SetIntensity(float Intensity) { Light.Intensity = Intensity; }

  size_t GetSize() const override { return (sizeof(LightComponent)); }
};

/**
 * SpriteRenderer - 2D rendering component for sprites.
 *
 * Holds a texture path, runtime handle, tint color, and sort order.
 */
class SpriteRenderer : public Component
{
public:
  static constexpr uint32_t TypeID = 0; // Assigned by ComponentFactory at registration

  char TexturePath[256];
  uint32_t TextureHandle;
  AxVec4 Color;
  int32_t SortOrder;

  SpriteRenderer()
    : TextureHandle(0)
    , SortOrder(0)
  {
    TexturePath[0] = '\0';
    Color = {1.0f, 1.0f, 1.0f, 1.0f};
  }

  size_t GetSize() const override { return (sizeof(SpriteRenderer)); }
};

//=============================================================================
// Physics Interface Components (data-only, no simulation)
//=============================================================================

/**
 * Body type for rigid body simulation.
 */
enum class BodyType : uint32_t
{
  Static = 0,
  Dynamic,
  Kinematic
};

/**
 * RigidBody - Data-only physics body interface.
 *
 * Stores mass, velocity, drag, and body type for future physics system use.
 * No simulation is performed by this component -- it is a data interface only.
 */
class RigidBody : public Component
{
public:
  static constexpr uint32_t TypeID = 0; // Assigned by ComponentFactory at registration

  float Mass;
  AxVec3 LinearVelocity;
  AxVec3 AngularVelocity;
  float Drag;
  BodyType Type;

  RigidBody()
    : Mass(1.0f)
    , Drag(0.0f)
    , Type(BodyType::Dynamic)
  {
    LinearVelocity = {0.0f, 0.0f, 0.0f};
    AngularVelocity = {0.0f, 0.0f, 0.0f};
  }

  size_t GetSize() const override { return (sizeof(RigidBody)); }
};

/**
 * Collider shape types.
 */
enum class ShapeType : uint32_t
{
  Box = 0,
  Sphere,
  Capsule,
  Mesh
};

/**
 * Collider - Data-only collision shape interface.
 *
 * Stores shape parameters for future physics collision detection.
 * No collision detection is performed by this component.
 */
class Collider : public Component
{
public:
  static constexpr uint32_t TypeID = 0; // Assigned by ComponentFactory at registration

  ShapeType Shape;
  AxVec3 Extents;
  float Radius;
  float Height;
  bool IsTrigger;

  Collider()
    : Shape(ShapeType::Box)
    , Radius(0.5f)
    , Height(1.0f)
    , IsTrigger(false)
  {
    Extents = {0.5f, 0.5f, 0.5f};
  }

  size_t GetSize() const override { return (sizeof(Collider)); }
};

//=============================================================================
// Audio Interface Components (data-only, no playback)
//=============================================================================

/**
 * AudioSource - Data-only audio playback interface.
 *
 * Stores clip path, volume, pitch, looping, and spatial audio settings.
 * No audio playback is performed by this component.
 */
class AudioSource : public Component
{
public:
  static constexpr uint32_t TypeID = 0; // Assigned by ComponentFactory at registration

  char ClipPath[256];
  float Volume;
  float Pitch;
  bool IsLooping;
  bool Is3D;
  float MaxDistance;

  AudioSource()
    : Volume(1.0f)
    , Pitch(1.0f)
    , IsLooping(false)
    , Is3D(true)
    , MaxDistance(100.0f)
  {
    ClipPath[0] = '\0';
  }

  size_t GetSize() const override { return (sizeof(AudioSource)); }
};

/**
 * AudioListener - Marker component for spatial audio receiver.
 *
 * Nodes with this component receive 3D audio based on their position.
 * Typically attached to the main camera node. No additional data fields.
 */
class AudioListener : public Component
{
public:
  static constexpr uint32_t TypeID = 0; // Assigned by ComponentFactory at registration

  AudioListener() {}

  size_t GetSize() const override { return (sizeof(AudioListener)); }
};

//=============================================================================
// Gameplay Components
//=============================================================================

/**
 * ScriptComponent - Hosts an AxScript and delegates lifecycle calls.
 *
 * Wraps an AxScript pointer and forwards lifecycle methods (Init, Start,
 * Tick, etc.) to the hosted script. The ScriptSystem calls these during
 * the Update phase.
 *
 * Delegate method implementations are in AxStandardComponents.cpp to avoid
 * pulling the heavy AxScript.h dependency chain into this header.
 */
class ScriptComponent : public Component
{
public:
  static constexpr uint32_t TypeID = 0; // Assigned by ComponentFactory at registration

  AxScript* ScriptPtr;

  ScriptComponent()
    : ScriptPtr(nullptr)
  {}

  void SetScript(AxScript* Script) { ScriptPtr = Script; }
  AxScript* GetScript() const { return (ScriptPtr); }

  // Lifecycle delegation to hosted script (implemented in .cpp)
  void DelegateInit();
  void DelegateStart();
  void DelegateFixedTick(float FixedDeltaT);
  void DelegateTick(float Alpha, float DeltaT);
  void DelegateLateTick();
  void DelegateStop();
  void DelegateTerm();

  size_t GetSize() const override { return (sizeof(ScriptComponent)); }
};

/**
 * Animator - Controls animation playback on a node.
 *
 * Stores animation name, speed, play state, and current time.
 * A future AnimationSystem will process these each frame.
 */
class Animator : public Component
{
public:
  static constexpr uint32_t TypeID = 0; // Assigned by ComponentFactory at registration

  char AnimationName[64];
  float Speed;
  bool IsPlaying;
  float CurrentTime;

  Animator()
    : Speed(1.0f)
    , IsPlaying(false)
    , CurrentTime(0.0f)
  {
    AnimationName[0] = '\0';
  }

  size_t GetSize() const override { return (sizeof(Animator)); }
};

/**
 * ParticleEmitter - Controls particle emission on a node.
 *
 * Stores emission parameters for a future ParticleSystem.
 */
class ParticleEmitter : public Component
{
public:
  static constexpr uint32_t TypeID = 0; // Assigned by ComponentFactory at registration

  uint32_t MaxParticles;
  float EmissionRate;
  float Lifetime;
  float Speed;
  bool IsEmitting;

  ParticleEmitter()
    : MaxParticles(100)
    , EmissionRate(10.0f)
    , Lifetime(2.0f)
    , Speed(1.0f)
    , IsEmitting(false)
  {}

  size_t GetSize() const override { return (sizeof(ParticleEmitter)); }
};

//=============================================================================
// Factory Registration
//=============================================================================

// Forward declaration
class ComponentFactory;

/**
 * Register all standard component types with the given ComponentFactory.
 * Called during AxScene plugin LoadPlugin.
 */
void RegisterStandardComponents(ComponentFactory* Factory);
