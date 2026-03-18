/**
 * AxPropertyReflectionTests.cpp - Tests for Property<T> and PropertyRegistry
 *
 * Tests Property<T> wrapper (construct, get, set, dirty marking, equality),
 * PropertyRegistry (register, lookup, find by name), and enum string mapping.
 */

#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxHashTable.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxAllocatorAPI.h"
#include "AxEngine/AxNode.h"
#include "AxEngine/AxProperty.h"
#include "AxEngine/AxPropertyReflection.h"
#include "AxEngine/AxTypedNodes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PropertyTest : public testing::Test
{
protected:
  void SetUp() override
  {
    AxonInitGlobalAPIRegistry();
    AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);
    TableAPI_ = (AxHashTableAPI*)AxonGlobalAPIRegistry->Get(AXON_HASH_TABLE_API_NAME);
    ASSERT_NE(TableAPI_, nullptr);
  }

  void TearDown() override
  {
    AxonTermGlobalAPIRegistry();
  }

  AxHashTableAPI* TableAPI_{nullptr};
};

//=============================================================================
// Property<T> — Construction and Get
//=============================================================================

TEST_F(PropertyTest, ConstructWithDefault_StoresValue)
{
  Node3D Owner("TestNode", TableAPI_);
  Property<float> P(&Owner, 60.0f);
  EXPECT_FLOAT_EQ(P, 60.0f);
}

TEST_F(PropertyTest, ImplicitConversion_ReturnsValue)
{
  Node3D Owner("TestNode", TableAPI_);
  Property<float> P(&Owner, 42.5f);
  float F = P;
  EXPECT_FLOAT_EQ(F, 42.5f);
}

TEST_F(PropertyTest, Get_ReturnsSameAsConversion)
{
  Node3D Owner("TestNode", TableAPI_);
  Property<int32_t> P(&Owner, 7);
  int32_t A = P;
  int32_t B = P;
  EXPECT_EQ(A, B);
}

//=============================================================================
// Property<T> — Assignment and Dirty Marking
//=============================================================================

TEST_F(PropertyTest, Assignment_UpdatesValue)
{
  Node3D Owner("TestNode", TableAPI_);
  Property<float> P(&Owner, 60.0f);
  P = 90.0f;
  EXPECT_FLOAT_EQ(P, 90.0f);
}

TEST_F(PropertyTest, Assignment_MarksDirty)
{
  Node3D Owner("TestNode", TableAPI_);
  EXPECT_FALSE(Owner.IsPropertiesDirty());

  Property<float> P(&Owner, 60.0f);
  P = 90.0f;
  EXPECT_TRUE(Owner.IsPropertiesDirty());
}

TEST_F(PropertyTest, Assignment_NullOwner_DoesNotCrash)
{
  Property<float> P(nullptr, 0.0f);
  P = 42.0f;
  EXPECT_FLOAT_EQ(P, 42.0f);
}

TEST_F(PropertyTest, ClearPropertiesDirty_Resets)
{
  Node3D Owner("TestNode", TableAPI_);
  Property<float> P(&Owner, 0.0f);
  P = 1.0f;
  EXPECT_TRUE(Owner.IsPropertiesDirty());
  Owner.ClearPropertiesDirty();
  EXPECT_FALSE(Owner.IsPropertiesDirty());
}

//=============================================================================
// Property<T> — Equality
//=============================================================================

TEST_F(PropertyTest, Equality_MatchesDefault)
{
  Node3D Owner("TestNode", TableAPI_);
  Property<float> P(&Owner, 60.0f);
  EXPECT_TRUE(P == 60.0f);
  EXPECT_FALSE(P != 60.0f);
}

TEST_F(PropertyTest, Equality_AfterChange)
{
  Node3D Owner("TestNode", TableAPI_);
  Property<float> P(&Owner, 60.0f);
  P = 90.0f;
  EXPECT_FALSE(P == 60.0f);
  EXPECT_TRUE(P != 60.0f);
  EXPECT_TRUE(P == 90.0f);
}

TEST_F(PropertyTest, BoolProperty_DefaultAndChange)
{
  Node3D Owner("TestNode", TableAPI_);
  Property<bool> P(&Owner, false);
  EXPECT_TRUE(P == false);
  P = true;
  EXPECT_TRUE(P == true);
  EXPECT_TRUE(Owner.IsPropertiesDirty());
}

TEST_F(PropertyTest, Int32Property_DefaultAndChange)
{
  Node3D Owner("TestNode", TableAPI_);
  Property<int32_t> P(&Owner, 0);
  EXPECT_TRUE(P == 0);
  P = 42;
  EXPECT_TRUE(P == 42);
}

//=============================================================================
// Property<Vec3/Vec4/Quat> — Compound Type Specializations
//=============================================================================

TEST_F(PropertyTest, Vec3_ComponentRead)
{
  Node3D Owner("TestNode", TableAPI_);
  Property<Vec3> P(&Owner, Vec3(1.0f, 2.0f, 3.0f));
  EXPECT_FLOAT_EQ(P.X, 1.0f);
  EXPECT_FLOAT_EQ(P.Y, 2.0f);
  EXPECT_FLOAT_EQ(P.Z, 3.0f);
}

TEST_F(PropertyTest, Vec3_ComponentWrite)
{
  Node3D Owner("TestNode", TableAPI_);
  Property<Vec3> P(&Owner, Vec3(1.0f, 2.0f, 3.0f));
  P.X = 10.0f;
  EXPECT_FLOAT_EQ(P.X, 10.0f);
  EXPECT_FLOAT_EQ(P.Y, 2.0f);
  EXPECT_FLOAT_EQ(P.Z, 3.0f);
}

TEST_F(PropertyTest, Vec3_WholeAssignment_MarksDirty)
{
  Node3D Owner("TestNode", TableAPI_);
  Property<Vec3> P(&Owner, Vec3(0.0f, 0.0f, 0.0f));
  EXPECT_FALSE(Owner.IsPropertiesDirty());
  P = Vec3(1.0f, 0.5f, 0.0f);
  EXPECT_TRUE(Owner.IsPropertiesDirty());
  EXPECT_FLOAT_EQ(P.X, 1.0f);
  EXPECT_FLOAT_EQ(P.Y, 0.5f);
  EXPECT_FLOAT_EQ(P.Z, 0.0f);
}

TEST_F(PropertyTest, Vec3_CopyOut)
{
  Node3D Owner("TestNode", TableAPI_);
  Property<Vec3> P(&Owner, Vec3(1.0f, 2.0f, 3.0f));
  Vec3 Copy = P;
  EXPECT_FLOAT_EQ(Copy.X, 1.0f);
  EXPECT_FLOAT_EQ(Copy.Y, 2.0f);
  EXPECT_FLOAT_EQ(Copy.Z, 3.0f);
}

TEST_F(PropertyTest, Vec3_ReferenceBinding)
{
  Node3D Owner("TestNode", TableAPI_);
  Property<Vec3> P(&Owner, Vec3(5.0f, 6.0f, 7.0f));
  const Vec3& Ref = P;
  EXPECT_FLOAT_EQ(Ref.X, 5.0f);
  EXPECT_FLOAT_EQ(Ref.Y, 6.0f);
  EXPECT_FLOAT_EQ(Ref.Z, 7.0f);
}

TEST_F(PropertyTest, Vec3_Equality)
{
  Node3D Owner("TestNode", TableAPI_);
  Property<Vec3> P(&Owner, Vec3(1.0f, 2.0f, 3.0f));
  EXPECT_TRUE(P == Vec3(1.0f, 2.0f, 3.0f));
  EXPECT_FALSE(P != Vec3(1.0f, 2.0f, 3.0f));
  P = Vec3(4.0f, 5.0f, 6.0f);
  EXPECT_FALSE(P == Vec3(1.0f, 2.0f, 3.0f));
  EXPECT_TRUE(P != Vec3(1.0f, 2.0f, 3.0f));
}

TEST_F(PropertyTest, Vec4_ComponentReadWrite)
{
  Node3D Owner("TestNode", TableAPI_);
  Property<Vec4> P(&Owner, Vec4(1.0f, 1.0f, 1.0f, 1.0f));
  EXPECT_FLOAT_EQ(P.W, 1.0f);
  P.W = 0.5f;
  EXPECT_FLOAT_EQ(P.W, 0.5f);
}

TEST_F(PropertyTest, Vec4_WholeAssignment_MarksDirty)
{
  Node3D Owner("TestNode", TableAPI_);
  Property<Vec4> P(&Owner, Vec4(0.0f, 0.0f, 0.0f, 0.0f));
  EXPECT_FALSE(Owner.IsPropertiesDirty());
  P = Vec4(1.0f, 0.0f, 0.0f, 1.0f);
  EXPECT_TRUE(Owner.IsPropertiesDirty());
}

TEST_F(PropertyTest, Quat_ComponentReadWrite)
{
  Node3D Owner("TestNode", TableAPI_);
  Property<Quat> P(&Owner, Quat(0.0f, 0.0f, 0.0f, 1.0f));
  EXPECT_FLOAT_EQ(P.W, 1.0f);
  EXPECT_FLOAT_EQ(P.X, 0.0f);
  P.X = 0.707f;
  EXPECT_FLOAT_EQ(P.X, 0.707f);
}

TEST_F(PropertyTest, Quat_WholeAssignment_MarksDirty)
{
  Node3D Owner("TestNode", TableAPI_);
  Property<Quat> P(&Owner, Quat(0.0f, 0.0f, 0.0f, 1.0f));
  EXPECT_FALSE(Owner.IsPropertiesDirty());
  P = Quat(0.0f, 0.707f, 0.0f, 0.707f);
  EXPECT_TRUE(Owner.IsPropertiesDirty());
}

TEST_F(PropertyTest, Vec3_OnTypedNode_ComponentAccess)
{
  LightNode LN("TestLight", TableAPI_);
  EXPECT_FLOAT_EQ(LN.Color.X, 1.0f);
  EXPECT_FLOAT_EQ(LN.Color.Y, 1.0f);
  EXPECT_FLOAT_EQ(LN.Color.Z, 1.0f);

  LN.Color.X = 0.5f;
  EXPECT_FLOAT_EQ(LN.Color.X, 0.5f);

  LN.Color = Vec3(0.2f, 0.4f, 0.6f);
  EXPECT_FLOAT_EQ(LN.Color.X, 0.2f);
  EXPECT_FLOAT_EQ(LN.Color.Y, 0.4f);
  EXPECT_FLOAT_EQ(LN.Color.Z, 0.6f);
}

TEST_F(PropertyTest, Vec3_Arithmetic_ThroughProperty)
{
  Node3D Owner("TestNode", TableAPI_);
  Property<Vec3> P(&Owner, Vec3(1.0f, 2.0f, 3.0f));

  Vec3 Result = P + Vec3(10.0f, 20.0f, 30.0f);
  EXPECT_FLOAT_EQ(Result.X, 11.0f);
  EXPECT_FLOAT_EQ(Result.Y, 22.0f);
  EXPECT_FLOAT_EQ(Result.Z, 33.0f);

  Vec3 Scaled = P * 2.0f;
  EXPECT_FLOAT_EQ(Scaled.X, 2.0f);
  EXPECT_FLOAT_EQ(Scaled.Y, 4.0f);
  EXPECT_FLOAT_EQ(Scaled.Z, 6.0f);
}

//=============================================================================
// PropertyRegistry — Registration and Lookup
//=============================================================================

static const PropDescriptor TestDescs[] = {
  {"alpha", PropType::Float, 0, PropFlags::Serialize | PropFlags::EditorVisible, nullptr, {.FloatDefault = 1.0f}, 0.0f, 1.0f, 0.01f, nullptr, 0, nullptr},
  {"beta",  PropType::Int32, 4, PropFlags::Serialize, "TestCat", {.IntDefault = 0}, 0.0f, 100.0f, 1.0f, nullptr, 0, nullptr},
};

TEST(PropertyRegistryTest, Register_GetProperties_ReturnsDescriptors)
{
  PropertyRegistry& Reg = PropertyRegistry::Get();
  // Use a NodeType that won't conflict with real registrations
  // NodeType::Node3D is unlikely to have real properties registered
  Reg.Register(NodeType::Node3D, TestDescs, 2);

  uint32_t Count = 0;
  const PropDescriptor* Props = Reg.GetProperties(NodeType::Node3D, &Count);
  ASSERT_NE(Props, nullptr);
  EXPECT_EQ(Count, 2u);
  EXPECT_STREQ(Props[0].Name, "alpha");
  EXPECT_STREQ(Props[1].Name, "beta");
}

TEST(PropertyRegistryTest, GetProperties_UnregisteredType_ReturnsNull)
{
  PropertyRegistry& Reg = PropertyRegistry::Get();
  uint32_t Count = 99;
  const PropDescriptor* Props = Reg.GetProperties(NodeType::Root, &Count);
  EXPECT_EQ(Props, nullptr);
  EXPECT_EQ(Count, 0u);
}

TEST(PropertyRegistryTest, FindProperty_ByName)
{
  PropertyRegistry& Reg = PropertyRegistry::Get();
  Reg.Register(NodeType::Node3D, TestDescs, 2);

  const PropDescriptor* Found = Reg.FindProperty(NodeType::Node3D, "beta");
  ASSERT_NE(Found, nullptr);
  EXPECT_STREQ(Found->Name, "beta");
  EXPECT_EQ(Found->Type, PropType::Int32);
}

TEST(PropertyRegistryTest, FindProperty_Unknown_ReturnsNull)
{
  PropertyRegistry& Reg = PropertyRegistry::Get();
  Reg.Register(NodeType::Node3D, TestDescs, 2);

  const PropDescriptor* Found = Reg.FindProperty(NodeType::Node3D, "nonexistent");
  EXPECT_EQ(Found, nullptr);
}

TEST(PropertyRegistryTest, DescriptorFlags_Correct)
{
  EXPECT_TRUE(HasFlag(TestDescs[0].Flags, PropFlags::Serialize));
  EXPECT_TRUE(HasFlag(TestDescs[0].Flags, PropFlags::EditorVisible));
  EXPECT_FALSE(HasFlag(TestDescs[0].Flags, PropFlags::ReadOnly));
  EXPECT_TRUE(HasFlag(TestDescs[1].Flags, PropFlags::Serialize));
  EXPECT_FALSE(HasFlag(TestDescs[1].Flags, PropFlags::EditorVisible));
}

TEST(PropertyRegistryTest, DescriptorCategory)
{
  EXPECT_EQ(TestDescs[0].Category, nullptr);
  EXPECT_STREQ(TestDescs[1].Category, "TestCat");
}

TEST(PropertyRegistryTest, DescriptorNumericHints)
{
  EXPECT_FLOAT_EQ(TestDescs[0].Min, 0.0f);
  EXPECT_FLOAT_EQ(TestDescs[0].Max, 1.0f);
  EXPECT_FLOAT_EQ(TestDescs[0].Step, 0.01f);
}

//=============================================================================
// Enum String Mapping
//=============================================================================

static const EnumEntry TestEnumEntries[] = {
  {"directional", 0},
  {"point",       1},
  {"spot",        2},
};

TEST(EnumMappingTest, EnumToString_ValidValue)
{
  const char* Str = EnumToString(TestEnumEntries, 3, 1);
  ASSERT_NE(Str, nullptr);
  EXPECT_STREQ(Str, "point");
}

TEST(EnumMappingTest, EnumToString_FirstEntry)
{
  const char* Str = EnumToString(TestEnumEntries, 3, 0);
  ASSERT_NE(Str, nullptr);
  EXPECT_STREQ(Str, "directional");
}

TEST(EnumMappingTest, EnumToString_LastEntry)
{
  const char* Str = EnumToString(TestEnumEntries, 3, 2);
  ASSERT_NE(Str, nullptr);
  EXPECT_STREQ(Str, "spot");
}

TEST(EnumMappingTest, EnumToString_InvalidValue_ReturnsNull)
{
  const char* Str = EnumToString(TestEnumEntries, 3, 99);
  EXPECT_EQ(Str, nullptr);
}

TEST(EnumMappingTest, StringToEnum_ValidName)
{
  int32_t Value = -1;
  bool Found = StringToEnum(TestEnumEntries, 3, "spot", &Value);
  EXPECT_TRUE(Found);
  EXPECT_EQ(Value, 2);
}

TEST(EnumMappingTest, StringToEnum_FirstEntry)
{
  int32_t Value = -1;
  bool Found = StringToEnum(TestEnumEntries, 3, "directional", &Value);
  EXPECT_TRUE(Found);
  EXPECT_EQ(Value, 0);
}

TEST(EnumMappingTest, StringToEnum_InvalidName_ReturnsFalse)
{
  int32_t Value = -1;
  bool Found = StringToEnum(TestEnumEntries, 3, "invalid", &Value);
  EXPECT_FALSE(Found);
  EXPECT_EQ(Value, -1);
}

TEST(EnumMappingTest, RoundTrip_IntToStringToInt)
{
  const char* Str = EnumToString(TestEnumEntries, 3, 1);
  ASSERT_NE(Str, nullptr);

  int32_t Value = -1;
  bool Found = StringToEnum(TestEnumEntries, 3, Str, &Value);
  EXPECT_TRUE(Found);
  EXPECT_EQ(Value, 1);
}

//=============================================================================
// AX_REGISTER_PROPERTIES — Node Type Registration Tests
//=============================================================================

TEST(RegistrationTest, MeshInstance_HasCorrectPropertyCount)
{
  uint32_t Count = 0;
  const PropDescriptor* Props = PropertyRegistry::Get().GetProperties(NodeType::MeshInstance, &Count);
  ASSERT_NE(Props, nullptr);
  EXPECT_EQ(Count, 3u);
}

TEST(RegistrationTest, MeshInstance_PropertyNames)
{
  uint32_t Count = 0;
  const PropDescriptor* Props = PropertyRegistry::Get().GetProperties(NodeType::MeshInstance, &Count);
  ASSERT_EQ(Count, 3u);
  EXPECT_STREQ(Props[0].Name, "mesh");
  EXPECT_STREQ(Props[1].Name, "material");
  EXPECT_STREQ(Props[2].Name, "renderLayer");
}

TEST(RegistrationTest, MeshInstance_PropertyTypes)
{
  uint32_t Count = 0;
  const PropDescriptor* Props = PropertyRegistry::Get().GetProperties(NodeType::MeshInstance, &Count);
  ASSERT_EQ(Count, 3u);
  EXPECT_EQ(Props[0].Type, PropType::String);
  EXPECT_EQ(Props[1].Type, PropType::String);
  EXPECT_EQ(Props[2].Type, PropType::Int32);
}

TEST(RegistrationTest, MeshInstance_Category)
{
  uint32_t Count = 0;
  const PropDescriptor* Props = PropertyRegistry::Get().GetProperties(NodeType::MeshInstance, &Count);
  ASSERT_EQ(Count, 3u);
  EXPECT_STREQ(Props[0].Category, "Mesh");
  EXPECT_STREQ(Props[1].Category, "Mesh");
  EXPECT_STREQ(Props[2].Category, "Rendering");
}

TEST(RegistrationTest, CameraNode_HasCorrectPropertyCount)
{
  uint32_t Count = 0;
  const PropDescriptor* Props = PropertyRegistry::Get().GetProperties(NodeType::Camera, &Count);
  ASSERT_NE(Props, nullptr);
  EXPECT_EQ(Count, 4u);
}

TEST(RegistrationTest, CameraNode_PropertyTypes)
{
  uint32_t Count = 0;
  const PropDescriptor* Props = PropertyRegistry::Get().GetProperties(NodeType::Camera, &Count);
  ASSERT_EQ(Count, 4u);
  EXPECT_EQ(Props[0].Type, PropType::Float);   // fov
  EXPECT_EQ(Props[1].Type, PropType::Float);   // near
  EXPECT_EQ(Props[2].Type, PropType::Float);   // far
  EXPECT_EQ(Props[3].Type, PropType::Enum);    // projection
}

TEST(RegistrationTest, CameraNode_RangeHints)
{
  const PropDescriptor* FOV = PropertyRegistry::Get().FindProperty(NodeType::Camera, "fov");
  ASSERT_NE(FOV, nullptr);
  EXPECT_FLOAT_EQ(FOV->Min, 1.0f);
  EXPECT_FLOAT_EQ(FOV->Max, 179.0f);
  EXPECT_FLOAT_EQ(FOV->Step, 1.0f);
}

TEST(RegistrationTest, CameraNode_Defaults)
{
  const PropDescriptor* FOV = PropertyRegistry::Get().FindProperty(NodeType::Camera, "fov");
  ASSERT_NE(FOV, nullptr);
  EXPECT_FLOAT_EQ(FOV->FloatDefault, 60.0f);

  const PropDescriptor* Near = PropertyRegistry::Get().FindProperty(NodeType::Camera, "near");
  ASSERT_NE(Near, nullptr);
  EXPECT_FLOAT_EQ(Near->FloatDefault, 0.1f);

  const PropDescriptor* Proj = PropertyRegistry::Get().FindProperty(NodeType::Camera, "projection");
  ASSERT_NE(Proj, nullptr);
  EXPECT_EQ(Proj->IntDefault, 0);  // perspective
  EXPECT_NE(Proj->EnumEntries, nullptr);
  EXPECT_EQ(Proj->EnumCount, 2u);
}

TEST(RegistrationTest, LightNode_HasCorrectPropertyCount)
{
  uint32_t Count = 0;
  const PropDescriptor* Props = PropertyRegistry::Get().GetProperties(NodeType::Light, &Count);
  ASSERT_NE(Props, nullptr);
  EXPECT_EQ(Count, 6u);
}

TEST(RegistrationTest, LightNode_TypeProperty_IsEnum)
{
  const PropDescriptor* Type = PropertyRegistry::Get().FindProperty(NodeType::Light, "type");
  ASSERT_NE(Type, nullptr);
  EXPECT_EQ(Type->Type, PropType::Enum);
  ASSERT_NE(Type->EnumEntries, nullptr);
  EXPECT_EQ(Type->EnumCount, 3u);
}

TEST(RegistrationTest, LightNode_EnumEntries_Correct)
{
  const PropDescriptor* Type = PropertyRegistry::Get().FindProperty(NodeType::Light, "type");
  ASSERT_NE(Type, nullptr);
  ASSERT_EQ(Type->EnumCount, 3u);
  EXPECT_STREQ(Type->EnumEntries[0].Name, "directional");
  EXPECT_STREQ(Type->EnumEntries[1].Name, "point");
  EXPECT_STREQ(Type->EnumEntries[2].Name, "spot");
  EXPECT_EQ(Type->EnumEntries[0].Value, AX_LIGHT_TYPE_DIRECTIONAL);
  EXPECT_EQ(Type->EnumEntries[1].Value, AX_LIGHT_TYPE_POINT);
  EXPECT_EQ(Type->EnumEntries[2].Value, AX_LIGHT_TYPE_SPOT);
}

TEST(RegistrationTest, LightNode_ColorProperty_IsVec3)
{
  const PropDescriptor* Color = PropertyRegistry::Get().FindProperty(NodeType::Light, "color");
  ASSERT_NE(Color, nullptr);
  EXPECT_EQ(Color->Type, PropType::Vec3);
}

TEST(RegistrationTest, RigidBodyNode_HasCorrectPropertyCount)
{
  uint32_t Count = 0;
  const PropDescriptor* Props = PropertyRegistry::Get().GetProperties(NodeType::RigidBody, &Count);
  ASSERT_NE(Props, nullptr);
  EXPECT_EQ(Count, 3u);
}

TEST(RegistrationTest, RigidBodyNode_BodyTypeEnum)
{
  const PropDescriptor* BT = PropertyRegistry::Get().FindProperty(NodeType::RigidBody, "bodyType");
  ASSERT_NE(BT, nullptr);
  EXPECT_EQ(BT->Type, PropType::Enum);
  EXPECT_EQ(BT->EnumCount, 3u);
  EXPECT_STREQ(BT->EnumEntries[0].Name, "static");
  EXPECT_STREQ(BT->EnumEntries[1].Name, "dynamic");
  EXPECT_STREQ(BT->EnumEntries[2].Name, "kinematic");
}

TEST(RegistrationTest, ColliderNode_HasCorrectPropertyCount)
{
  uint32_t Count = 0;
  const PropDescriptor* Props = PropertyRegistry::Get().GetProperties(NodeType::Collider, &Count);
  ASSERT_NE(Props, nullptr);
  EXPECT_EQ(Count, 4u);
}

TEST(RegistrationTest, ColliderNode_ShapeEnum)
{
  const PropDescriptor* Shape = PropertyRegistry::Get().FindProperty(NodeType::Collider, "shape");
  ASSERT_NE(Shape, nullptr);
  EXPECT_EQ(Shape->Type, PropType::Enum);
  EXPECT_EQ(Shape->EnumCount, 4u);
  EXPECT_STREQ(Shape->EnumEntries[0].Name, "box");
  EXPECT_STREQ(Shape->EnumEntries[3].Name, "mesh");
}

TEST(RegistrationTest, AudioSourceNode_HasCorrectPropertyCount)
{
  uint32_t Count = 0;
  const PropDescriptor* Props = PropertyRegistry::Get().GetProperties(NodeType::AudioSource, &Count);
  ASSERT_NE(Props, nullptr);
  EXPECT_EQ(Count, 3u);
}

TEST(RegistrationTest, AnimatorNode_HasCorrectPropertyCount)
{
  uint32_t Count = 0;
  const PropDescriptor* Props = PropertyRegistry::Get().GetProperties(NodeType::Animator, &Count);
  ASSERT_NE(Props, nullptr);
  EXPECT_EQ(Count, 2u);
}

TEST(RegistrationTest, ParticleEmitterNode_HasCorrectPropertyCount)
{
  uint32_t Count = 0;
  const PropDescriptor* Props = PropertyRegistry::Get().GetProperties(NodeType::ParticleEmitter, &Count);
  ASSERT_NE(Props, nullptr);
  EXPECT_EQ(Count, 4u);
}

TEST(RegistrationTest, SpriteNode_HasCorrectPropertyCount)
{
  uint32_t Count = 0;
  const PropDescriptor* Props = PropertyRegistry::Get().GetProperties(NodeType::Sprite, &Count);
  ASSERT_NE(Props, nullptr);
  EXPECT_EQ(Count, 2u);
}

TEST(RegistrationTest, AllNodeTypes_DefaultFlags_SerializeAndEditorVisible)
{
  // All registered properties should have Serialize | EditorVisible by default
  uint32_t Count = 0;
  const PropDescriptor* Props = PropertyRegistry::Get().GetProperties(NodeType::Camera, &Count);
  for (uint32_t I = 0; I < Count; ++I) {
    EXPECT_TRUE(HasFlag(Props[I].Flags, PropFlags::Serialize)) << "Property " << Props[I].Name;
    EXPECT_TRUE(HasFlag(Props[I].Flags, PropFlags::EditorVisible)) << "Property " << Props[I].Name;
  }
}

TEST(RegistrationTest, FindProperty_AcrossTypes)
{
  // "intensity" is on LightNode, not on CameraNode
  const PropDescriptor* Found = PropertyRegistry::Get().FindProperty(NodeType::Light, "intensity");
  ASSERT_NE(Found, nullptr);
  EXPECT_EQ(Found->Type, PropType::Float);

  const PropDescriptor* NotFound = PropertyRegistry::Get().FindProperty(NodeType::Camera, "intensity");
  EXPECT_EQ(NotFound, nullptr);
}

TEST(RegistrationTest, Offset_NonZero_ForNonFirstProperty)
{
  uint32_t Count = 0;
  const PropDescriptor* Props = PropertyRegistry::Get().GetProperties(NodeType::Camera, &Count);
  ASSERT_GE(Count, 2u);
  // Second property should have a different offset than the first
  EXPECT_NE(Props[0].Offset, Props[1].Offset);
}

//=============================================================================
// Type-Erased Get/Set Tests
//=============================================================================

TEST_F(PropertyTest, TypeErasedGetSet_Float)
{
  CameraNode Cam("TestCam", TableAPI_);
  const PropDescriptor* Desc = PropertyRegistry::Get().FindProperty(NodeType::Camera, "fov");
  ASSERT_NE(Desc, nullptr);

  EXPECT_FLOAT_EQ(GetPropertyFloat(&Cam, Desc), 60.0f);
  SetPropertyFloat(&Cam, Desc, 42.5f);
  EXPECT_FLOAT_EQ(GetPropertyFloat(&Cam, Desc), 42.5f);
  EXPECT_FLOAT_EQ(Cam.FieldOfView, 42.5f);
}

TEST_F(PropertyTest, TypeErasedGetSet_Int32)
{
  MeshInstance MI("TestMesh", TableAPI_);
  const PropDescriptor* Desc = PropertyRegistry::Get().FindProperty(NodeType::MeshInstance, "renderLayer");
  ASSERT_NE(Desc, nullptr);

  EXPECT_EQ(GetPropertyInt32(&MI, Desc), 0);
  SetPropertyInt32(&MI, Desc, 7);
  EXPECT_EQ(GetPropertyInt32(&MI, Desc), 7);
  EXPECT_TRUE(MI.RenderLayer == 7);
}

TEST_F(PropertyTest, TypeErasedGetSet_Bool)
{
  ColliderNode Col("TestCol", TableAPI_);
  const PropDescriptor* Desc = PropertyRegistry::Get().FindProperty(NodeType::Collider, "isTrigger");
  ASSERT_NE(Desc, nullptr);

  EXPECT_FALSE(GetPropertyBool(&Col, Desc));
  SetPropertyBool(&Col, Desc, true);
  EXPECT_TRUE(GetPropertyBool(&Col, Desc));
  EXPECT_TRUE(Col.IsTrigger == true);
}

TEST_F(PropertyTest, TypeErasedGetSet_String)
{
  MeshInstance MI("TestMesh", TableAPI_);
  const PropDescriptor* Desc = PropertyRegistry::Get().FindProperty(NodeType::MeshInstance, "mesh");
  ASSERT_NE(Desc, nullptr);

  EXPECT_EQ(GetPropertyString(&MI, Desc), "");
  SetPropertyString(&MI, Desc, "test/model.gltf");
  EXPECT_EQ(GetPropertyString(&MI, Desc), "test/model.gltf");
  EXPECT_TRUE(MI.MeshPath == "test/model.gltf");
}

TEST_F(PropertyTest, TypeErasedGetSet_Vec3)
{
  LightNode LN("TestLight", TableAPI_);
  const PropDescriptor* Desc = PropertyRegistry::Get().FindProperty(NodeType::Light, "color");
  ASSERT_NE(Desc, nullptr);

  Vec3 Color = GetPropertyVec3(&LN, Desc);
  EXPECT_FLOAT_EQ(Color.X, 1.0f);
  EXPECT_FLOAT_EQ(Color.Y, 1.0f);
  EXPECT_FLOAT_EQ(Color.Z, 1.0f);

  SetPropertyVec3(&LN, Desc, Vec3(1.0f, 0.5f, 0.0f));
  Color = GetPropertyVec3(&LN, Desc);
  EXPECT_FLOAT_EQ(Color.X, 1.0f);
  EXPECT_FLOAT_EQ(Color.Y, 0.5f);
  EXPECT_FLOAT_EQ(Color.Z, 0.0f);
}

TEST_F(PropertyTest, TypeErasedGetSet_Enum)
{
  LightNode LN("TestLight", TableAPI_);
  const PropDescriptor* Desc = PropertyRegistry::Get().FindProperty(NodeType::Light, "type");
  ASSERT_NE(Desc, nullptr);

  EXPECT_EQ(GetPropertyInt32(&LN, Desc), AX_LIGHT_TYPE_POINT);
  SetPropertyEnum(&LN, Desc, "spot");
  EXPECT_EQ(GetPropertyInt32(&LN, Desc), AX_LIGHT_TYPE_SPOT);
  EXPECT_TRUE(LN.LightType == AX_LIGHT_TYPE_SPOT);
}

//=============================================================================
// Scene Parse/Serialize Round-Trip Integration Tests
//=============================================================================

#include "AxEngine/AxSceneParser.h"
#include "AxEngine/AxSceneTree.h"

class ReflectionRoundTripTest : public testing::Test
{
protected:
  void SetUp() override
  {
    AxonInitGlobalAPIRegistry();
    AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);
    TableAPI_ = (AxHashTableAPI*)AxonGlobalAPIRegistry->Get(AXON_HASH_TABLE_API_NAME);
    AllocatorAPI_ = (AxAllocatorAPI*)AxonGlobalAPIRegistry->Get(AXON_ALLOCATOR_API_NAME);
    ASSERT_NE(TableAPI_, nullptr);
    ASSERT_NE(AllocatorAPI_, nullptr);
    Parser_.Init(AxonGlobalAPIRegistry);
  }

  void TearDown() override
  {
    Parser_.Term();
  }

  AxHashTableAPI* TableAPI_ = nullptr;
  AxAllocatorAPI* AllocatorAPI_ = nullptr;
  SceneParser Parser_;
};

TEST_F(ReflectionRoundTripTest, ParseAllNodeTypes)
{
  const char* Source = R"(scene "AllTypes" {
    node "Cam" Camera {
      fov: 90.0
      near: 0.5
      far: 500.0
      projection: orthographic
    }
    node "Sun" Light {
      type: directional
      color: 1.0, 0.9, 0.8
      intensity: 3.0
      range: 50.0
    }
    node "Cube" MeshInstance {
      mesh: "res://cube.glb"
      material: "Default"
      renderLayer: 2
    }
    node "Body" RigidBody {
      mass: 5.0
      drag: 0.1
      bodyType: kinematic
    }
    node "Col" Collider {
      shape: sphere
      radius: 2.0
      height: 3.0
      isTrigger: true
    }
    node "Snd" AudioSource {
      clip: "res://sound.wav"
      volume: 0.8
      pitch: 1.2
    }
    node "Anim" Animator {
      animation: "walk"
      speed: 2.0
    }
    node "Emit" ParticleEmitter {
      maxParticles: 500
      emissionRate: 50.0
      lifetime: 5.0
      speed: 3.0
    }
    node "Spr" Sprite {
      texture: "res://icon.png"
      sortOrder: 5
    }
  })";

  AxAllocator* Alloc = AllocatorAPI_->CreateLinear("Test", 64 * 1024);
  SceneTree* Tree = Parser_.ParseFromString(Source, Alloc);
  ASSERT_NE(Tree, nullptr);

  // Camera
  Node* CamNode = Tree->FindNode("Cam");
  ASSERT_NE(CamNode, nullptr);
  CameraNode* Cam = static_cast<CameraNode*>(CamNode);
  EXPECT_FLOAT_EQ(Cam->FieldOfView, 90.0f);
  EXPECT_FLOAT_EQ(Cam->NearClipPlane, 0.5f);
  EXPECT_FLOAT_EQ(Cam->FarClipPlane, 500.0f);
  EXPECT_TRUE(Cam->Projection == 1);  // orthographic

  // Light
  Node* LightNode_ = Tree->FindNode("Sun");
  ASSERT_NE(LightNode_, nullptr);
  LightNode* LN = static_cast<LightNode*>(LightNode_);
  EXPECT_TRUE(LN->LightType == AX_LIGHT_TYPE_DIRECTIONAL);
  EXPECT_FLOAT_EQ(LN->Color.X, 1.0f);
  EXPECT_FLOAT_EQ(LN->Color.Y, 0.9f);
  EXPECT_FLOAT_EQ(LN->Color.Z, 0.8f);
  EXPECT_FLOAT_EQ(LN->Intensity, 3.0f);
  EXPECT_FLOAT_EQ(LN->Range, 50.0f);

  // MeshInstance
  Node* MeshNode = Tree->FindNode("Cube");
  ASSERT_NE(MeshNode, nullptr);
  MeshInstance* MI = static_cast<MeshInstance*>(MeshNode);
  EXPECT_TRUE(MI->MeshPath == "res://cube.glb");
  EXPECT_TRUE(MI->MaterialName == "Default");
  EXPECT_TRUE(MI->RenderLayer == 2);

  // RigidBody
  Node* RBNode = Tree->FindNode("Body");
  ASSERT_NE(RBNode, nullptr);
  RigidBodyNode* RB = static_cast<RigidBodyNode*>(RBNode);
  EXPECT_FLOAT_EQ(RB->Mass, 5.0f);
  EXPECT_FLOAT_EQ(RB->Drag, 0.1f);
  EXPECT_TRUE(RB->BodyKind == BodyType::Kinematic);

  // Collider
  Node* ColNode = Tree->FindNode("Col");
  ASSERT_NE(ColNode, nullptr);
  ColliderNode* Col = static_cast<ColliderNode*>(ColNode);
  EXPECT_TRUE(Col->Shape == ShapeType::Sphere);
  EXPECT_FLOAT_EQ(Col->Radius, 2.0f);
  EXPECT_FLOAT_EQ(Col->Height, 3.0f);
  EXPECT_TRUE(Col->IsTrigger == true);

  // AudioSource
  Node* SndNode = Tree->FindNode("Snd");
  ASSERT_NE(SndNode, nullptr);
  AudioSourceNode* AS = static_cast<AudioSourceNode*>(SndNode);
  EXPECT_TRUE(AS->ClipPath == "res://sound.wav");
  EXPECT_FLOAT_EQ(AS->Volume, 0.8f);
  EXPECT_FLOAT_EQ(AS->Pitch, 1.2f);

  // Animator
  Node* AnimNode = Tree->FindNode("Anim");
  ASSERT_NE(AnimNode, nullptr);
  AnimatorNode* Anim = static_cast<AnimatorNode*>(AnimNode);
  EXPECT_TRUE(Anim->AnimationName == "walk");
  EXPECT_FLOAT_EQ(Anim->Speed, 2.0f);

  // ParticleEmitter
  Node* EmitNode = Tree->FindNode("Emit");
  ASSERT_NE(EmitNode, nullptr);
  ParticleEmitterNode* PE = static_cast<ParticleEmitterNode*>(EmitNode);
  EXPECT_TRUE(PE->MaxParticles == 500u);
  EXPECT_FLOAT_EQ(PE->EmissionRate, 50.0f);
  EXPECT_FLOAT_EQ(PE->Lifetime, 5.0f);
  EXPECT_FLOAT_EQ(PE->Speed, 3.0f);

  // Sprite
  Node* SprNode = Tree->FindNode("Spr");
  ASSERT_NE(SprNode, nullptr);
  SpriteNode* Spr = static_cast<SpriteNode*>(SprNode);
  EXPECT_TRUE(Spr->TexturePath == "res://icon.png");
  EXPECT_TRUE(Spr->SortOrder == 5);

  delete Tree;
}

TEST_F(ReflectionRoundTripTest, SerializeAndParseBack_AllNodeTypes)
{
  // Create a scene with non-default values
  SceneTree* Tree = new SceneTree(TableAPI_, nullptr);
  Tree->Name = "RoundTrip";

  CameraNode* Cam = static_cast<CameraNode*>(Tree->CreateNode("Cam", NodeType::Camera, nullptr));
  Cam->FieldOfView = 90.0f;
  Cam->Projection = 1;  // orthographic

  LightNode* LN = static_cast<LightNode*>(Tree->CreateNode("Sun", NodeType::Light, nullptr));
  LN->LightType = AX_LIGHT_TYPE_SPOT;
  LN->Color = Vec3(1.0f, 0.5f, 0.0f);
  LN->Intensity = 5.0f;
  LN->Range = 200.0f;
  LN->InnerConeAngle = 30.0f;
  LN->OuterConeAngle = 45.0f;

  MeshInstance* MI = static_cast<MeshInstance*>(Tree->CreateNode("Cube", NodeType::MeshInstance, nullptr));
  MI->MeshPath = "res://cube.glb";
  MI->MaterialName = "Metal";

  RigidBodyNode* RB = static_cast<RigidBodyNode*>(Tree->CreateNode("Body", NodeType::RigidBody, nullptr));
  RB->Mass = 10.0f;
  RB->BodyKind = BodyType::Kinematic;

  ColliderNode* Col = static_cast<ColliderNode*>(Tree->CreateNode("Col", NodeType::Collider, nullptr));
  Col->Shape = ShapeType::Capsule;
  Col->Radius = 1.5f;
  Col->IsTrigger = true;

  AudioSourceNode* AS = static_cast<AudioSourceNode*>(Tree->CreateNode("Snd", NodeType::AudioSource, nullptr));
  AS->ClipPath = "res://boom.wav";
  AS->Volume = 0.5f;

  AnimatorNode* Anim = static_cast<AnimatorNode*>(Tree->CreateNode("Anim", NodeType::Animator, nullptr));
  Anim->AnimationName = "run";
  Anim->Speed = 1.5f;

  ParticleEmitterNode* PE = static_cast<ParticleEmitterNode*>(Tree->CreateNode("Emit", NodeType::ParticleEmitter, nullptr));
  PE->MaxParticles = 200u;
  PE->EmissionRate = 25.0f;

  SpriteNode* Spr = static_cast<SpriteNode*>(Tree->CreateNode("Spr", NodeType::Sprite, nullptr));
  Spr->TexturePath = "res://icon.png";
  Spr->SortOrder = 3;

  // Serialize
  std::string Output = Parser_.SaveSceneToString(Tree);
  delete Tree;

  // Parse back
  AxAllocator* Alloc = AllocatorAPI_->CreateLinear("Test", 64 * 1024);
  SceneTree* Parsed = Parser_.ParseFromString(Output.c_str(), Alloc);
  ASSERT_NE(Parsed, nullptr);

  // Verify round-trip
  CameraNode* PCam = static_cast<CameraNode*>(Parsed->FindNode("Cam"));
  ASSERT_NE(PCam, nullptr);
  EXPECT_FLOAT_EQ(PCam->FieldOfView, 90.0f);
  EXPECT_TRUE(PCam->Projection == 1);

  LightNode* PLN = static_cast<LightNode*>(Parsed->FindNode("Sun"));
  ASSERT_NE(PLN, nullptr);
  EXPECT_TRUE(PLN->LightType == AX_LIGHT_TYPE_SPOT);
  EXPECT_FLOAT_EQ(PLN->Color.Y, 0.5f);
  EXPECT_FLOAT_EQ(PLN->Intensity, 5.0f);
  EXPECT_FLOAT_EQ(PLN->Range, 200.0f);
  EXPECT_FLOAT_EQ(PLN->InnerConeAngle, 30.0f);
  EXPECT_FLOAT_EQ(PLN->OuterConeAngle, 45.0f);

  MeshInstance* PMI = static_cast<MeshInstance*>(Parsed->FindNode("Cube"));
  ASSERT_NE(PMI, nullptr);
  EXPECT_TRUE(PMI->MeshPath == "res://cube.glb");
  EXPECT_TRUE(PMI->MaterialName == "Metal");

  RigidBodyNode* PRB = static_cast<RigidBodyNode*>(Parsed->FindNode("Body"));
  ASSERT_NE(PRB, nullptr);
  EXPECT_FLOAT_EQ(PRB->Mass, 10.0f);
  EXPECT_TRUE(PRB->BodyKind == BodyType::Kinematic);

  ColliderNode* PCol = static_cast<ColliderNode*>(Parsed->FindNode("Col"));
  ASSERT_NE(PCol, nullptr);
  EXPECT_TRUE(PCol->Shape == ShapeType::Capsule);
  EXPECT_FLOAT_EQ(PCol->Radius, 1.5f);
  EXPECT_TRUE(PCol->IsTrigger == true);

  AudioSourceNode* PAS = static_cast<AudioSourceNode*>(Parsed->FindNode("Snd"));
  ASSERT_NE(PAS, nullptr);
  EXPECT_TRUE(PAS->ClipPath == "res://boom.wav");
  EXPECT_FLOAT_EQ(PAS->Volume, 0.5f);

  AnimatorNode* PAnim = static_cast<AnimatorNode*>(Parsed->FindNode("Anim"));
  ASSERT_NE(PAnim, nullptr);
  EXPECT_TRUE(PAnim->AnimationName == "run");
  EXPECT_FLOAT_EQ(PAnim->Speed, 1.5f);

  ParticleEmitterNode* PPE = static_cast<ParticleEmitterNode*>(Parsed->FindNode("Emit"));
  ASSERT_NE(PPE, nullptr);
  EXPECT_TRUE(PPE->MaxParticles == 200u);
  EXPECT_FLOAT_EQ(PPE->EmissionRate, 25.0f);

  SpriteNode* PSpr = static_cast<SpriteNode*>(Parsed->FindNode("Spr"));
  ASSERT_NE(PSpr, nullptr);
  EXPECT_TRUE(PSpr->TexturePath == "res://icon.png");
  EXPECT_TRUE(PSpr->SortOrder == 3);

  delete Parsed;
}

TEST_F(ReflectionRoundTripTest, SkipIfDefault_NoPropertiesWritten)
{
  // Create a camera with all defaults
  SceneTree* Tree = new SceneTree(TableAPI_, nullptr);
  Tree->Name = "Defaults";
  Tree->CreateNode("DefaultCam", NodeType::Camera, nullptr);

  std::string Output = Parser_.SaveSceneToString(Tree);
  // Should have the node header but no property lines
  EXPECT_NE(Output.find("node \"DefaultCam\" Camera"), std::string::npos);
  EXPECT_EQ(Output.find("fov:"), std::string::npos);
  EXPECT_EQ(Output.find("near:"), std::string::npos);
  EXPECT_EQ(Output.find("far:"), std::string::npos);
  EXPECT_EQ(Output.find("projection:"), std::string::npos);

  delete Tree;
}

TEST_F(ReflectionRoundTripTest, NonDefaultWritten)
{
  SceneTree* Tree = new SceneTree(TableAPI_, nullptr);
  Tree->Name = "NonDefault";
  CameraNode* Cam = static_cast<CameraNode*>(Tree->CreateNode("Cam", NodeType::Camera, nullptr));
  Cam->FieldOfView = 90.0f;

  std::string Output = Parser_.SaveSceneToString(Tree);
  EXPECT_NE(Output.find("fov: 90.0"), std::string::npos);

  delete Tree;
}

TEST_F(ReflectionRoundTripTest, EnumRoundTrip)
{
  SceneTree* Tree = new SceneTree(TableAPI_, nullptr);
  Tree->Name = "EnumTest";
  LightNode* LN = static_cast<LightNode*>(Tree->CreateNode("Light", NodeType::Light, nullptr));
  LN->LightType = AX_LIGHT_TYPE_SPOT;

  std::string Output = Parser_.SaveSceneToString(Tree);
  EXPECT_NE(Output.find("type: spot"), std::string::npos);

  // Parse back
  AxAllocator* Alloc = AllocatorAPI_->CreateLinear("Test", 64 * 1024);
  SceneTree* Parsed = Parser_.ParseFromString(Output.c_str(), Alloc);
  ASSERT_NE(Parsed, nullptr);

  LightNode* PLN = static_cast<LightNode*>(Parsed->FindNode("Light"));
  ASSERT_NE(PLN, nullptr);
  EXPECT_TRUE(PLN->LightType == AX_LIGHT_TYPE_SPOT);

  delete Tree;
  delete Parsed;
}
