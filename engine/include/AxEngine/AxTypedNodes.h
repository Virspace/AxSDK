#pragma once

/**
 * AxTypedNodes.h - Typed Node Subclasses
 *
 * Specialized node types that inherit from Node3D or Node2D and carry
 * their data directly via Property<T> wrappers. Serializable fields use
 * Property<T> for automatic dirty marking and type safety. Runtime-only
 * fields (handles, computed state) remain plain members.
 *
 * Type dispatch uses the NodeType enum via GetType():
 *   if (node->GetType() == NodeType::MeshInstance) {
 *       MeshInstance* MI = static_cast<MeshInstance*>(node);
 *   }
 */

#include "AxEngine/AxNode.h"
#include "AxEngine/AxProperty.h"
#include "AxEngine/AxMathTypes.h"
#include "AxOpenGL/AxOpenGLTypes.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxMath.h"

#include "AxResource/AxResourceTypes.h"

#include <cstring>
#include <cstdint>
#include <string>

// Required for MeshInstance::SetPrimitiveMesh
#include "AxEngine/AxPrimitives.h"

//=============================================================================
// Physics Enums
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
 * Collider shape types.
 */
enum class ShapeType : uint32_t
{
  Box = 0,
  Sphere,
  Capsule,
  Mesh
};

//=============================================================================
// 3D Typed Nodes
//=============================================================================

/**
 * MeshInstance - Node that holds a mesh asset reference and material binding.
 *
 * Serializable: MeshPath, MaterialName, RenderLayer
 * Runtime-only: ModelHandle, MaterialHandle
 */
class MeshInstance : public Node3D
{
public:
  static constexpr NodeType StaticType = NodeType::MeshInstance;

  // Serializable properties
  Property<std::string> MeshPath;
  Property<std::string> MaterialName;
  Property<int32_t> RenderLayer;

  // Runtime state
  AxModelHandle ModelHandle = {0, 0};
  uint32_t MaterialHandle = 0;

  MeshInstance(std::string_view Name, AxHashTableAPI* TableAPI);

  /** Assign a primitive mesh, clearing MeshPath. */
  void SetPrimitiveMesh(PrimitiveMesh& Mesh)  { ModelHandle = Mesh.CreateModel(); MeshPath = ""; }
  void SetPrimitiveMesh(PrimitiveMesh&& Mesh) { SetPrimitiveMesh(Mesh); }
};

/**
 * CameraNode - Camera with perspective/orthographic projection.
 *
 * Serializable: FieldOfView, NearClipPlane, FarClipPlane, Projection
 * Runtime-only: AspectRatio, ZoomLevel (renderer sets these)
 */
class CameraNode : public Node3D
{
public:
  static constexpr NodeType StaticType = NodeType::Camera;

  // Serializable properties
  Property<float> FieldOfView;
  Property<float> NearClipPlane;
  Property<float> FarClipPlane;
  Property<int32_t> Projection;  // 0=perspective, 1=orthographic

  // Runtime state
  float AspectRatio = 16.0f / 9.0f;
  float ZoomLevel = 1.0f;

  CameraNode(std::string_view Name, AxHashTableAPI* TableAPI);

  Mat4 GetViewMatrix() const { return (GetTransform().GetViewMatrix()); }

  Mat4 GetProjectionMatrix() const
  {
    if (Projection != 0) {
      float HalfH = ZoomLevel;
      float HalfW = HalfH * AspectRatio;
      return (Mat4::Orthographic(-HalfW, HalfW, -HalfH, HalfH, NearClipPlane, FarClipPlane));
    }
    float FOVRadians = FieldOfView * (AX_PI / 180.0f);
    return (Mat4::Perspective(FOVRadians, AspectRatio, NearClipPlane, FarClipPlane));
  }
};

/**
 * LightNode - Scene light source.
 *
 * Serializable: LightType, Color, Intensity, Range, InnerConeAngle, OuterConeAngle
 * Runtime-only: none (Position is extracted from world transform by renderer)
 */
class LightNode : public Node3D
{
public:
  static constexpr NodeType StaticType = NodeType::Light;

  // Serializable properties
  Property<AxLightType> LightType;
  Property<Vec3> Color;
  Property<float> Intensity;
  Property<float> Range;
  Property<float> InnerConeAngle;
  Property<float> OuterConeAngle;

  LightNode(std::string_view Name, AxHashTableAPI* TableAPI);

  /** Build an AxLight struct for the renderer's OpenGL API. */
  AxLight BuildLight() const
  {
    AxLight L;
    memset(&L, 0, sizeof(L));
    L.Type = LightType;
    L.Color = Color;
    L.Intensity = Intensity;
    L.Range = Range;
    L.InnerConeAngle = InnerConeAngle;
    L.OuterConeAngle = OuterConeAngle;
    strncpy(L.Name, std::string(GetName()).c_str(), sizeof(L.Name) - 1);
    L.Name[sizeof(L.Name) - 1] = '\0';
    return (L);
  }
};

/**
 * RigidBodyNode - Physics body data.
 *
 * Serializable: Mass, Drag, BodyKind
 * Runtime-only: LinearVelocity, AngularVelocity
 */
class RigidBodyNode : public Node3D
{
public:
  static constexpr NodeType StaticType = NodeType::RigidBody;

  // Serializable properties
  Property<float> Mass;
  Property<float> Drag;
  Property<BodyType> BodyKind;

  // Runtime state
  Vec3 LinearVelocity = Vec3::Zero();
  Vec3 AngularVelocity = Vec3::Zero();

  RigidBodyNode(std::string_view Name, AxHashTableAPI* TableAPI);
};

/**
 * ColliderNode - Collision shape data.
 *
 * Serializable: Shape, Radius, Height, IsTrigger
 * Runtime-only: Extents
 */
class ColliderNode : public Node3D
{
public:
  static constexpr NodeType StaticType = NodeType::Collider;

  // Serializable properties
  Property<ShapeType> Shape;
  Property<float> Radius;
  Property<float> Height;
  Property<bool> IsTrigger;

  // Runtime state
  Vec3 Extents = Vec3(0.5f, 0.5f, 0.5f);

  ColliderNode(std::string_view Name, AxHashTableAPI* TableAPI);
};

/**
 * AudioSourceNode - Audio playback data.
 *
 * Serializable: ClipPath, Volume, Pitch
 * Runtime-only: IsLooping, Is3D, MaxDistance
 */
class AudioSourceNode : public Node3D
{
public:
  static constexpr NodeType StaticType = NodeType::AudioSource;

  // Serializable properties
  Property<std::string> ClipPath;
  Property<float> Volume;
  Property<float> Pitch;

  // Runtime state
  bool IsLooping = false;
  bool Is3D = true;
  float MaxDistance = 100.0f;

  AudioSourceNode(std::string_view Name, AxHashTableAPI* TableAPI);
};

/**
 * AudioListenerNode - Marker node for spatial audio receiver.
 *
 * Receives 3D audio based on position. Typically a child of the camera.
 */
class AudioListenerNode : public Node3D
{
public:
  static constexpr NodeType StaticType = NodeType::AudioListener;

  AudioListenerNode(std::string_view Name, AxHashTableAPI* TableAPI);
};

/**
 * AnimatorNode - Controls animation playback on a node.
 *
 * Serializable: AnimationName, Speed
 * Runtime-only: IsPlaying, CurrentTime
 */
class AnimatorNode : public Node3D
{
public:
  static constexpr NodeType StaticType = NodeType::Animator;

  // Serializable properties
  Property<std::string> AnimationName;
  Property<float> Speed;

  // Runtime state
  bool IsPlaying = false;
  float CurrentTime = 0.0f;

  AnimatorNode(std::string_view Name, AxHashTableAPI* TableAPI);
};

/**
 * ParticleEmitterNode - Controls particle emission on a node.
 *
 * Serializable: MaxParticles, EmissionRate, Lifetime, Speed
 * Runtime-only: IsEmitting
 */
class ParticleEmitterNode : public Node3D
{
public:
  static constexpr NodeType StaticType = NodeType::ParticleEmitter;

  // Serializable properties
  Property<uint32_t> MaxParticles;
  Property<float> EmissionRate;
  Property<float> Lifetime;
  Property<float> Speed;

  // Runtime state
  bool IsEmitting = false;

  ParticleEmitterNode(std::string_view Name, AxHashTableAPI* TableAPI);
};

//=============================================================================
// 2D Typed Nodes
//=============================================================================

/**
 * SpriteNode - 2D rendering node for sprites.
 *
 * Serializable: TexturePath, SortOrder
 * Runtime-only: TextureHandle, Color
 */
class SpriteNode : public Node2D
{
public:
  static constexpr NodeType StaticType = NodeType::Sprite;

  // Serializable properties
  Property<std::string> TexturePath;
  Property<int32_t> SortOrder;

  // Runtime state
  uint32_t TextureHandle = 0;
  Vec4 Color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);

  SpriteNode(std::string_view Name, AxHashTableAPI* TableAPI);
};
