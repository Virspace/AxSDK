/**
 * AxTypedNodeTests.cpp - Tests for Godot-Style Typed Node Subclasses
 *
 * Tests each typed node subclass: creation, default values, setter methods,
 * NodeType correctness, and hierarchy operations through typed nodes.
 *
 * Validates that typed nodes carry data that was previously in Component
 * objects, and that type dispatch via NodeType enum works correctly.
 */

#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxHashTable.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxMath.h"
#include "AxEngine/AxNode.h"
#include "AxEngine/AxTypedNodes.h"
#include "AxEngine/AxSceneTree.h"

#include <cstring>
#include <string>
#include <string_view>

//=============================================================================
// Test Fixture
//=============================================================================

class TypedNodeTest : public testing::Test
{
protected:
  void SetUp() override
  {
    AxonInitGlobalAPIRegistry();
    AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);
    TableAPI_ = (AxHashTableAPI*)AxonGlobalAPIRegistry->Get(AXON_HASH_TABLE_API_NAME);
    ASSERT_NE(TableAPI_, nullptr) << "Failed to get HashTableAPI from registry";
  }

  void TearDown() override
  {
    AxonTermGlobalAPIRegistry();
  }

  AxHashTableAPI* TableAPI_{nullptr};
};

//=============================================================================
// Test 1: MeshInstance creation, defaults, and setters
//=============================================================================
TEST_F(TypedNodeTest, MeshInstanceCreationAndDefaults)
{
  MeshInstance MI("TestMesh", TableAPI_);

  EXPECT_EQ(MI.GetType(), NodeType::MeshInstance);
  EXPECT_EQ(MI.GetName(), "TestMesh");
  EXPECT_EQ(MI.GetMeshPath(), "");
  EXPECT_EQ(MI.GetMaterialName(), "");
  EXPECT_EQ(MI.MaterialHandle, 0u);
  EXPECT_EQ(MI.RenderLayer, 0);

  // Test setters
  MI.SetMeshPath("models/cube.glb");
  EXPECT_EQ(MI.GetMeshPath(), "models/cube.glb");

  MI.SetMaterialName("DefaultMat");
  EXPECT_EQ(MI.GetMaterialName(), "DefaultMat");

  MI.RenderLayer = 5;
  EXPECT_EQ(MI.RenderLayer, 5);
}

//=============================================================================
// Test 2: CameraNode creation, defaults, and accessors
//=============================================================================
TEST_F(TypedNodeTest, CameraNodeCreationAndDefaults)
{
  CameraNode Cam("TestCamera", TableAPI_);

  EXPECT_EQ(Cam.GetType(), NodeType::Camera);
  EXPECT_EQ(Cam.GetName(), "TestCamera");

  // Camera should have reasonable defaults
  EXPECT_FLOAT_EQ(Cam.GetFOV(), 60.0f);
  EXPECT_FLOAT_EQ(Cam.GetNear(), 0.1f);
  EXPECT_FLOAT_EQ(Cam.GetFar(), 1000.0f);
  EXPECT_FALSE(Cam.IsOrthographic());

  // Test setters
  Cam.SetFOV(90.0f);
  EXPECT_FLOAT_EQ(Cam.GetFOV(), 90.0f);

  Cam.SetNear(0.5f);
  EXPECT_FLOAT_EQ(Cam.GetNear(), 0.5f);

  Cam.SetFar(500.0f);
  EXPECT_FLOAT_EQ(Cam.GetFar(), 500.0f);

  Cam.SetOrthographic(true);
  EXPECT_TRUE(Cam.IsOrthographic());
}

//=============================================================================
// Test 3: LightNode creation, defaults, and accessors
//=============================================================================
TEST_F(TypedNodeTest, LightNodeCreationAndDefaults)
{
  LightNode LN("TestLight", TableAPI_);

  EXPECT_EQ(LN.GetType(), NodeType::Light);
  EXPECT_EQ(LN.GetName(), "TestLight");

  // Light defaults to point light
  EXPECT_EQ(LN.GetLightType(), AX_LIGHT_TYPE_POINT);
  EXPECT_FLOAT_EQ(LN.GetIntensity(), 1.0f);

  // Test setters
  LN.SetLightType(AX_LIGHT_TYPE_POINT);
  EXPECT_EQ(LN.GetLightType(), AX_LIGHT_TYPE_POINT);

  LN.SetIntensity(2.5f);
  EXPECT_FLOAT_EQ(LN.GetIntensity(), 2.5f);

  LN.SetColor({1.0f, 0.5f, 0.0f});
  AxVec3 Color = LN.GetColor();
  EXPECT_FLOAT_EQ(Color.R, 1.0f);
  EXPECT_FLOAT_EQ(Color.G, 0.5f);
  EXPECT_FLOAT_EQ(Color.B, 0.0f);

  // Test direct Light struct access
  LN.Light.Range = 100.0f;
  EXPECT_FLOAT_EQ(LN.Light.Range, 100.0f);
}

//=============================================================================
// Test 4: RigidBodyNode creation and defaults
//=============================================================================
TEST_F(TypedNodeTest, RigidBodyNodeCreationAndDefaults)
{
  RigidBodyNode RB("TestBody", TableAPI_);

  EXPECT_EQ(RB.GetType(), NodeType::RigidBody);
  EXPECT_EQ(RB.GetName(), "TestBody");
  EXPECT_FLOAT_EQ(RB.Mass, 1.0f);
  EXPECT_FLOAT_EQ(RB.Drag, 0.0f);
  EXPECT_EQ(RB.Type, BodyType::Dynamic);
  EXPECT_FLOAT_EQ(RB.LinearVelocity.X, 0.0f);
  EXPECT_FLOAT_EQ(RB.LinearVelocity.Y, 0.0f);
  EXPECT_FLOAT_EQ(RB.LinearVelocity.Z, 0.0f);

  // Modify fields
  RB.Mass = 10.0f;
  RB.Type = BodyType::Kinematic;
  EXPECT_FLOAT_EQ(RB.Mass, 10.0f);
  EXPECT_EQ(RB.Type, BodyType::Kinematic);
}

//=============================================================================
// Test 5: ColliderNode creation and defaults
//=============================================================================
TEST_F(TypedNodeTest, ColliderNodeCreationAndDefaults)
{
  ColliderNode Col("TestCollider", TableAPI_);

  EXPECT_EQ(Col.GetType(), NodeType::Collider);
  EXPECT_EQ(Col.GetName(), "TestCollider");
  EXPECT_EQ(Col.Shape, ShapeType::Box);
  EXPECT_FLOAT_EQ(Col.Radius, 0.5f);
  EXPECT_FLOAT_EQ(Col.Height, 1.0f);
  EXPECT_FALSE(Col.IsTrigger);

  Col.Shape = ShapeType::Sphere;
  Col.Radius = 2.0f;
  Col.IsTrigger = true;
  EXPECT_EQ(Col.Shape, ShapeType::Sphere);
  EXPECT_FLOAT_EQ(Col.Radius, 2.0f);
  EXPECT_TRUE(Col.IsTrigger);
}

//=============================================================================
// Test 6: AudioSourceNode creation and defaults
//=============================================================================
TEST_F(TypedNodeTest, AudioSourceNodeCreationAndDefaults)
{
  AudioSourceNode AS("TestAudio", TableAPI_);

  EXPECT_EQ(AS.GetType(), NodeType::AudioSource);
  EXPECT_EQ(AS.GetName(), "TestAudio");
  EXPECT_EQ(AS.GetClipPath(), "");
  EXPECT_FLOAT_EQ(AS.Volume, 1.0f);
  EXPECT_FLOAT_EQ(AS.Pitch, 1.0f);
  EXPECT_FALSE(AS.IsLooping);
  EXPECT_TRUE(AS.Is3D);
  EXPECT_FLOAT_EQ(AS.MaxDistance, 100.0f);

  AS.SetClipPath("sounds/explosion.wav");
  AS.Volume = 0.5f;
  AS.IsLooping = true;
  EXPECT_EQ(AS.GetClipPath(), "sounds/explosion.wav");
  EXPECT_FLOAT_EQ(AS.Volume, 0.5f);
  EXPECT_TRUE(AS.IsLooping);
}

//=============================================================================
// Test 7: AudioListenerNode creation
//=============================================================================
TEST_F(TypedNodeTest, AudioListenerNodeCreation)
{
  AudioListenerNode AL("TestListener", TableAPI_);

  EXPECT_EQ(AL.GetType(), NodeType::AudioListener);
  EXPECT_EQ(AL.GetName(), "TestListener");
}

//=============================================================================
// Test 8: AnimatorNode creation and defaults
//=============================================================================
TEST_F(TypedNodeTest, AnimatorNodeCreationAndDefaults)
{
  AnimatorNode Anim("TestAnimator", TableAPI_);

  EXPECT_EQ(Anim.GetType(), NodeType::Animator);
  EXPECT_EQ(Anim.GetName(), "TestAnimator");
  EXPECT_EQ(Anim.GetAnimationName(), "");
  EXPECT_FLOAT_EQ(Anim.Speed, 1.0f);
  EXPECT_FALSE(Anim.IsPlaying);
  EXPECT_FLOAT_EQ(Anim.CurrentTime, 0.0f);

  Anim.SetAnimationName("idle");
  Anim.Speed = 2.0f;
  Anim.IsPlaying = true;
  EXPECT_EQ(Anim.GetAnimationName(), "idle");
  EXPECT_FLOAT_EQ(Anim.Speed, 2.0f);
  EXPECT_TRUE(Anim.IsPlaying);
}

//=============================================================================
// Test 9: ParticleEmitterNode creation and defaults
//=============================================================================
TEST_F(TypedNodeTest, ParticleEmitterNodeCreationAndDefaults)
{
  ParticleEmitterNode PE("TestEmitter", TableAPI_);

  EXPECT_EQ(PE.GetType(), NodeType::ParticleEmitter);
  EXPECT_EQ(PE.GetName(), "TestEmitter");
  EXPECT_EQ(PE.MaxParticles, 100u);
  EXPECT_FLOAT_EQ(PE.EmissionRate, 10.0f);
  EXPECT_FLOAT_EQ(PE.Lifetime, 2.0f);
  EXPECT_FLOAT_EQ(PE.Speed, 1.0f);
  EXPECT_FALSE(PE.IsEmitting);

  PE.MaxParticles = 500;
  PE.EmissionRate = 50.0f;
  PE.IsEmitting = true;
  EXPECT_EQ(PE.MaxParticles, 500u);
  EXPECT_FLOAT_EQ(PE.EmissionRate, 50.0f);
  EXPECT_TRUE(PE.IsEmitting);
}

//=============================================================================
// Test 10: SpriteNode creation and defaults
//=============================================================================
TEST_F(TypedNodeTest, SpriteNodeCreationAndDefaults)
{
  SpriteNode Spr("TestSprite", TableAPI_);

  EXPECT_EQ(Spr.GetType(), NodeType::Sprite);
  EXPECT_EQ(Spr.GetName(), "TestSprite");
  EXPECT_EQ(Spr.GetTexturePath(), "");
  EXPECT_EQ(Spr.TextureHandle, 0u);
  EXPECT_EQ(Spr.SortOrder, 0);

  // SpriteNode inherits from Node2D
  EXPECT_FLOAT_EQ(Spr.GetTransform().Scale.X, 1.0f);

  Spr.SetTexturePath("textures/hero.png");
  Spr.SortOrder = 10;
  Spr.Color = {1.0f, 0.0f, 0.0f, 1.0f};
  EXPECT_EQ(Spr.GetTexturePath(), "textures/hero.png");
  EXPECT_EQ(Spr.SortOrder, 10);
  EXPECT_FLOAT_EQ(Spr.Color.R, 1.0f);
}

//=============================================================================
// Test 11: Typed nodes work in hierarchy (parent-child operations)
//=============================================================================
TEST_F(TypedNodeTest, TypedNodesWorkInHierarchy)
{
  SceneTree* Tree = new SceneTree(TableAPI_, nullptr);
  ASSERT_NE(Tree, nullptr);

  Node* MeshNode = Tree->CreateNode("Mesh", NodeType::MeshInstance, nullptr);
  Node* LightNode = Tree->CreateNode("Light", NodeType::Light, nullptr);
  Node* CamNode = Tree->CreateNode("Camera", NodeType::Camera, MeshNode);

  ASSERT_NE(MeshNode, nullptr);
  ASSERT_NE(LightNode, nullptr);
  ASSERT_NE(CamNode, nullptr);

  // Verify types
  EXPECT_EQ(MeshNode->GetType(), NodeType::MeshInstance);
  EXPECT_EQ(LightNode->GetType(), NodeType::Light);
  EXPECT_EQ(CamNode->GetType(), NodeType::Camera);

  // Verify hierarchy
  EXPECT_EQ(CamNode->GetParent(), MeshNode);
  EXPECT_EQ(MeshNode->GetFirstChild(), CamNode);

  // Node count: root + 3 nodes = 4
  EXPECT_EQ(Tree->GetNodeCount(), 4u);

  // Cast to typed node and modify data
  MeshInstance* MI = static_cast<MeshInstance*>(MeshNode);
  MI->SetMeshPath("models/player.glb");
  EXPECT_EQ(MI->GetMeshPath(), "models/player.glb");

  ::LightNode* LN = static_cast<::LightNode*>(LightNode);
  LN->SetIntensity(3.0f);
  EXPECT_FLOAT_EQ(LN->GetIntensity(), 3.0f);

  delete Tree;
}

//=============================================================================
// Test 12: GetNodesByType returns correct typed node lists
//=============================================================================
TEST_F(TypedNodeTest, GetNodesByTypeReturnsCorrectLists)
{
  SceneTree* Tree = new SceneTree(TableAPI_, nullptr);
  ASSERT_NE(Tree, nullptr);

  // Create several typed nodes
  Tree->CreateNode("Mesh1", NodeType::MeshInstance, nullptr);
  Tree->CreateNode("Mesh2", NodeType::MeshInstance, nullptr);
  Tree->CreateNode("Light1", NodeType::Light, nullptr);
  Tree->CreateNode("Camera1", NodeType::Camera, nullptr);
  Tree->CreateNode("Plain", NodeType::Node3D, nullptr);

  // Query MeshInstance nodes
  uint32_t MeshCount = 0;
  Node** Meshes = Tree->GetNodesByType(NodeType::MeshInstance, &MeshCount);
  ASSERT_NE(Meshes, nullptr);
  EXPECT_EQ(MeshCount, 2u);
  EXPECT_EQ(Meshes[0]->GetName(), "Mesh1");
  EXPECT_EQ(Meshes[1]->GetName(), "Mesh2");

  // Query Light nodes
  uint32_t LightCount = 0;
  Node** Lights = Tree->GetNodesByType(NodeType::Light, &LightCount);
  ASSERT_NE(Lights, nullptr);
  EXPECT_EQ(LightCount, 1u);
  EXPECT_EQ(Lights[0]->GetName(), "Light1");

  // Query Camera nodes
  uint32_t CamCount = 0;
  Node** Cameras = Tree->GetNodesByType(NodeType::Camera, &CamCount);
  ASSERT_NE(Cameras, nullptr);
  EXPECT_EQ(CamCount, 1u);
  EXPECT_EQ(Cameras[0]->GetName(), "Camera1");

  // Query Node3D nodes (not tracked)
  uint32_t N3DCount = 0;
  Node** N3DNodes = Tree->GetNodesByType(NodeType::Node3D, &N3DCount);
  EXPECT_EQ(N3DNodes, nullptr);
  EXPECT_EQ(N3DCount, 0u);

  delete Tree;
}

//=============================================================================
// Test 13: DestroyNode removes typed nodes from tracking arrays
//=============================================================================
TEST_F(TypedNodeTest, DestroyNodeRemovesFromTypedTracking)
{
  SceneTree* Tree = new SceneTree(TableAPI_, nullptr);
  ASSERT_NE(Tree, nullptr);

  Node* Mesh1 = Tree->CreateNode("Mesh1", NodeType::MeshInstance, nullptr);
  Node* Mesh2 = Tree->CreateNode("Mesh2", NodeType::MeshInstance, nullptr);

  uint32_t Count = 0;
  Tree->GetNodesByType(NodeType::MeshInstance, &Count);
  EXPECT_EQ(Count, 2u);

  Tree->DestroyNode(Mesh1);

  Tree->GetNodesByType(NodeType::MeshInstance, &Count);
  EXPECT_EQ(Count, 1u);

  Tree->DestroyNode(Mesh2);

  Node** Result = Tree->GetNodesByType(NodeType::MeshInstance, &Count);
  EXPECT_EQ(Count, 0u);
  EXPECT_EQ(Result, nullptr);

  delete Tree;
}

//=============================================================================
// Test 14: Typed node inherits transform operations from Node3D
//=============================================================================
TEST_F(TypedNodeTest, TypedNodeInheritsTransformOperations)
{
  MeshInstance MI("TransformTest", TableAPI_);

  // Set transform
  MI.GetTransform().Translation = Vec3(1.0f, 2.0f, 3.0f);
  MI.GetTransform().Scale = Vec3(2.0f, 2.0f, 2.0f);

  EXPECT_FLOAT_EQ(MI.GetTransform().Translation.X, 1.0f);
  EXPECT_FLOAT_EQ(MI.GetTransform().Translation.Y, 2.0f);
  EXPECT_FLOAT_EQ(MI.GetTransform().Translation.Z, 3.0f);
  EXPECT_FLOAT_EQ(MI.GetTransform().Scale.X, 2.0f);
}

//=============================================================================
// Test 15: Type dispatch via static_cast pattern
//=============================================================================
TEST_F(TypedNodeTest, TypeDispatchViaStaticCast)
{
  SceneTree* Tree = new SceneTree(TableAPI_, nullptr);
  ASSERT_NE(Tree, nullptr);

  Node* NodePtr = Tree->CreateNode("DispatchTest", NodeType::MeshInstance, nullptr);
  ASSERT_NE(NodePtr, nullptr);

  // This is the canonical dispatch pattern
  if (NodePtr->GetType() == NodeType::MeshInstance) {
    MeshInstance* MI = static_cast<MeshInstance*>(NodePtr);
    MI->SetMeshPath("dispatch_test.glb");
    EXPECT_EQ(MI->GetMeshPath(), "dispatch_test.glb");
  } else {
    FAIL() << "Expected NodeType::MeshInstance";
  }

  delete Tree;
}

//=============================================================================
// Test 16: String path setters accept long paths without truncation
//=============================================================================
TEST_F(TypedNodeTest, StringPathsAcceptLongPaths)
{
  MeshInstance MI("LongPathTest", TableAPI_);

  // Build a path longer than the old 256-char limit
  std::string LongPath(512, 'a');
  LongPath = "models/" + LongPath + ".glb";
  MI.SetMeshPath(LongPath);
  EXPECT_EQ(MI.GetMeshPath(), LongPath);
  EXPECT_EQ(MI.GetMeshPath().size(), LongPath.size());

  // Same for material name (old limit was 64)
  std::string LongMat(128, 'm');
  MI.SetMaterialName(LongMat);
  EXPECT_EQ(MI.GetMaterialName(), LongMat);
}

//=============================================================================
// Test 17: String path getters return string_view
//=============================================================================
TEST_F(TypedNodeTest, StringPathGettersReturnStringView)
{
  MeshInstance MI("ViewTest", TableAPI_);
  MI.SetMeshPath("models/test.glb");

  std::string_view Path = MI.GetMeshPath();
  EXPECT_EQ(Path, "models/test.glb");

  AudioSourceNode AS("AudioView", TableAPI_);
  AS.SetClipPath("sounds/test.wav");
  EXPECT_EQ(AS.GetClipPath(), "sounds/test.wav");

  AnimatorNode Anim("AnimView", TableAPI_);
  Anim.SetAnimationName("walk");
  EXPECT_EQ(Anim.GetAnimationName(), "walk");

  SpriteNode Spr("SpriteView", TableAPI_);
  Spr.SetTexturePath("textures/test.png");
  EXPECT_EQ(Spr.GetTexturePath(), "textures/test.png");
}

//=============================================================================
// Test 18: Default string paths are empty
//=============================================================================
TEST_F(TypedNodeTest, DefaultStringPathsAreEmpty)
{
  MeshInstance MI("EmptyTest", TableAPI_);
  EXPECT_TRUE(MI.GetMeshPath().empty());
  EXPECT_TRUE(MI.GetMaterialName().empty());

  AudioSourceNode AS("EmptyAudio", TableAPI_);
  EXPECT_TRUE(AS.GetClipPath().empty());

  AnimatorNode Anim("EmptyAnim", TableAPI_);
  EXPECT_TRUE(Anim.GetAnimationName().empty());

  SpriteNode Spr("EmptySprite", TableAPI_);
  EXPECT_TRUE(Spr.GetTexturePath().empty());
}

//=============================================================================
// Node::As<T> Safe Casting Tests
//=============================================================================

TEST_F(TypedNodeTest, As_CorrectType_ReturnsTypedPointer)
{
  MeshInstance MI("Mesh", TableAPI_);
  Node* N = &MI;
  MeshInstance* Result = N->As<MeshInstance>();
  ASSERT_NE(Result, nullptr);
  EXPECT_EQ(Result, &MI);
}

TEST_F(TypedNodeTest, As_WrongType_ReturnsNullptr)
{
  MeshInstance MI("Mesh", TableAPI_);
  Node* N = &MI;
  CameraNode* Result = N->As<CameraNode>();
  EXPECT_EQ(Result, nullptr);
}

TEST_F(TypedNodeTest, As_BaseNode_ReturnsNullptr)
{
  Node N("Base", NodeType::Base, TableAPI_);
  MeshInstance* Result = N.As<MeshInstance>();
  EXPECT_EQ(Result, nullptr);
}

TEST_F(TypedNodeTest, As_Const_CorrectType_ReturnsConstPointer)
{
  MeshInstance MI("Mesh", TableAPI_);
  const Node* N = &MI;
  const MeshInstance* Result = N->As<MeshInstance>();
  ASSERT_NE(Result, nullptr);
  EXPECT_EQ(Result, &MI);
}

TEST_F(TypedNodeTest, As_Const_WrongType_ReturnsNullptr)
{
  LightNode LN("Light", TableAPI_);
  const Node* N = &LN;
  const CameraNode* Result = N->As<CameraNode>();
  EXPECT_EQ(Result, nullptr);
}

TEST_F(TypedNodeTest, As_AllTypedNodes_MatchOwnType)
{
  MeshInstance MI("MI", TableAPI_);
  CameraNode Cam("Cam", TableAPI_);
  LightNode Light("Light", TableAPI_);
  RigidBodyNode RB("RB", TableAPI_);
  ColliderNode Col("Col", TableAPI_);
  AudioSourceNode AS("AS", TableAPI_);
  AudioListenerNode AL("AL", TableAPI_);
  AnimatorNode Anim("Anim", TableAPI_);
  ParticleEmitterNode PE("PE", TableAPI_);
  SpriteNode Spr("Spr", TableAPI_);

  EXPECT_NE(((Node*)&MI)->As<MeshInstance>(), nullptr);
  EXPECT_NE(((Node*)&Cam)->As<CameraNode>(), nullptr);
  EXPECT_NE(((Node*)&Light)->As<LightNode>(), nullptr);
  EXPECT_NE(((Node*)&RB)->As<RigidBodyNode>(), nullptr);
  EXPECT_NE(((Node*)&Col)->As<ColliderNode>(), nullptr);
  EXPECT_NE(((Node*)&AS)->As<AudioSourceNode>(), nullptr);
  EXPECT_NE(((Node*)&AL)->As<AudioListenerNode>(), nullptr);
  EXPECT_NE(((Node*)&Anim)->As<AnimatorNode>(), nullptr);
  EXPECT_NE(((Node*)&PE)->As<ParticleEmitterNode>(), nullptr);
  EXPECT_NE(((Node*)&Spr)->As<SpriteNode>(), nullptr);
}

TEST_F(TypedNodeTest, As_NullptrPattern_WorksWithIf)
{
  MeshInstance MI("Mesh", TableAPI_);
  Node* N = &MI;

  bool Entered = false;
  if (auto* Result = N->As<MeshInstance>()) {
    Entered = true;
    EXPECT_EQ(Result->GetType(), NodeType::MeshInstance);
  }
  EXPECT_TRUE(Entered) << "Should enter if-block for correct type";

  if (auto* Result = N->As<CameraNode>()) {
    (void)Result;
    FAIL() << "Should not enter if-block for wrong type";
  }
}
