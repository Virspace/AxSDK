#pragma once

/**
 * AxTypedNodes.h - Typed Node Subclasses
 *
 * Specialized node types that inherit from Node3D or Node2D and carry
 * their data directly. Type dispatch uses the NodeType enum via GetType():
 *
 *   if (node->GetType() == NodeType::MeshInstance) {
 *       MeshInstance* MI = static_cast<MeshInstance*>(node);
 *   }
 */

#include "AxEngine/AxNode.h"
#include "AxOpenGL/AxOpenGLTypes.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxMath.h"

#include "AxResource/AxResourceTypes.h"

#include <cstring>
#include <cstdint>

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
 * Holds a path to the mesh file, a runtime model handle, material name,
 * and render layer.
 */
class MeshInstance : public Node3D
{
public:
  static constexpr NodeType StaticType = NodeType::MeshInstance;

  AxModelHandle ModelHandle;
  uint32_t MaterialHandle;
  int32_t RenderLayer;

  MeshInstance(std::string_view Name, AxHashTableAPI* TableAPI);

  std::string_view GetMeshPath() const { return (MeshPath_); }
  void SetMeshPath(std::string_view Path) { MeshPath_ = Path; }

  std::string_view GetMaterialName() const { return (MaterialName_); }
  void SetMaterialName(std::string_view MatName) { MaterialName_ = MatName; }

  /** Assign a primitive mesh, clearing MeshPath. */
  void SetPrimitiveMesh(PrimitiveMesh& Mesh)  { ModelHandle = Mesh.CreateModel(); MeshPath_.clear(); }
  void SetPrimitiveMesh(PrimitiveMesh&& Mesh) { SetPrimitiveMesh(Mesh); }

private:
  std::string MeshPath_;
  std::string MaterialName_;
};

/**
 * CameraNode - Node that wraps AxCamera for camera control.
 *
 * Exposes FOV, near/far clip planes, and projection mode through
 * the underlying AxCamera struct.
 */
class CameraNode : public Node3D
{
public:
  static constexpr NodeType StaticType = NodeType::Camera;

  AxCamera Camera;

  CameraNode(std::string_view Name, AxHashTableAPI* TableAPI);

  Mat4 GetViewMatrix() const { return (GetTransform().GetViewMatrix()); }
  Mat4 GetProjectionMatrix() const { return (Mat4(Camera.ProjectionMatrix)); }

  float GetFOV() const { return (Camera.FieldOfView); }
  void SetFOV(float FOV) { Camera.FieldOfView = FOV; }

  float GetNear() const { return (Camera.NearClipPlane); }
  void SetNear(float Near) { Camera.NearClipPlane = Near; }

  float GetFar() const { return (Camera.FarClipPlane); }
  void SetFar(float Far) { Camera.FarClipPlane = Far; }

  bool IsOrthographic() const { return (Camera.IsOrthographic); }
  void SetOrthographic(bool Ortho) { Camera.IsOrthographic = Ortho; }
};

/**
 * LightNode - Node that wraps AxLight for lighting.
 *
 * Exposes light type, color, intensity, and other properties through
 * the underlying AxLight struct.
 */
class LightNode : public Node3D
{
public:
  static constexpr NodeType StaticType = NodeType::Light;

  AxLight Light;

  LightNode(std::string_view Name, AxHashTableAPI* TableAPI);

  AxLightType GetLightType() const { return (Light.Type); }
  void SetLightType(AxLightType Type) { Light.Type = Type; }

  AxVec3 GetColor() const { return (Light.Color); }
  void SetColor(AxVec3 Color) { Light.Color = Color; }

  float GetIntensity() const { return (Light.Intensity); }
  void SetIntensity(float Intensity) { Light.Intensity = Intensity; }
};

/**
 * RigidBodyNode - Physics body data.
 *
 * Stores mass, velocity, drag, and body type for physics simulation.
 */
class RigidBodyNode : public Node3D
{
public:
  static constexpr NodeType StaticType = NodeType::RigidBody;

  float Mass;
  AxVec3 LinearVelocity;
  AxVec3 AngularVelocity;
  float Drag;
  BodyType Type;

  RigidBodyNode(std::string_view Name, AxHashTableAPI* TableAPI);
};

/**
 * ColliderNode - Collision shape data.
 *
 * Stores shape parameters for physics collision detection.
 */
class ColliderNode : public Node3D
{
public:
  static constexpr NodeType StaticType = NodeType::Collider;

  ShapeType Shape;
  AxVec3 Extents;
  float Radius;
  float Height;
  bool IsTrigger;

  ColliderNode(std::string_view Name, AxHashTableAPI* TableAPI);
};

/**
 * AudioSourceNode - Audio playback data.
 *
 * Stores clip path, volume, pitch, looping, and spatial audio settings.
 */
class AudioSourceNode : public Node3D
{
public:
  static constexpr NodeType StaticType = NodeType::AudioSource;

  float Volume;
  float Pitch;
  bool IsLooping;
  bool Is3D;
  float MaxDistance;

  AudioSourceNode(std::string_view Name, AxHashTableAPI* TableAPI);

  std::string_view GetClipPath() const { return (ClipPath_); }
  void SetClipPath(std::string_view Path) { ClipPath_ = Path; }

private:
  std::string ClipPath_;
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
 * Stores animation name, speed, play state, and current time.
 */
class AnimatorNode : public Node3D
{
public:
  static constexpr NodeType StaticType = NodeType::Animator;

  float Speed;
  bool IsPlaying;
  float CurrentTime;

  AnimatorNode(std::string_view Name, AxHashTableAPI* TableAPI);

  std::string_view GetAnimationName() const { return (AnimationName_); }
  void SetAnimationName(std::string_view AName) { AnimationName_ = AName; }

private:
  std::string AnimationName_;
};

/**
 * ParticleEmitterNode - Controls particle emission on a node.
 *
 * Stores emission parameters for particle effects.
 */
class ParticleEmitterNode : public Node3D
{
public:
  static constexpr NodeType StaticType = NodeType::ParticleEmitter;

  uint32_t MaxParticles;
  float EmissionRate;
  float Lifetime;
  float Speed;
  bool IsEmitting;

  ParticleEmitterNode(std::string_view Name, AxHashTableAPI* TableAPI);
};

//=============================================================================
// 2D Typed Nodes
//=============================================================================

/**
 * SpriteNode - 2D rendering node for sprites.
 *
 * Holds a texture path, runtime handle, tint color, and sort order.
 */
class SpriteNode : public Node2D
{
public:
  static constexpr NodeType StaticType = NodeType::Sprite;

  uint32_t TextureHandle;
  AxVec4 Color;
  int32_t SortOrder;

  SpriteNode(std::string_view Name, AxHashTableAPI* TableAPI);

  std::string_view GetTexturePath() const { return (TexturePath_); }
  void SetTexturePath(std::string_view Path) { TexturePath_ = Path; }

private:
  std::string TexturePath_;
};
