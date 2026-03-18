/**
 * AxTypedNodes.cpp - Typed Node Subclass Constructors and Methods
 *
 * Each typed node constructor initializes Property<T> members with the owner
 * pointer (this) and default values. Runtime-only fields are initialized
 * directly in the initializer list.
 */

#include "AxEngine/AxTypedNodes.h"
#include "AxEngine/AxPropertyReflection.h"

#include <cstring>

//=============================================================================
// Enum Tables
//=============================================================================

static const EnumEntry CameraProjectionEntries[] = {
  {"perspective",  0},
  {"orthographic", 1},
};

static const EnumEntry LightTypeEntries[] = {
  {"directional", AX_LIGHT_TYPE_DIRECTIONAL},
  {"point",       AX_LIGHT_TYPE_POINT},
  {"spot",        AX_LIGHT_TYPE_SPOT},
};

static const EnumEntry BodyTypeEntries[] = {
  {"static",    static_cast<int32_t>(BodyType::Static)},
  {"dynamic",   static_cast<int32_t>(BodyType::Dynamic)},
  {"kinematic", static_cast<int32_t>(BodyType::Kinematic)},
};

static const EnumEntry ShapeTypeEntries[] = {
  {"box",     static_cast<int32_t>(ShapeType::Box)},
  {"sphere",  static_cast<int32_t>(ShapeType::Sphere)},
  {"capsule", static_cast<int32_t>(ShapeType::Capsule)},
  {"mesh",    static_cast<int32_t>(ShapeType::Mesh)},
};

//=============================================================================
// Property Registrations
//=============================================================================

AX_REGISTER_PROPERTIES(MeshInstance,
  AX_PROP("mesh",        &MeshInstance::MeshPath,      std::string("")).Category("Mesh"),
  AX_PROP("material",    &MeshInstance::MaterialName,   std::string("")).Category("Mesh"),
  AX_PROP("renderLayer", &MeshInstance::RenderLayer,    0).Category("Rendering")
);

AX_REGISTER_PROPERTIES(CameraNode,
  AX_PROP("fov",        &CameraNode::FieldOfView,    60.0f).Range(1.0f, 179.0f, 1.0f).Category("Projection"),
  AX_PROP("near",       &CameraNode::NearClipPlane,  0.1f).Range(0.001f, 1000.0f, 0.1f).Category("Projection"),
  AX_PROP("far",        &CameraNode::FarClipPlane,   1000.0f).Range(1.0f, 100000.0f, 10.0f).Category("Projection"),
  AX_PROP("projection", &CameraNode::Projection, 0).Enum(CameraProjectionEntries).Category("Projection")
);

AX_REGISTER_PROPERTIES(LightNode,
  AX_PROP("type",      &LightNode::LightType,      AX_LIGHT_TYPE_POINT).Enum(LightTypeEntries).Category("Light"),
  AX_PROP("color",     &LightNode::Color,           Vec3(1.0f, 1.0f, 1.0f)).Category("Light"),
  AX_PROP("intensity", &LightNode::Intensity,       1.0f).Range(0.0f, 100.0f, 0.1f).Category("Light"),
  AX_PROP("range",     &LightNode::Range,           0.0f).Range(0.0f, 1000.0f, 1.0f).Category("Light"),
  AX_PROP("innerCone", &LightNode::InnerConeAngle,  0.5f).Range(0.0f, 90.0f, 1.0f).Category("Light"),
  AX_PROP("outerCone", &LightNode::OuterConeAngle,  1.0f).Range(0.0f, 90.0f, 1.0f).Category("Light")
);

AX_REGISTER_PROPERTIES(RigidBodyNode,
  AX_PROP("mass",     &RigidBodyNode::Mass,     1.0f).Range(0.0f, 10000.0f, 0.1f).Category("Physics"),
  AX_PROP("drag",     &RigidBodyNode::Drag,     0.0f).Range(0.0f, 100.0f, 0.01f).Category("Physics"),
  AX_PROP("bodyType", &RigidBodyNode::BodyKind, BodyType::Dynamic).Enum(BodyTypeEntries).Category("Physics")
);

AX_REGISTER_PROPERTIES(ColliderNode,
  AX_PROP("shape",     &ColliderNode::Shape,     ShapeType::Box).Enum(ShapeTypeEntries).Category("Collision"),
  AX_PROP("radius",    &ColliderNode::Radius,    0.5f).Range(0.0f, 1000.0f, 0.1f).Category("Collision"),
  AX_PROP("height",    &ColliderNode::Height,    1.0f).Range(0.0f, 1000.0f, 0.1f).Category("Collision"),
  AX_PROP("isTrigger", &ColliderNode::IsTrigger, false).Category("Collision")
);

AX_REGISTER_PROPERTIES(AudioSourceNode,
  AX_PROP("clip",   &AudioSourceNode::ClipPath, std::string("")).Category("Audio"),
  AX_PROP("volume", &AudioSourceNode::Volume,   1.0f).Range(0.0f, 10.0f, 0.01f).Category("Audio"),
  AX_PROP("pitch",  &AudioSourceNode::Pitch,    1.0f).Range(0.1f, 10.0f, 0.01f).Category("Audio")
);

AX_REGISTER_PROPERTIES(AnimatorNode,
  AX_PROP("animation", &AnimatorNode::AnimationName, std::string("")).Category("Animation"),
  AX_PROP("speed",     &AnimatorNode::Speed,         1.0f).Range(0.0f, 10.0f, 0.1f).Category("Animation")
);

AX_REGISTER_PROPERTIES(ParticleEmitterNode,
  AX_PROP("maxParticles",  &ParticleEmitterNode::MaxParticles,  100u).Category("Particles"),
  AX_PROP("emissionRate",  &ParticleEmitterNode::EmissionRate,  10.0f).Range(0.0f, 10000.0f, 1.0f).Category("Particles"),
  AX_PROP("lifetime",      &ParticleEmitterNode::Lifetime,      2.0f).Range(0.0f, 100.0f, 0.1f).Category("Particles"),
  AX_PROP("speed",         &ParticleEmitterNode::Speed,         1.0f).Range(0.0f, 100.0f, 0.1f).Category("Particles")
);

AX_REGISTER_PROPERTIES(SpriteNode,
  AX_PROP("texture",   &SpriteNode::TexturePath, std::string("")).Category("Sprite"),
  AX_PROP("sortOrder", &SpriteNode::SortOrder,   0).Category("Sprite")
);

//=============================================================================
// MeshInstance
//=============================================================================

MeshInstance::MeshInstance(std::string_view Name, AxHashTableAPI* TableAPI)
  : Node3D(Name, TableAPI)
  , PROP(MeshPath, "")
  , PROP(MaterialName, "")
  , PROP(RenderLayer, 0)
{
  Type_ = NodeType::MeshInstance;
}

//=============================================================================
// CameraNode
//=============================================================================

CameraNode::CameraNode(std::string_view Name, AxHashTableAPI* TableAPI)
  : Node3D(Name, TableAPI)
  , PROP(FieldOfView, 60.0f)
  , PROP(NearClipPlane, 0.1f)
  , PROP(FarClipPlane, 1000.0f)
  , PROP(Projection, 0)
{
  Type_ = NodeType::Camera;
}

//=============================================================================
// LightNode
//=============================================================================

LightNode::LightNode(std::string_view Name, AxHashTableAPI* TableAPI)
  : Node3D(Name, TableAPI)
  , PROP(LightType, AX_LIGHT_TYPE_POINT)
  , PROP(Color, Vec3(1.0f, 1.0f, 1.0f))
  , PROP(Intensity, 1.0f)
  , PROP(Range, 0.0f)
  , PROP(InnerConeAngle, 0.5f)
  , PROP(OuterConeAngle, 1.0f)
{
  Type_ = NodeType::Light;
}

//=============================================================================
// RigidBodyNode
//=============================================================================

RigidBodyNode::RigidBodyNode(std::string_view Name, AxHashTableAPI* TableAPI)
  : Node3D(Name, TableAPI)
  , PROP(Mass, 1.0f)
  , PROP(Drag, 0.0f)
  , PROP(BodyKind, BodyType::Dynamic)
{
  Type_ = NodeType::RigidBody;
}

//=============================================================================
// ColliderNode
//=============================================================================

ColliderNode::ColliderNode(std::string_view Name, AxHashTableAPI* TableAPI)
  : Node3D(Name, TableAPI)
  , PROP(Shape, ShapeType::Box)
  , PROP(Radius, 0.5f)
  , PROP(Height, 1.0f)
  , PROP(IsTrigger, false)
{
  Type_ = NodeType::Collider;
}

//=============================================================================
// AudioSourceNode
//=============================================================================

AudioSourceNode::AudioSourceNode(std::string_view Name, AxHashTableAPI* TableAPI)
  : Node3D(Name, TableAPI)
  , PROP(ClipPath, "")
  , PROP(Volume, 1.0f)
  , PROP(Pitch, 1.0f)
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
  , PROP(AnimationName, "")
  , PROP(Speed, 1.0f)
{
  Type_ = NodeType::Animator;
}

//=============================================================================
// ParticleEmitterNode
//=============================================================================

ParticleEmitterNode::ParticleEmitterNode(std::string_view Name, AxHashTableAPI* TableAPI)
  : Node3D(Name, TableAPI)
  , PROP(MaxParticles, 100)
  , PROP(EmissionRate, 10.0f)
  , PROP(Lifetime, 2.0f)
  , PROP(Speed, 1.0f)
{
  Type_ = NodeType::ParticleEmitter;
}

//=============================================================================
// SpriteNode
//=============================================================================

SpriteNode::SpriteNode(std::string_view Name, AxHashTableAPI* TableAPI)
  : Node2D(Name, TableAPI)
  , PROP(TexturePath, "")
  , PROP(SortOrder, 0)
{
  Type_ = NodeType::Sprite;
}
