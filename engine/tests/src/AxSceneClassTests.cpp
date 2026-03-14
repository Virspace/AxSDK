/**
 * AxSceneClassTests.cpp - Tests for SceneTree (formerly AxScene) Class
 *
 * Tests the SceneTree class directly: construction, node management,
 * typed-node tracking, correct defaults, light/camera via typed nodes,
 * and engine consumer patterns.
 *
 * Component-based tests have been replaced with typed-node tests as part
 * of the migration to Godot-style typed node subclasses. Lights and cameras
 * are now tracked via GetNodesByType() instead of public arrays.
 */

#include "gtest/gtest.h"
#include "AxEngine/AxSceneTree.h"
#include "AxEngine/AxTypedNodes.h"
#include "AxOpenGL/AxOpenGLTypes.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxHashTable.h"
#include "Foundation/AxAllocator.h"
#include "Foundation/AxAPIRegistry.h"

#include <cstring>

//=============================================================================
// Test Fixture
//=============================================================================

class SceneTreeClassTest : public testing::Test
{
protected:
  void SetUp() override
  {
    AxonInitGlobalAPIRegistry();
    AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);

    TableAPI_ = static_cast<AxHashTableAPI*>(
      AxonGlobalAPIRegistry->Get(AXON_HASH_TABLE_API_NAME));
    ASSERT_NE(TableAPI_, nullptr) << "Failed to get HashTableAPI";
  }

  void TearDown() override
  {
    AxonTermGlobalAPIRegistry();
  }

  AxHashTableAPI* TableAPI_{nullptr};
};

//=============================================================================
// Test 1: SceneTree construction creates RootNode, EventBus, and correct defaults
//=============================================================================
TEST_F(SceneTreeClassTest, ConstructionCreatesRootAndDefaults)
{
  SceneTree* Tree = new SceneTree(TableAPI_, nullptr);
  ASSERT_NE(Tree, nullptr);

  // Root node should exist
  ASSERT_NE(Tree->GetRootNode(), nullptr);
  EXPECT_EQ(Tree->GetRootNode()->GetName(), "Root");
  EXPECT_EQ(Tree->GetRootNode()->GetType(), NodeType::Root);

  // EventBus should exist
  ASSERT_NE(Tree->GetEventBus(), nullptr);

  // Node count should be 1 (just the root)
  EXPECT_EQ(Tree->GetNodeCount(), 1u);

  // Scene settings should be initialized to defaults
  EXPECT_FLOAT_EQ(Tree->AmbientLight.X, 0.1f);
  EXPECT_FLOAT_EQ(Tree->AmbientLight.Y, 0.1f);
  EXPECT_FLOAT_EQ(Tree->AmbientLight.Z, 0.1f);

  EXPECT_FLOAT_EQ(Tree->Gravity.X, 0.0f);
  EXPECT_FLOAT_EQ(Tree->Gravity.Y, -9.81f);
  EXPECT_FLOAT_EQ(Tree->Gravity.Z, 0.0f);

  EXPECT_FLOAT_EQ(Tree->FogDensity, 0.0f);

  EXPECT_FLOAT_EQ(Tree->FogColor.X, 0.5f);
  EXPECT_FLOAT_EQ(Tree->FogColor.Y, 0.5f);
  EXPECT_FLOAT_EQ(Tree->FogColor.Z, 0.5f);

  delete Tree;
}

//=============================================================================
// Test 2: CreateNode/GetNodeCount -- creating nodes increments count,
//         nodes parent correctly to root or specified parent
//=============================================================================
TEST_F(SceneTreeClassTest, CreateNodeIncrementsCountAndParentsCorrectly)
{
  SceneTree* Tree = new SceneTree(TableAPI_, nullptr);
  ASSERT_NE(Tree, nullptr);

  // Create two nodes as children of root (nullptr parent defaults to root)
  Node* NodeA = Tree->CreateNode("NodeA", NodeType::Node3D, nullptr);
  ASSERT_NE(NodeA, nullptr);
  EXPECT_EQ(NodeA->GetName(), "NodeA");
  EXPECT_EQ(NodeA->GetType(), NodeType::Node3D);
  EXPECT_EQ(Tree->GetNodeCount(), 2u); // Root + NodeA

  Node* NodeB = Tree->CreateNode("NodeB", NodeType::Node2D, nullptr);
  ASSERT_NE(NodeB, nullptr);
  EXPECT_EQ(NodeB->GetType(), NodeType::Node2D);
  EXPECT_EQ(Tree->GetNodeCount(), 3u); // Root + NodeA + NodeB

  // Both should be children of root
  EXPECT_EQ(NodeA->GetParent(), static_cast<Node*>(Tree->GetRootNode()));
  EXPECT_EQ(NodeB->GetParent(), static_cast<Node*>(Tree->GetRootNode()));

  // Create a child of NodeA
  Node* ChildA1 = Tree->CreateNode("ChildA1", NodeType::Node3D, NodeA);
  ASSERT_NE(ChildA1, nullptr);
  EXPECT_EQ(ChildA1->GetParent(), NodeA);
  EXPECT_EQ(Tree->GetNodeCount(), 4u);

  delete Tree;
}

//=============================================================================
// Test 3: DestroyNode -- destroying a node with children removes the
//         entire subtree and decrements NodeCount
//=============================================================================
TEST_F(SceneTreeClassTest, DestroyNodeRemovesSubtreeAndDecrementsCount)
{
  SceneTree* Tree = new SceneTree(TableAPI_, nullptr);
  ASSERT_NE(Tree, nullptr);

  // Create a parent with two children
  Node* Parent = Tree->CreateNode("Parent", NodeType::Node3D, nullptr);
  Node* Child1 = Tree->CreateNode("Child1", NodeType::Node3D, Parent);
  Node* Child2 = Tree->CreateNode("Child2", NodeType::Node3D, Parent);

  EXPECT_EQ(Tree->GetNodeCount(), 4u); // Root + Parent + Child1 + Child2

  // Destroy the parent (should also remove children)
  Tree->DestroyNode(Parent);

  // All 3 nodes (Parent + 2 children) should be removed
  EXPECT_EQ(Tree->GetNodeCount(), 1u); // Only root remains

  // Cannot find the destroyed nodes
  EXPECT_EQ(Tree->FindNode("Parent"), nullptr);
  EXPECT_EQ(Tree->FindNode("Child1"), nullptr);
  EXPECT_EQ(Tree->FindNode("Child2"), nullptr);

  // Suppress unused variable warnings
  (void)Child1;
  (void)Child2;

  delete Tree;
}

//=============================================================================
// Test 4: CreateNode with typed nodes -- nodes appear in typed tracking lists
//=============================================================================
TEST_F(SceneTreeClassTest, CreateTypedNodeAppearsInTrackingList)
{
  SceneTree* Tree = new SceneTree(TableAPI_, nullptr);
  ASSERT_NE(Tree, nullptr);

  Node* MeshA = Tree->CreateNode("MeshA", NodeType::MeshInstance, nullptr);
  Node* MeshB = Tree->CreateNode("MeshB", NodeType::MeshInstance, nullptr);
  ASSERT_NE(MeshA, nullptr);
  ASSERT_NE(MeshB, nullptr);

  // Query typed node list for MeshInstance
  uint32_t Count = 0;
  Node** List = Tree->GetNodesByType(NodeType::MeshInstance, &Count);
  ASSERT_NE(List, nullptr);
  EXPECT_EQ(Count, 2u);
  EXPECT_EQ(List[0], MeshA);
  EXPECT_EQ(List[1], MeshB);

  // Light type should return empty
  uint32_t LightCount = 0;
  Node** LightList = Tree->GetNodesByType(NodeType::Light, &LightCount);
  EXPECT_EQ(LightList, nullptr);
  EXPECT_EQ(LightCount, 0u);

  delete Tree;
}

//=============================================================================
// Test 5: DestroyNode removes typed nodes from tracking lists
//=============================================================================
TEST_F(SceneTreeClassTest, DestroyNodeRemovesFromTrackingList)
{
  SceneTree* Tree = new SceneTree(TableAPI_, nullptr);
  ASSERT_NE(Tree, nullptr);

  Node* MeshA = Tree->CreateNode("MeshA", NodeType::MeshInstance, nullptr);
  Node* MeshB = Tree->CreateNode("MeshB", NodeType::MeshInstance, nullptr);

  // Verify both are present
  uint32_t Count = 0;
  Tree->GetNodesByType(NodeType::MeshInstance, &Count);
  EXPECT_EQ(Count, 2u);

  // Destroy MeshA
  Tree->DestroyNode(MeshA);

  // Only MeshB should remain
  uint32_t NewCount = 0;
  Node** NewList = Tree->GetNodesByType(NodeType::MeshInstance, &NewCount);
  ASSERT_NE(NewList, nullptr);
  EXPECT_EQ(NewCount, 1u);
  EXPECT_EQ(NewList[0], MeshB);

  delete Tree;
}

//=============================================================================
// Task Group 2 Regression Tests
//=============================================================================

//=============================================================================
// Test 7: Foundation types regression -- AxLight, AxCamera, AxTransform,
//         AxMaterial still exist and are usable after cleanup
//=============================================================================
TEST_F(SceneTreeClassTest, FoundationTypesStillUsableAfterCleanup)
{
  // Verify AxLight is usable: create and populate
  AxLight Light;
  memset(&Light, 0, sizeof(AxLight));
  strncpy(Light.Name, "TestLight", sizeof(Light.Name) - 1);
  Light.Type = AX_LIGHT_TYPE_DIRECTIONAL;
  Light.Color = {1.0f, 0.8f, 0.6f};
  Light.Intensity = 2.0f;

  EXPECT_STREQ(Light.Name, "TestLight");
  EXPECT_EQ(Light.Type, AX_LIGHT_TYPE_DIRECTIONAL);
  EXPECT_FLOAT_EQ(Light.Color.R, 1.0f);
  EXPECT_FLOAT_EQ(Light.Intensity, 2.0f);

  // Verify AxCamera is usable: create and populate
  AxCamera Camera;
  memset(&Camera, 0, sizeof(AxCamera));
  Camera.FieldOfView = 60.0f;
  Camera.NearClipPlane = 0.1f;
  Camera.FarClipPlane = 1000.0f;
  Camera.IsOrthographic = false;

  EXPECT_FLOAT_EQ(Camera.FieldOfView, 60.0f);
  EXPECT_FLOAT_EQ(Camera.NearClipPlane, 0.1f);
  EXPECT_FLOAT_EQ(Camera.FarClipPlane, 1000.0f);
  EXPECT_FALSE(Camera.IsOrthographic);

  // Verify AxTransform is usable: create and populate
  AxTransform Transform;
  memset(&Transform, 0, sizeof(AxTransform));
  Transform.Translation = {1.0f, 2.0f, 3.0f};
  Transform.Scale = {1.0f, 1.0f, 1.0f};
  Transform.Rotation = {0.0f, 0.0f, 0.0f, 1.0f};

  EXPECT_FLOAT_EQ(Transform.Translation.X, 1.0f);
  EXPECT_FLOAT_EQ(Transform.Translation.Y, 2.0f);
  EXPECT_FLOAT_EQ(Transform.Translation.Z, 3.0f);
  EXPECT_FLOAT_EQ(Transform.Rotation.W, 1.0f);

  // Verify AxMaterialDesc is usable (AxMaterial is opaque, but AxMaterialDesc is concrete)
  AxMaterialDesc MatDesc;
  memset(&MatDesc, 0, sizeof(AxMaterialDesc));
  strncpy(MatDesc.Name, "TestMat", sizeof(MatDesc.Name) - 1);
  MatDesc.Type = AX_MATERIAL_TYPE_PBR;
  MatDesc.PBR.MetallicFactor = 0.5f;
  MatDesc.PBR.RoughnessFactor = 0.8f;

  EXPECT_STREQ(MatDesc.Name, "TestMat");
  EXPECT_EQ(MatDesc.Type, AX_MATERIAL_TYPE_PBR);
  EXPECT_FLOAT_EQ(MatDesc.PBR.MetallicFactor, 0.5f);
  EXPECT_FLOAT_EQ(MatDesc.PBR.RoughnessFactor, 0.8f);
}

//=============================================================================
// Test 8: SceneTree stores LightNode and CameraNode via typed nodes
//=============================================================================
TEST_F(SceneTreeClassTest, SceneTreeStoresLightsAndCamerasViaTypedNodes)
{
  SceneTree* Tree = new SceneTree(TableAPI_, nullptr);
  ASSERT_NE(Tree, nullptr);

  // Initially no lights or cameras
  uint32_t LightCount = 0;
  uint32_t CameraCount = 0;
  Tree->GetNodesByType(NodeType::Light, &LightCount);
  Tree->GetNodesByType(NodeType::Camera, &CameraCount);
  EXPECT_EQ(LightCount, 0u);
  EXPECT_EQ(CameraCount, 0u);

  // Create a directional light via LightNode
  Node* SunNode = Tree->CreateNode("Sun", NodeType::Light, nullptr);
  ASSERT_NE(SunNode, nullptr);
  LightNode* Sun = static_cast<LightNode*>(SunNode);
  Sun->SetLightType(AX_LIGHT_TYPE_DIRECTIONAL);
  Sun->SetIntensity(2.5f);
  Sun->SetColor({1.0f, 0.95f, 0.8f});

  EXPECT_EQ(Sun->GetLightType(), AX_LIGHT_TYPE_DIRECTIONAL);
  EXPECT_FLOAT_EQ(Sun->GetIntensity(), 2.5f);

  // Create a point light via LightNode
  Node* LampNode = Tree->CreateNode("Lamp", NodeType::Light, nullptr);
  ASSERT_NE(LampNode, nullptr);
  LightNode* Lamp = static_cast<LightNode*>(LampNode);
  Lamp->SetLightType(AX_LIGHT_TYPE_POINT);
  Lamp->SetIntensity(0.8f);

  // Verify lights are tracked
  Node** Lights = Tree->GetNodesByType(NodeType::Light, &LightCount);
  ASSERT_NE(Lights, nullptr);
  EXPECT_EQ(LightCount, 2u);
  EXPECT_EQ(Lights[0]->GetName(), "Sun");
  EXPECT_EQ(Lights[1]->GetName(), "Lamp");

  // Find light by name via FindNode
  Node* Found = Tree->FindNode("Sun");
  ASSERT_NE(Found, nullptr);
  EXPECT_EQ(Found->GetName(), "Sun");
  EXPECT_EQ(Found->GetType(), NodeType::Light);

  // Create a camera via CameraNode
  Node* CamNode = Tree->CreateNode("MainCamera", NodeType::Camera, nullptr);
  ASSERT_NE(CamNode, nullptr);
  CameraNode* Cam = static_cast<CameraNode*>(CamNode);
  Cam->SetFOV(75.0f);
  Cam->SetNear(0.1f);
  Cam->SetFar(500.0f);

  // Set camera transform via the node's transform
  Cam->GetTransform().Translation = Vec3(0.0f, 5.0f, -10.0f);

  // Verify camera is tracked
  Node** Cameras = Tree->GetNodesByType(NodeType::Camera, &CameraCount);
  ASSERT_NE(Cameras, nullptr);
  EXPECT_EQ(CameraCount, 1u);

  CameraNode* RetrievedCam = static_cast<CameraNode*>(Cameras[0]);
  EXPECT_FLOAT_EQ(RetrievedCam->GetFOV(), 75.0f);
  EXPECT_FLOAT_EQ(RetrievedCam->GetTransform().Translation.Y, 5.0f);

  delete Tree;
}

//=============================================================================
// Task Group 6 Engine Consumer Tests
//=============================================================================

//=============================================================================
// Test 9: SceneTree with MeshInstance typed nodes -- verify iterating
//         GetNodesByType(MeshInstance) finds all mesh references
//         (validates the ResourceSystem loading approach)
//=============================================================================
TEST_F(SceneTreeClassTest, GetNodesByTypeFindsMeshInstanceReferences)
{
  SceneTree* Tree = new SceneTree(TableAPI_, nullptr);
  ASSERT_NE(Tree, nullptr);

  // Create 3 nodes: 2 MeshInstance, 1 plain Node3D
  Node* MeshNodeA = Tree->CreateNode("MeshNodeA", NodeType::MeshInstance, nullptr);
  Node* MeshNodeB = Tree->CreateNode("MeshNodeB", NodeType::MeshInstance, nullptr);
  Node* EmptyNode = Tree->CreateNode("EmptyNode", NodeType::Node3D, nullptr);

  ASSERT_NE(MeshNodeA, nullptr);
  ASSERT_NE(MeshNodeB, nullptr);
  ASSERT_NE(EmptyNode, nullptr);

  // Set mesh paths on the MeshInstance nodes
  MeshInstance* MIA = static_cast<MeshInstance*>(MeshNodeA);
  MIA->SetMeshPath("models/cube.gltf");

  MeshInstance* MIB = static_cast<MeshInstance*>(MeshNodeB);
  MIB->SetMeshPath("models/sphere.gltf");

  // Iterate all MeshInstance nodes by type
  uint32_t Count = 0;
  Node** MeshNodes = Tree->GetNodesByType(NodeType::MeshInstance, &Count);
  ASSERT_NE(MeshNodes, nullptr);
  EXPECT_EQ(Count, 2u);

  // Verify we can read MeshPath from the typed nodes (resource loading approach)
  MeshInstance* MI0 = static_cast<MeshInstance*>(MeshNodes[0]);
  MeshInstance* MI1 = static_cast<MeshInstance*>(MeshNodes[1]);
  EXPECT_EQ(MI0->GetMeshPath(), "models/cube.gltf");
  EXPECT_EQ(MI1->GetMeshPath(), "models/sphere.gltf");

  // Verify the empty node is not in the MeshInstance list
  EXPECT_EQ(EmptyNode->GetType(), NodeType::Node3D);

  delete Tree;
}

//=============================================================================
// Test 10: SceneTree with LightNode and CameraNode typed nodes -- verify
//          they are accessible via GetNodesByType (validates renderer pattern)
//=============================================================================
TEST_F(SceneTreeClassTest, LightsAndCamerasAccessibleViaGetNodesByType)
{
  SceneTree* Tree = new SceneTree(TableAPI_, nullptr);
  ASSERT_NE(Tree, nullptr);

  // Create lights via typed LightNode
  Node* SunNode = Tree->CreateNode("Sun", NodeType::Light, nullptr);
  ASSERT_NE(SunNode, nullptr);
  LightNode* Sun = static_cast<LightNode*>(SunNode);
  Sun->SetLightType(AX_LIGHT_TYPE_DIRECTIONAL);
  Sun->SetColor({1.0f, 0.95f, 0.8f});
  Sun->SetIntensity(2.5f);

  Node* FillNode = Tree->CreateNode("Fill", NodeType::Light, nullptr);
  ASSERT_NE(FillNode, nullptr);
  LightNode* Fill = static_cast<LightNode*>(FillNode);
  Fill->SetLightType(AX_LIGHT_TYPE_POINT);
  Fill->SetColor({0.3f, 0.3f, 0.5f});
  Fill->SetIntensity(0.8f);

  // Verify lights are accessible via GetNodesByType (renderer reads these)
  uint32_t LightCount = 0;
  Node** Lights = Tree->GetNodesByType(NodeType::Light, &LightCount);
  ASSERT_NE(Lights, nullptr);
  EXPECT_EQ(LightCount, 2u);

  LightNode* L0 = static_cast<LightNode*>(Lights[0]);
  LightNode* L1 = static_cast<LightNode*>(Lights[1]);
  EXPECT_FLOAT_EQ(L0->GetIntensity(), 2.5f);
  EXPECT_FLOAT_EQ(L1->GetIntensity(), 0.8f);
  EXPECT_EQ(L0->GetName(), "Sun");
  EXPECT_EQ(L1->GetName(), "Fill");

  // Create camera via typed CameraNode
  Node* CamNode = Tree->CreateNode("MainCamera", NodeType::Camera, nullptr);
  ASSERT_NE(CamNode, nullptr);
  CameraNode* Cam = static_cast<CameraNode*>(CamNode);
  Cam->SetFOV(60.0f);
  Cam->SetNear(0.1f);
  Cam->SetFar(200.0f);
  Cam->GetTransform().Translation = Vec3(0.0f, 2.0f, 5.0f);

  // Verify cameras are accessible
  uint32_t CameraCount = 0;
  Node** Cameras = Tree->GetNodesByType(NodeType::Camera, &CameraCount);
  ASSERT_NE(Cameras, nullptr);
  EXPECT_EQ(CameraCount, 1u);

  CameraNode* RetrievedCam = static_cast<CameraNode*>(Cameras[0]);
  EXPECT_FLOAT_EQ(RetrievedCam->GetFOV(), 60.0f);
  EXPECT_FLOAT_EQ(RetrievedCam->GetNear(), 0.1f);
  EXPECT_FLOAT_EQ(RetrievedCam->GetFar(), 200.0f);
  EXPECT_FLOAT_EQ(RetrievedCam->GetTransform().Translation.X, 0.0f);
  EXPECT_FLOAT_EQ(RetrievedCam->GetTransform().Translation.Y, 2.0f);
  EXPECT_FLOAT_EQ(RetrievedCam->GetTransform().Translation.Z, 5.0f);

  // Verify GetRootNode is accessible for scene traversal
  ASSERT_NE(Tree->GetRootNode(), nullptr);
  // Root + Sun + Fill + MainCamera = 4
  EXPECT_EQ(Tree->GetNodeCount(), 4u);

  delete Tree;
}
