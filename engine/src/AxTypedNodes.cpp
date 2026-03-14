/**
 * AxTypedNodes.cpp - Typed Node Subclass Constructors and Methods
 *
 * Each typed node constructor sets Type_ to the correct NodeType enum value
 * and initializes data fields to defaults matching the original component defaults.
 */

#include "AxEngine/AxTypedNodes.h"

#include <cstring>

//=============================================================================
// MeshInstance
//=============================================================================

MeshInstance::MeshInstance(std::string_view Name, AxHashTableAPI* TableAPI)
  : Node3D(Name, TableAPI)
  , ModelHandle{0, 0}
  , MaterialHandle(0)
  , RenderLayer(0)
{
  Type_ = NodeType::MeshInstance;
}

//=============================================================================
// CameraNode
//=============================================================================

CameraNode::CameraNode(std::string_view Name, AxHashTableAPI* TableAPI)
  : Node3D(Name, TableAPI)
{
  Type_ = NodeType::Camera;
  memset(&Camera, 0, sizeof(AxCamera));
  Camera.FieldOfView = 60.0f;
  Camera.NearClipPlane = 0.1f;
  Camera.FarClipPlane = 1000.0f;
  Camera.AspectRatio = 16.0f / 9.0f;
  Camera.IsOrthographic = false;
  Camera.ZoomLevel = 1.0f;
}

//=============================================================================
// LightNode
//=============================================================================

LightNode::LightNode(std::string_view Name, AxHashTableAPI* TableAPI)
  : Node3D(Name, TableAPI)
{
  Type_ = NodeType::Light;
  memset(&Light, 0, sizeof(AxLight));
  Light.Type = AX_LIGHT_TYPE_POINT;
  Light.Color = {1.0f, 1.0f, 1.0f};
  Light.Intensity = 1.0f;
  Light.Range = 0.0f;
  Light.InnerConeAngle = 0.5f;
  Light.OuterConeAngle = 1.0f;

  // Copy the node name into the AxLight::Name field
  strncpy(Light.Name, std::string(Name).c_str(), sizeof(Light.Name) - 1);
  Light.Name[sizeof(Light.Name) - 1] = '\0';
}

//=============================================================================
// RigidBodyNode
//=============================================================================

RigidBodyNode::RigidBodyNode(std::string_view Name, AxHashTableAPI* TableAPI)
  : Node3D(Name, TableAPI)
  , Mass(1.0f)
  , Drag(0.0f)
  , Type(BodyType::Dynamic)
{
  Type_ = NodeType::RigidBody;
  LinearVelocity = {0.0f, 0.0f, 0.0f};
  AngularVelocity = {0.0f, 0.0f, 0.0f};
}

//=============================================================================
// ColliderNode
//=============================================================================

ColliderNode::ColliderNode(std::string_view Name, AxHashTableAPI* TableAPI)
  : Node3D(Name, TableAPI)
  , Shape(ShapeType::Box)
  , Radius(0.5f)
  , Height(1.0f)
  , IsTrigger(false)
{
  Type_ = NodeType::Collider;
  Extents = {0.5f, 0.5f, 0.5f};
}

//=============================================================================
// AudioSourceNode
//=============================================================================

AudioSourceNode::AudioSourceNode(std::string_view Name, AxHashTableAPI* TableAPI)
  : Node3D(Name, TableAPI)
  , Volume(1.0f)
  , Pitch(1.0f)
  , IsLooping(false)
  , Is3D(true)
  , MaxDistance(100.0f)
{
  Type_ = NodeType::AudioSource;
}

//=============================================================================
// AudioListenerNode
//=============================================================================

AudioListenerNode::AudioListenerNode(std::string_view Name, AxHashTableAPI* TableAPI)
  : Node3D(Name, TableAPI)
{
  Type_ = NodeType::AudioListener;
}

//=============================================================================
// AnimatorNode
//=============================================================================

AnimatorNode::AnimatorNode(std::string_view Name, AxHashTableAPI* TableAPI)
  : Node3D(Name, TableAPI)
  , Speed(1.0f)
  , IsPlaying(false)
  , CurrentTime(0.0f)
{
  Type_ = NodeType::Animator;
}

//=============================================================================
// ParticleEmitterNode
//=============================================================================

ParticleEmitterNode::ParticleEmitterNode(std::string_view Name, AxHashTableAPI* TableAPI)
  : Node3D(Name, TableAPI)
  , MaxParticles(100)
  , EmissionRate(10.0f)
  , Lifetime(2.0f)
  , Speed(1.0f)
  , IsEmitting(false)
{
  Type_ = NodeType::ParticleEmitter;
}

//=============================================================================
// SpriteNode
//=============================================================================

SpriteNode::SpriteNode(std::string_view Name, AxHashTableAPI* TableAPI)
  : Node2D(Name, TableAPI)
  , TextureHandle(0)
  , SortOrder(0)
{
  Type_ = NodeType::Sprite;
  Color = {1.0f, 1.0f, 1.0f, 1.0f};
}
