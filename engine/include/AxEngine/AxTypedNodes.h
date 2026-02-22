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
#include "AxEngine/AxSceneTypes.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxMath.h"

#include "AxResource/AxResourceTypes.h"

#include <cstring>
#include <cstdint>

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
  char MeshPath[256];
  AxModelHandle ModelHandle;
  char MaterialName[64];
  uint32_t MaterialHandle;
  int32_t RenderLayer;

  MeshInstance(std::string_view Name, AxHashTableAPI* TableAPI);

  void SetMeshPath(const char* Path);
  void SetMaterialName(const char* MatName);
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
  AxCamera Camera;

  CameraNode(std::string_view Name, AxHashTableAPI* TableAPI);

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
  char ClipPath[256];
  float Volume;
  float Pitch;
  bool IsLooping;
  bool Is3D;
  float MaxDistance;

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
  char AnimationName[64];
  float Speed;
  bool IsPlaying;
  float CurrentTime;

  AnimatorNode(std::string_view Name, AxHashTableAPI* TableAPI);
};

/**
 * ParticleEmitterNode - Controls particle emission on a node.
 *
 * Stores emission parameters for particle effects.
 */
class ParticleEmitterNode : public Node3D
{
public:
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
  char TexturePath[256];
  uint32_t TextureHandle;
  AxVec4 Color;
  int32_t SortOrder;

  SpriteNode(std::string_view Name, AxHashTableAPI* TableAPI);
};
