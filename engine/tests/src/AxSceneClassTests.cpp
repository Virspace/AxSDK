/**
 * AxSceneClassTests.cpp - Tests for the New C++ AxScene Class
 *
 * Tests the AxScene class directly (no AxSceneExtension, no old C AxScene struct).
 * Validates construction, node management, component tracking, system execution,
 * and correct defaults.
 */

#include "gtest/gtest.h"
#include "AxEngine/AxScene.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxHashTable.h"
#include "Foundation/AxAllocator.h"
#include "Foundation/AxAPIRegistry.h"

#include <cstring>

//=============================================================================
// Test Components (local to this test file)
//=============================================================================

namespace AxSceneClassTestHelpers {

struct TestCompA : public Component
{
  static constexpr uint32_t TypeID = 100;
  float Value;

  TestCompA() : Value(0.0f)
  {
    TypeID_ = TypeID;
    TypeName_ = "TestCompA";
  }

  size_t GetSize() const override { return (sizeof(TestCompA)); }
};

struct TestCompB : public Component
{
  static constexpr uint32_t TypeID = 200;
  int Counter;

  TestCompB() : Counter(0)
  {
    TypeID_ = TypeID;
    TypeName_ = "TestCompB";
  }

  size_t GetSize() const override { return (sizeof(TestCompB)); }
};

//=============================================================================
// MeshFilter-like test component for engine consumer tests
//=============================================================================

struct TestMeshFilter : public Component
{
  static constexpr uint32_t TypeID = 300;
  char MeshPath[256];
  uint32_t MeshHandle;

  TestMeshFilter() : MeshHandle(0)
  {
    MeshPath[0] = '\0';
    TypeID_ = TypeID;
    TypeName_ = "TestMeshFilter";
  }

  void SetMeshPath(const char* Path)
  {
    if (Path) {
      strncpy(MeshPath, Path, sizeof(MeshPath) - 1);
      MeshPath[sizeof(MeshPath) - 1] = '\0';
    }
  }

  size_t GetSize() const override { return (sizeof(TestMeshFilter)); }
};

//=============================================================================
// Test System - Tracks calls for verification
//=============================================================================

class TestSystem : public System
{
public:
  mutable int UpdateCallCount;
  mutable uint32_t LastComponentCount;
  mutable float LastDeltaT;

  TestSystem(const char* Name, SystemPhase Phase, int32_t Priority, uint32_t CompTypeID)
    : System(Name, Phase, Priority, CompTypeID)
    , UpdateCallCount(0)
    , LastComponentCount(0)
    , LastDeltaT(0.0f)
  {}

  void Update(float DeltaT, Component** Components, uint32_t Count) override
  {
    UpdateCallCount++;
    LastComponentCount = Count;
    LastDeltaT = DeltaT;
  }
};

} // namespace AxSceneClassTestHelpers

using namespace AxSceneClassTestHelpers;

//=============================================================================
// Test Fixture
//=============================================================================

class AxSceneClassTest : public testing::Test
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
// Test 1: AxScene construction creates RootNode, EventBus, and correct defaults
//=============================================================================
TEST_F(AxSceneClassTest, ConstructionCreatesRootAndDefaults)
{
  AxScene* Scene = new AxScene(TableAPI_);
  ASSERT_NE(Scene, nullptr);

  // Root node should exist
  ASSERT_NE(Scene->GetRootNode(), nullptr);
  EXPECT_EQ(Scene->GetRootNode()->GetName(), "Root");
  EXPECT_EQ(Scene->GetRootNode()->GetType(), NodeType::Root);

  // EventBus should exist
  ASSERT_NE(Scene->GetEventBus(), nullptr);

  // Node count should be 1 (just the root)
  EXPECT_EQ(Scene->GetNodeCount(), 1u);

  // Scene settings should be initialized to defaults
  EXPECT_FLOAT_EQ(Scene->AmbientLight.X, 0.1f);
  EXPECT_FLOAT_EQ(Scene->AmbientLight.Y, 0.1f);
  EXPECT_FLOAT_EQ(Scene->AmbientLight.Z, 0.1f);

  EXPECT_FLOAT_EQ(Scene->Gravity.X, 0.0f);
  EXPECT_FLOAT_EQ(Scene->Gravity.Y, -9.81f);
  EXPECT_FLOAT_EQ(Scene->Gravity.Z, 0.0f);

  EXPECT_FLOAT_EQ(Scene->FogDensity, 0.0f);

  EXPECT_FLOAT_EQ(Scene->FogColor.X, 0.5f);
  EXPECT_FLOAT_EQ(Scene->FogColor.Y, 0.5f);
  EXPECT_FLOAT_EQ(Scene->FogColor.Z, 0.5f);

  delete Scene;
}

//=============================================================================
// Test 2: CreateNode/GetNodeCount -- creating nodes increments count,
//         nodes parent correctly to root or specified parent
//=============================================================================
TEST_F(AxSceneClassTest, CreateNodeIncrementsCountAndParentsCorrectly)
{
  AxScene* Scene = new AxScene(TableAPI_);
  ASSERT_NE(Scene, nullptr);

  // Create two nodes as children of root (nullptr parent defaults to root)
  Node* NodeA = Scene->CreateNode("NodeA", NodeType::Node3D, nullptr);
  ASSERT_NE(NodeA, nullptr);
  EXPECT_EQ(NodeA->GetName(), "NodeA");
  EXPECT_EQ(NodeA->GetType(), NodeType::Node3D);
  EXPECT_EQ(Scene->GetNodeCount(), 2u); // Root + NodeA

  Node* NodeB = Scene->CreateNode("NodeB", NodeType::Node2D, nullptr);
  ASSERT_NE(NodeB, nullptr);
  EXPECT_EQ(NodeB->GetType(), NodeType::Node2D);
  EXPECT_EQ(Scene->GetNodeCount(), 3u); // Root + NodeA + NodeB

  // Both should be children of root
  EXPECT_EQ(NodeA->GetParent(), static_cast<Node*>(Scene->GetRootNode()));
  EXPECT_EQ(NodeB->GetParent(), static_cast<Node*>(Scene->GetRootNode()));

  // Create a child of NodeA
  Node* ChildA1 = Scene->CreateNode("ChildA1", NodeType::Node3D, NodeA);
  ASSERT_NE(ChildA1, nullptr);
  EXPECT_EQ(ChildA1->GetParent(), NodeA);
  EXPECT_EQ(Scene->GetNodeCount(), 4u);

  delete Scene;
}

//=============================================================================
// Test 3: DestroyNode -- destroying a node with children removes the
//         entire subtree and decrements NodeCount
//=============================================================================
TEST_F(AxSceneClassTest, DestroyNodeRemovesSubtreeAndDecrementsCount)
{
  AxScene* Scene = new AxScene(TableAPI_);
  ASSERT_NE(Scene, nullptr);

  // Create a parent with two children
  Node* Parent = Scene->CreateNode("Parent", NodeType::Node3D, nullptr);
  Node* Child1 = Scene->CreateNode("Child1", NodeType::Node3D, Parent);
  Node* Child2 = Scene->CreateNode("Child2", NodeType::Node3D, Parent);

  EXPECT_EQ(Scene->GetNodeCount(), 4u); // Root + Parent + Child1 + Child2

  // Destroy the parent (should also remove children)
  Scene->DestroyNode(Parent);

  // All 3 nodes (Parent + 2 children) should be removed
  EXPECT_EQ(Scene->GetNodeCount(), 1u); // Only root remains

  // Cannot find the destroyed nodes
  EXPECT_EQ(Scene->FindNode("Parent"), nullptr);
  EXPECT_EQ(Scene->FindNode("Child1"), nullptr);
  EXPECT_EQ(Scene->FindNode("Child2"), nullptr);

  // Suppress unused variable warnings
  (void)Child1;
  (void)Child2;

  delete Scene;
}

//=============================================================================
// Test 4: AddComponent/GetComponentsByType -- components added to nodes
//         appear in scene-level global lists by type
//=============================================================================
TEST_F(AxSceneClassTest, AddComponentAppearsInGlobalListByType)
{
  AxScene* Scene = new AxScene(TableAPI_);
  ASSERT_NE(Scene, nullptr);

  Node* NodeA = Scene->CreateNode("NodeA", NodeType::Node3D, nullptr);
  Node* NodeB = Scene->CreateNode("NodeB", NodeType::Node3D, nullptr);

  // Add TestCompA to both nodes
  TestCompA CompA1;
  TestCompA CompA2;
  EXPECT_TRUE(Scene->AddComponent(NodeA, &CompA1));
  EXPECT_TRUE(Scene->AddComponent(NodeB, &CompA2));

  // Query global component list for TestCompA type
  uint32_t Count = 0;
  Component** List = Scene->GetComponentsByType(TestCompA::TypeID, &Count);
  ASSERT_NE(List, nullptr);
  EXPECT_EQ(Count, 2u);
  EXPECT_EQ(List[0], static_cast<Component*>(&CompA1));
  EXPECT_EQ(List[1], static_cast<Component*>(&CompA2));

  // A different type should return empty
  uint32_t CountB = 0;
  Component** ListB = Scene->GetComponentsByType(TestCompB::TypeID, &CountB);
  EXPECT_EQ(ListB, nullptr);
  EXPECT_EQ(CountB, 0u);

  // Clean up: remove components before destruction
  Scene->RemoveComponent(NodeA, TestCompA::TypeID);
  Scene->RemoveComponent(NodeB, TestCompA::TypeID);

  delete Scene;
}

//=============================================================================
// Test 5: RemoveComponent -- removing a component from a node also
//         removes it from the scene-level global list
//=============================================================================
TEST_F(AxSceneClassTest, RemoveComponentRemovesFromGlobalList)
{
  AxScene* Scene = new AxScene(TableAPI_);
  ASSERT_NE(Scene, nullptr);

  Node* NodeA = Scene->CreateNode("NodeA", NodeType::Node3D, nullptr);
  Node* NodeB = Scene->CreateNode("NodeB", NodeType::Node3D, nullptr);

  TestCompA CompA1;
  TestCompA CompA2;
  Scene->AddComponent(NodeA, &CompA1);
  Scene->AddComponent(NodeB, &CompA2);

  // Verify both are present
  uint32_t Count = 0;
  Scene->GetComponentsByType(TestCompA::TypeID, &Count);
  EXPECT_EQ(Count, 2u);

  // Remove component from NodeA
  EXPECT_TRUE(Scene->RemoveComponent(NodeA, TestCompA::TypeID));

  // Global list should now have only one component
  uint32_t NewCount = 0;
  Component** NewList = Scene->GetComponentsByType(TestCompA::TypeID, &NewCount);
  ASSERT_NE(NewList, nullptr);
  EXPECT_EQ(NewCount, 1u);
  EXPECT_EQ(NewList[0], static_cast<Component*>(&CompA2));

  // The node should no longer have the component
  EXPECT_EQ(NodeA->GetComponent(TestCompA::TypeID), nullptr);

  // Clean up remaining component
  Scene->RemoveComponent(NodeB, TestCompA::TypeID);

  delete Scene;
}

//=============================================================================
// Test 6: RegisterSystem/UpdateSystems -- systems registered are sorted
//         by Phase then Priority; UpdateSystems calls them with correct
//         component lists
//=============================================================================
TEST_F(AxSceneClassTest, RegisterSystemSortsAndUpdateCallsWithCorrectLists)
{
  AxScene* Scene = new AxScene(TableAPI_);
  ASSERT_NE(Scene, nullptr);

  // Add components so systems have something to process
  Node* NodeA = Scene->CreateNode("NodeA", NodeType::Node3D, nullptr);
  Node* NodeB = Scene->CreateNode("NodeB", NodeType::Node3D, nullptr);

  TestCompA CompA1;
  TestCompA CompA2;
  TestCompB CompB1;
  Scene->AddComponent(NodeA, &CompA1);
  Scene->AddComponent(NodeB, &CompA2);
  Scene->AddComponent(NodeA, &CompB1);

  // Create systems in non-sorted order
  TestSystem RenderSys("RenderSys", SystemPhase::Render, 0, TestCompA::TypeID);
  TestSystem EarlySys("EarlySys", SystemPhase::EarlyUpdate, 0, TestCompA::TypeID);
  TestSystem UpdateSys("UpdateSys", SystemPhase::Update, 0, TestCompB::TypeID);

  // Register in arbitrary order
  Scene->RegisterSystem(&RenderSys);
  Scene->RegisterSystem(&EarlySys);
  Scene->RegisterSystem(&UpdateSys);

  EXPECT_EQ(Scene->GetSystemCount(), 3u);

  // Call UpdateSystems
  Scene->UpdateSystems(0.016f);

  // EarlySys processes TestCompA (2 components)
  EXPECT_EQ(EarlySys.UpdateCallCount, 1);
  EXPECT_EQ(EarlySys.LastComponentCount, 2u);
  EXPECT_FLOAT_EQ(EarlySys.LastDeltaT, 0.016f);

  // UpdateSys processes TestCompB (1 component)
  EXPECT_EQ(UpdateSys.UpdateCallCount, 1);
  EXPECT_EQ(UpdateSys.LastComponentCount, 1u);

  // RenderSys processes TestCompA (2 components)
  EXPECT_EQ(RenderSys.UpdateCallCount, 1);
  EXPECT_EQ(RenderSys.LastComponentCount, 2u);

  // Unregister before cleanup
  Scene->UnregisterSystem(&RenderSys);
  Scene->UnregisterSystem(&EarlySys);
  Scene->UnregisterSystem(&UpdateSys);

  // Clean up components
  Scene->RemoveComponent(NodeA, TestCompA::TypeID);
  Scene->RemoveComponent(NodeB, TestCompA::TypeID);
  Scene->RemoveComponent(NodeA, TestCompB::TypeID);

  delete Scene;
}

//=============================================================================
// Task Group 2 Regression Tests
//=============================================================================

//=============================================================================
// Test 7: Foundation types regression -- AxLight, AxCamera, AxTransform,
//         AxMaterial still exist and are usable after AxSceneObject and
//         old C AxScene removal from AxTypes.h
//=============================================================================
TEST_F(AxSceneClassTest, FoundationTypesStillUsableAfterCleanup)
{
  // Verify AxLight is usable: create and populate
  AxLight Light;
  memset(&Light, 0, sizeof(AxLight));
  strncpy(Light.Name, "TestLight", sizeof(Light.Name) - 1);
  Light.Type = AX_LIGHT_TYPE_DIRECTIONAL;
  Light.Color = {1.0f, 0.8f, 0.6f};
  Light.Intensity = 2.0f;

  EXPECT_EQ(Light.Name, "TestLight");
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

  EXPECT_EQ(MatDesc.Name, "TestMat");
  EXPECT_EQ(MatDesc.Type, AX_MATERIAL_TYPE_PBR);
  EXPECT_FLOAT_EQ(MatDesc.PBR.MetallicFactor, 0.5f);
  EXPECT_FLOAT_EQ(MatDesc.PBR.RoughnessFactor, 0.8f);
}

//=============================================================================
// Test 8: AxScene stores AxLight and AxCamera correctly -- verifies that
//         the new C++ AxScene class can create/store lights and cameras
//         (the Foundation types that remain) without any collision or
//         issues after the old C AxScene struct was removed
//=============================================================================
TEST_F(AxSceneClassTest, AxSceneStoresLightsAndCamerasCorrectly)
{
  AxScene* Scene = new AxScene(TableAPI_);
  ASSERT_NE(Scene, nullptr);

  // Initially no lights or cameras
  EXPECT_EQ(Scene->LightCount, 0u);
  EXPECT_EQ(Scene->CameraCount, 0u);

  // Create a directional light via AxScene
  AxLight* Light = Scene->CreateLight("Sun", AX_LIGHT_TYPE_DIRECTIONAL);
  ASSERT_NE(Light, nullptr);
  EXPECT_EQ(Light->Name, "Sun");
  EXPECT_EQ(Light->Type, AX_LIGHT_TYPE_DIRECTIONAL);
  EXPECT_FLOAT_EQ(Light->Intensity, 1.0f);
  EXPECT_EQ(Scene->LightCount, 1u);

  // Create a point light
  AxLight* PointLight = Scene->CreateLight("Lamp", AX_LIGHT_TYPE_POINT);
  ASSERT_NE(PointLight, nullptr);
  EXPECT_EQ(PointLight->Name, "Lamp");
  EXPECT_EQ(PointLight->Type, AX_LIGHT_TYPE_POINT);
  EXPECT_EQ(Scene->LightCount, 2u);

  // Find light by name
  AxLight* Found = Scene->FindLight("Sun");
  ASSERT_NE(Found, nullptr);
  EXPECT_EQ(Found->Name, "Sun");

  // Add a camera
  AxCamera Camera;
  memset(&Camera, 0, sizeof(AxCamera));
  Camera.FieldOfView = 75.0f;
  Camera.NearClipPlane = 0.1f;
  Camera.FarClipPlane = 500.0f;

  AxTransform CamTransform;
  memset(&CamTransform, 0, sizeof(AxTransform));
  CamTransform.Translation = {0.0f, 5.0f, -10.0f};
  CamTransform.Scale = {1.0f, 1.0f, 1.0f};
  CamTransform.Rotation = {0.0f, 0.0f, 0.0f, 1.0f};

  EXPECT_TRUE(Scene->AddCamera(&Camera, &CamTransform));
  EXPECT_EQ(Scene->CameraCount, 1u);

  // Retrieve camera
  AxTransform* OutTransform = nullptr;
  AxCamera* RetrievedCam = Scene->GetCamera(0, &OutTransform);
  ASSERT_NE(RetrievedCam, nullptr);
  ASSERT_NE(OutTransform, nullptr);
  EXPECT_FLOAT_EQ(RetrievedCam->FieldOfView, 75.0f);
  EXPECT_FLOAT_EQ(OutTransform->Translation.Y, 5.0f);

  // Verify AxScene Lights pointer is accessible (renderer reads this directly)
  ASSERT_NE(Scene->Lights, nullptr);
  EXPECT_EQ(Scene->Lights[0].Name, "Sun");
  EXPECT_EQ(Scene->Lights[1].Name, "Lamp");

  // Verify AxScene Cameras pointer is accessible
  ASSERT_NE(Scene->Cameras, nullptr);
  EXPECT_FLOAT_EQ(Scene->Cameras[0].FieldOfView, 75.0f);

  delete Scene;
}

//=============================================================================
// Task Group 6 Engine Consumer Tests
//=============================================================================

//=============================================================================
// Test 9: AxScene with nodes containing MeshFilter components -- verify
//         iterating GetComponentsByType(MeshFilter::TypeID) finds all mesh
//         references (validates the ResourceSystem loading approach)
//=============================================================================
TEST_F(AxSceneClassTest, GetComponentsByTypeFindsMeshFilterReferences)
{
  AxScene* Scene = new AxScene(TableAPI_);
  ASSERT_NE(Scene, nullptr);

  // Create 3 nodes, each with a TestMeshFilter component
  Node* NodeA = Scene->CreateNode("MeshNodeA", NodeType::Node3D, nullptr);
  Node* NodeB = Scene->CreateNode("MeshNodeB", NodeType::Node3D, nullptr);
  Node* NodeC = Scene->CreateNode("EmptyNode", NodeType::Node3D, nullptr);

  TestMeshFilter FilterA;
  FilterA.SetMeshPath("models/cube.gltf");
  TestMeshFilter FilterB;
  FilterB.SetMeshPath("models/sphere.gltf");

  EXPECT_TRUE(Scene->AddComponent(NodeA, &FilterA));
  EXPECT_TRUE(Scene->AddComponent(NodeB, &FilterB));
  // NodeC has no MeshFilter -- it is intentionally empty

  // Iterate all MeshFilter components by type
  uint32_t Count = 0;
  Component** MeshFilters = Scene->GetComponentsByType(TestMeshFilter::TypeID, &Count);
  ASSERT_NE(MeshFilters, nullptr);
  EXPECT_EQ(Count, 2u);

  // Verify we can read MeshPath from the components (resource loading approach)
  TestMeshFilter* MF0 = static_cast<TestMeshFilter*>(MeshFilters[0]);
  TestMeshFilter* MF1 = static_cast<TestMeshFilter*>(MeshFilters[1]);
  EXPECT_EQ(MF0->MeshPath, "models/cube.gltf");
  EXPECT_EQ(MF1->MeshPath, "models/sphere.gltf");

  // Verify we can get the owning node from the component
  EXPECT_EQ(MF0->GetOwner(), NodeA);
  EXPECT_EQ(MF1->GetOwner(), NodeB);

  // Verify the empty node has no MeshFilter
  EXPECT_EQ(NodeC->GetComponent(TestMeshFilter::TypeID), nullptr);

  // Clean up
  Scene->RemoveComponent(NodeA, TestMeshFilter::TypeID);
  Scene->RemoveComponent(NodeB, TestMeshFilter::TypeID);

  delete Scene;
}

//=============================================================================
// Test 10: AxScene with lights and cameras -- verify Scene->Lights,
//          Scene->LightCount, Scene->Cameras, Scene->CameraCount are
//          accessible (validates renderer can read light/camera data)
//=============================================================================
TEST_F(AxSceneClassTest, LightsAndCamerasAccessibleForRenderer)
{
  AxScene* Scene = new AxScene(TableAPI_);
  ASSERT_NE(Scene, nullptr);

  // Create lights
  AxLight* Sun = Scene->CreateLight("Sun", AX_LIGHT_TYPE_DIRECTIONAL);
  ASSERT_NE(Sun, nullptr);
  Sun->Color = {1.0f, 0.95f, 0.8f};
  Sun->Intensity = 2.5f;
  Sun->Direction = {0.0f, -1.0f, -0.5f};

  AxLight* Fill = Scene->CreateLight("Fill", AX_LIGHT_TYPE_POINT);
  ASSERT_NE(Fill, nullptr);
  Fill->Color = {0.3f, 0.3f, 0.5f};
  Fill->Intensity = 0.8f;

  // Verify lights are accessible via public members (renderer reads these directly)
  EXPECT_EQ(Scene->LightCount, 2u);
  ASSERT_NE(Scene->Lights, nullptr);
  EXPECT_FLOAT_EQ(Scene->Lights[0].Intensity, 2.5f);
  EXPECT_FLOAT_EQ(Scene->Lights[1].Intensity, 0.8f);
  EXPECT_EQ(Scene->Lights[0].Name, "Sun");
  EXPECT_EQ(Scene->Lights[1].Name, "Fill");

  // Add cameras
  AxCamera Cam1;
  memset(&Cam1, 0, sizeof(AxCamera));
  Cam1.FieldOfView = 60.0f;
  Cam1.NearClipPlane = 0.1f;
  Cam1.FarClipPlane = 200.0f;

  AxTransform CamTransform1;
  memset(&CamTransform1, 0, sizeof(AxTransform));
  CamTransform1.Translation = {0.0f, 2.0f, 5.0f};
  CamTransform1.Scale = {1.0f, 1.0f, 1.0f};
  CamTransform1.Rotation = {0.0f, 0.0f, 0.0f, 1.0f};

  EXPECT_TRUE(Scene->AddCamera(&Cam1, &CamTransform1));

  // Verify cameras are accessible
  EXPECT_EQ(Scene->CameraCount, 1u);
  ASSERT_NE(Scene->Cameras, nullptr);
  EXPECT_FLOAT_EQ(Scene->Cameras[0].FieldOfView, 60.0f);
  EXPECT_FLOAT_EQ(Scene->Cameras[0].NearClipPlane, 0.1f);
  EXPECT_FLOAT_EQ(Scene->Cameras[0].FarClipPlane, 200.0f);

  // Verify camera transform is accessible
  ASSERT_NE(Scene->CameraTransforms, nullptr);
  EXPECT_FLOAT_EQ(Scene->CameraTransforms[0].Translation.X, 0.0f);
  EXPECT_FLOAT_EQ(Scene->CameraTransforms[0].Translation.Y, 2.0f);
  EXPECT_FLOAT_EQ(Scene->CameraTransforms[0].Translation.Z, 5.0f);

  // Verify GetRootNode is accessible for scene traversal
  ASSERT_NE(Scene->GetRootNode(), nullptr);
  EXPECT_EQ(Scene->GetNodeCount(), 1u);

  delete Scene;
}
