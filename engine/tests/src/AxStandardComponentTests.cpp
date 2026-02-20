/**
 * AxStandardComponentTests.cpp - Tests for Standard Components (Task Group 4)
 *
 * Tests the concrete component types: MeshFilter, MeshRenderer,
 * CameraComponent, LightComponent, RigidBody, ScriptComponent,
 * and multi-component attachment to nodes.
 */

#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxHashTable.h"
#include "Foundation/AxAllocator.h"
#include "Foundation/AxAllocatorAPI.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxMath.h"
#include "AxEngine/AxNode.h"
#include "AxEngine/AxComponent.h"
#include "AxEngine/AxComponentFactory.h"
#include "AxEngine/AxScript.h"

// Include the standard components header directly (it is in the AxScene plugin src)
#include "AxStandardComponents.h"

#include <cstring>

/**
 * Minimal mock AxScript for testing ScriptComponent lifecycle delegation.
 *
 * Inherits from the real AxScript class and overrides lifecycle methods
 * to record call counts for test verification. The mock never calls the
 * engine-dependent inner struct methods (SceneManager, Input), so there
 * are no linker issues with the test binary.
 */
class MockAxScript : public AxScript
{
public:
  int InitCallCount;
  int StartCallCount;
  int TickCallCount;
  int TermCallCount;
  float LastDeltaT;

  MockAxScript()
    : InitCallCount(0)
    , StartCallCount(0)
    , TickCallCount(0)
    , TermCallCount(0)
    , LastDeltaT(0.0f)
  {}

  void Init() override { InitCallCount++; }
  void Start() override { StartCallCount++; }
  void Tick(float Alpha, float DeltaT) override
  {
    (void)Alpha;
    TickCallCount++;
    LastDeltaT = DeltaT;
  }
  void Term() override { TermCallCount++; }
};

/**
 * Test fixture that initializes Foundation APIs and ComponentFactory.
 */
class StandardComponentTest : public testing::Test
{
protected:
  void SetUp() override
  {
    AxonInitGlobalAPIRegistry();
    AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);

    TableAPI_ = static_cast<AxHashTableAPI*>(
      AxonGlobalAPIRegistry->Get(AXON_HASH_TABLE_API_NAME));
    ASSERT_NE(TableAPI_, nullptr) << "Failed to get HashTableAPI";

    AllocatorAPI_ = static_cast<AxAllocatorAPI*>(
      AxonGlobalAPIRegistry->Get(AXON_ALLOCATOR_API_NAME));
    ASSERT_NE(AllocatorAPI_, nullptr) << "Failed to get AllocatorAPI";

    TestAllocator_ = AllocatorAPI_->CreateHeap("TestAllocator", 1024 * 1024, 4 * 1024 * 1024);
    ASSERT_NE(TestAllocator_, nullptr) << "Failed to create test allocator";

    Factory_ = new ComponentFactory();

    // Register all standard components with the factory
    RegisterStandardComponents(Factory_);
  }

  void TearDown() override
  {
    delete Factory_;
    Factory_ = nullptr;

    if (TestAllocator_) {
      TestAllocator_->Destroy(TestAllocator_);
      TestAllocator_ = nullptr;
    }

    AxonTermGlobalAPIRegistry();
  }

  AxHashTableAPI* TableAPI_{nullptr};
  AxAllocatorAPI* AllocatorAPI_{nullptr};
  AxAllocator* TestAllocator_{nullptr};
  ComponentFactory* Factory_{nullptr};
};

//=============================================================================
// Test 1: MeshFilter creation and mesh path assignment
//=============================================================================
TEST_F(StandardComponentTest, MeshFilterCreationAndPathAssignment)
{
  // Create MeshFilter via factory
  Component* Comp = Factory_->CreateComponent("MeshFilter", TestAllocator_);
  ASSERT_NE(Comp, nullptr);
  EXPECT_EQ(Comp->GetTypeName(), "MeshFilter");
  EXPECT_GT(Comp->GetTypeID(), 0u);

  // Cast and verify default state
  MeshFilter* Filter = static_cast<MeshFilter*>(Comp);
  EXPECT_STREQ(Filter->MeshPath, "");
  EXPECT_FALSE(AX_HANDLE_IS_VALID(Filter->ModelHandle));

  // Set mesh path and verify
  Filter->SetMeshPath("res://meshes/character.glb");
  EXPECT_STREQ(Filter->MeshPath, "res://meshes/character.glb");

  // Verify ModelHandle can be set
  Filter->ModelHandle = {42, 1};
  EXPECT_EQ(Filter->ModelHandle.Index, 42u);

  Factory_->DestroyComponent(Comp, TestAllocator_);
}

//=============================================================================
// Test 2: MeshRenderer creation with material name binding
//=============================================================================
TEST_F(StandardComponentTest, MeshRendererCreationWithMaterialBinding)
{
  Component* Comp = Factory_->CreateComponent("MeshRenderer", TestAllocator_);
  ASSERT_NE(Comp, nullptr);
  EXPECT_EQ(Comp->GetTypeName(), "MeshRenderer");

  MeshRenderer* Renderer = static_cast<MeshRenderer*>(Comp);

  // Verify defaults
  EXPECT_STREQ(Renderer->MaterialName, "");
  EXPECT_EQ(Renderer->MaterialHandle, 0u);
  EXPECT_EQ(Renderer->RenderLayer, 0);

  // Set material name and verify
  Renderer->SetMaterialName("DefaultMaterial");
  EXPECT_STREQ(Renderer->MaterialName, "DefaultMaterial");

  // Set render layer
  Renderer->RenderLayer = 5;
  EXPECT_EQ(Renderer->RenderLayer, 5);

  Factory_->DestroyComponent(Comp, TestAllocator_);
}

//=============================================================================
// Test 3: CameraComponent wraps AxCamera and exposes FOV, near, far
//=============================================================================
TEST_F(StandardComponentTest, CameraComponentWrapsAxCamera)
{
  Component* Comp = Factory_->CreateComponent("CameraComponent", TestAllocator_);
  ASSERT_NE(Comp, nullptr);
  EXPECT_EQ(Comp->GetTypeName(), "CameraComponent");

  CameraComponent* Cam = static_cast<CameraComponent*>(Comp);

  // Verify default camera values
  EXPECT_FLOAT_EQ(Cam->GetFOV(), 60.0f);
  EXPECT_FLOAT_EQ(Cam->GetNear(), 0.1f);
  EXPECT_FLOAT_EQ(Cam->GetFar(), 1000.0f);
  EXPECT_FALSE(Cam->IsOrthographic());

  // Modify and verify
  Cam->SetFOV(90.0f);
  Cam->SetNear(0.01f);
  Cam->SetFar(5000.0f);
  Cam->SetOrthographic(true);

  EXPECT_FLOAT_EQ(Cam->GetFOV(), 90.0f);
  EXPECT_FLOAT_EQ(Cam->GetNear(), 0.01f);
  EXPECT_FLOAT_EQ(Cam->GetFar(), 5000.0f);
  EXPECT_TRUE(Cam->IsOrthographic());

  // Verify the underlying AxCamera struct is consistent
  EXPECT_FLOAT_EQ(Cam->Camera.FieldOfView, 90.0f);
  EXPECT_FLOAT_EQ(Cam->Camera.NearClipPlane, 0.01f);
  EXPECT_FLOAT_EQ(Cam->Camera.FarClipPlane, 5000.0f);
  EXPECT_TRUE(Cam->Camera.IsOrthographic);

  Factory_->DestroyComponent(Comp, TestAllocator_);
}

//=============================================================================
// Test 4: LightComponent wraps AxLight and exposes type, color, intensity
//=============================================================================
TEST_F(StandardComponentTest, LightComponentWrapsAxLight)
{
  Component* Comp = Factory_->CreateComponent("LightComponent", TestAllocator_);
  ASSERT_NE(Comp, nullptr);
  EXPECT_EQ(Comp->GetTypeName(), "LightComponent");

  LightComponent* Lit = static_cast<LightComponent*>(Comp);

  // Verify default light values
  EXPECT_EQ(Lit->GetLightType(), AX_LIGHT_TYPE_POINT);
  EXPECT_FLOAT_EQ(Lit->GetColor().R, 1.0f);
  EXPECT_FLOAT_EQ(Lit->GetColor().G, 1.0f);
  EXPECT_FLOAT_EQ(Lit->GetColor().B, 1.0f);
  EXPECT_FLOAT_EQ(Lit->GetIntensity(), 1.0f);

  // Modify and verify
  Lit->SetLightType(AX_LIGHT_TYPE_DIRECTIONAL);
  Lit->SetColor({0.9f, 0.8f, 0.7f});
  Lit->SetIntensity(2.5f);

  EXPECT_EQ(Lit->GetLightType(), AX_LIGHT_TYPE_DIRECTIONAL);
  EXPECT_FLOAT_EQ(Lit->GetColor().R, 0.9f);
  EXPECT_FLOAT_EQ(Lit->GetColor().G, 0.8f);
  EXPECT_FLOAT_EQ(Lit->GetColor().B, 0.7f);
  EXPECT_FLOAT_EQ(Lit->GetIntensity(), 2.5f);

  // Verify the underlying AxLight struct is consistent
  EXPECT_EQ(Lit->Light.Type, AX_LIGHT_TYPE_DIRECTIONAL);
  EXPECT_FLOAT_EQ(Lit->Light.Intensity, 2.5f);

  Factory_->DestroyComponent(Comp, TestAllocator_);
}

//=============================================================================
// Test 5: Attaching MeshFilter + MeshRenderer to a node via component operations
//=============================================================================
TEST_F(StandardComponentTest, AttachMultipleComponentsToNode)
{
  // Create a node
  Node node("TestMesh", NodeType::Node3D, TableAPI_);

  // Create components via factory
  Component* FilterComp = Factory_->CreateComponent("MeshFilter", TestAllocator_);
  Component* RendererComp = Factory_->CreateComponent("MeshRenderer", TestAllocator_);
  ASSERT_NE(FilterComp, nullptr);
  ASSERT_NE(RendererComp, nullptr);

  // Set up the components
  MeshFilter* Filter = static_cast<MeshFilter*>(FilterComp);
  Filter->SetMeshPath("res://meshes/cube.glb");

  MeshRenderer* Renderer = static_cast<MeshRenderer*>(RendererComp);
  Renderer->SetMaterialName("WoodMaterial");

  // Add both to the node -- they have different TypeIDs so both should succeed
  EXPECT_TRUE(node.AddComponent(FilterComp));
  EXPECT_TRUE(node.AddComponent(RendererComp));

  // Verify both are attached
  EXPECT_TRUE(node.HasComponent(FilterComp->GetTypeID()));
  EXPECT_TRUE(node.HasComponent(RendererComp->GetTypeID()));

  // Verify we can retrieve them
  Component* FoundFilter = node.GetComponent(FilterComp->GetTypeID());
  Component* FoundRenderer = node.GetComponent(RendererComp->GetTypeID());
  ASSERT_NE(FoundFilter, nullptr);
  ASSERT_NE(FoundRenderer, nullptr);
  EXPECT_EQ(FoundFilter, FilterComp);
  EXPECT_EQ(FoundRenderer, RendererComp);

  // Verify owner is set
  EXPECT_EQ(FilterComp->GetOwner(), &node);
  EXPECT_EQ(RendererComp->GetOwner(), &node);

  // One component per type: adding a second MeshFilter should fail
  Component* DuplicateFilter = Factory_->CreateComponent("MeshFilter", TestAllocator_);
  ASSERT_NE(DuplicateFilter, nullptr);
  EXPECT_FALSE(node.AddComponent(DuplicateFilter));

  // Clean up: remove from node before destroying
  node.RemoveComponent(FilterComp->GetTypeID());
  node.RemoveComponent(RendererComp->GetTypeID());

  Factory_->DestroyComponent(FilterComp, TestAllocator_);
  Factory_->DestroyComponent(RendererComp, TestAllocator_);
  Factory_->DestroyComponent(DuplicateFilter, TestAllocator_);
}

//=============================================================================
// Test 6: RigidBody component stores mass, velocity, and bodyType
//=============================================================================
TEST_F(StandardComponentTest, RigidBodyStoresMassVelocityBodyType)
{
  Component* Comp = Factory_->CreateComponent("RigidBody", TestAllocator_);
  ASSERT_NE(Comp, nullptr);
  EXPECT_EQ(Comp->GetTypeName(), "RigidBody");

  RigidBody* Body = static_cast<RigidBody*>(Comp);

  // Verify defaults
  EXPECT_FLOAT_EQ(Body->Mass, 1.0f);
  EXPECT_FLOAT_EQ(Body->LinearVelocity.X, 0.0f);
  EXPECT_FLOAT_EQ(Body->LinearVelocity.Y, 0.0f);
  EXPECT_FLOAT_EQ(Body->LinearVelocity.Z, 0.0f);
  EXPECT_FLOAT_EQ(Body->AngularVelocity.X, 0.0f);
  EXPECT_FLOAT_EQ(Body->AngularVelocity.Y, 0.0f);
  EXPECT_FLOAT_EQ(Body->AngularVelocity.Z, 0.0f);
  EXPECT_FLOAT_EQ(Body->Drag, 0.0f);
  EXPECT_EQ(Body->Type, BodyType::Dynamic);

  // Modify and verify
  Body->Mass = 70.0f;
  Body->LinearVelocity = {1.0f, 2.0f, 3.0f};
  Body->AngularVelocity = {0.1f, 0.2f, 0.3f};
  Body->Drag = 0.5f;
  Body->Type = BodyType::Kinematic;

  EXPECT_FLOAT_EQ(Body->Mass, 70.0f);
  EXPECT_FLOAT_EQ(Body->LinearVelocity.X, 1.0f);
  EXPECT_FLOAT_EQ(Body->LinearVelocity.Y, 2.0f);
  EXPECT_FLOAT_EQ(Body->LinearVelocity.Z, 3.0f);
  EXPECT_FLOAT_EQ(Body->Drag, 0.5f);
  EXPECT_EQ(Body->Type, BodyType::Kinematic);

  Factory_->DestroyComponent(Comp, TestAllocator_);
}

//=============================================================================
// Test 7: ScriptComponent hosts an AxScript pointer and delegates lifecycle
//=============================================================================
TEST_F(StandardComponentTest, ScriptComponentDelegatesLifecycle)
{
  Component* Comp = Factory_->CreateComponent("ScriptComponent", TestAllocator_);
  ASSERT_NE(Comp, nullptr);
  EXPECT_EQ(Comp->GetTypeName(), "ScriptComponent");

  ScriptComponent* SC = static_cast<ScriptComponent*>(Comp);

  // Verify default state
  EXPECT_EQ(SC->GetScript(), nullptr);

  // Create a mock script and attach it
  MockAxScript MockScript;
  SC->SetScript(&MockScript);
  EXPECT_EQ(SC->GetScript(), &MockScript);

  // Delegate Init
  SC->DelegateInit();
  EXPECT_EQ(MockScript.InitCallCount, 1);

  // Delegate Start
  SC->DelegateStart();
  EXPECT_EQ(MockScript.StartCallCount, 1);

  // Delegate Tick
  SC->DelegateTick(1.0f, 0.016f);
  EXPECT_EQ(MockScript.TickCallCount, 1);
  EXPECT_FLOAT_EQ(MockScript.LastDeltaT, 0.016f);

  // Delegate Term
  SC->DelegateTerm();
  EXPECT_EQ(MockScript.TermCallCount, 1);

  // Delegating with null script should be safe (no-op)
  SC->SetScript(nullptr);
  SC->DelegateInit();  // Should not crash
  SC->DelegateTick(1.0f, 0.016f);  // Should not crash

  // Mock script call counts should not change
  EXPECT_EQ(MockScript.InitCallCount, 1);
  EXPECT_EQ(MockScript.TickCallCount, 1);

  Factory_->DestroyComponent(Comp, TestAllocator_);
}
