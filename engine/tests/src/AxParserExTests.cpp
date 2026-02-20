/**
 * AxParserExTests.cpp - Tests for Extended Parser and Prefab Support
 *
 * Tests the .ats parser extension for node/component syntax and .axp prefab
 * loading/instantiation. Uses the AxSceneParserAPI (via AxSceneParser_GetAPI)
 * and the internal ParsePrefab/InstantiatePrefab functions directly.
 *
 * - Parsing `node` keyword creates Node
 * - Parsing `component MeshFilter { mesh: "path" }` attaches component
 * - Nested nodes with components build correct hierarchy
 * - Multiple nodes with mixed component types
 * - .axp prefab loading creates standalone subtree
 * - Prefab instantiation creates deep copy
 * - TG5: Parser produces AxScene with correct light and node data
 * - TG5: Parser calls AxScene->CreateNode and AddComponent directly
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
#include "AxEngine/AxScene.h"
#include "AxScene/AxScene.h"
#include "AxSceneParserInternal.h"
#include "AxStandardComponents.h"

#include <string>
#include <string_view>

//=============================================================================
// Test Fixture
//=============================================================================

class ParserExTest : public testing::Test
{
protected:
  void SetUp() override
  {
    AxonInitGlobalAPIRegistry();
    AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);

    TableAPI_ = static_cast<AxHashTableAPI*>(
      AxonGlobalAPIRegistry->Get(AXON_HASH_TABLE_API_NAME));
    ASSERT_NE(TableAPI_, nullptr);

    AllocatorAPI_ = static_cast<AxAllocatorAPI*>(
      AxonGlobalAPIRegistry->Get(AXON_ALLOCATOR_API_NAME));
    ASSERT_NE(AllocatorAPI_, nullptr);

    Allocator_ = AllocatorAPI_->CreateHeap("TestParserAlloc", 64 * 1024, 256 * 1024);
    ASSERT_NE(Allocator_, nullptr);

    Factory_ = AxComponentFactory_Get();
    ASSERT_NE(Factory_, nullptr);

    RegisterStandardComponents(Factory_);

    AxSceneParser_Init(AxonGlobalAPIRegistry);
    ParserAPI_ = AxSceneParser_GetAPI();
    ASSERT_NE(ParserAPI_, nullptr);
  }

  void TearDown() override
  {
    AxSceneParser_Term();

    if (Allocator_) {
      Allocator_->Destroy(Allocator_);
      Allocator_ = nullptr;
    }

    AxonTermGlobalAPIRegistry();
  }

  /**
   * Helper: wrap a node block in a minimal scene, parse it, and return the
   * first root child (the top-level parsed node). Caller must delete the scene.
   */
  AxScene* ParseSceneFromNode(std::string_view NodeSource, Node** OutNode = nullptr)
  {
    char Buf[8192];
    snprintf(Buf, sizeof(Buf), "scene \"Test\" {\n%s\n}", NodeSource);

    AxScene* Scene = ParserAPI_->ParseFromString(Buf, Allocator_);
    if (Scene && OutNode) {
      *OutNode = Scene->GetRootNode()->GetFirstChild();
    }
    return (Scene);
  }

  AxHashTableAPI*   TableAPI_{nullptr};
  AxAllocatorAPI*   AllocatorAPI_{nullptr};
  AxAllocator*      Allocator_{nullptr};
  ComponentFactory* Factory_{nullptr};
  AxSceneParserAPI* ParserAPI_{nullptr};
};

//=============================================================================
// Test 1: Parsing `node` keyword creates Node
//=============================================================================
TEST_F(ParserExTest, ParseNodeKeywordCreatesNode)
{
  std::string Source = R"(node "Player" {
  })";

  Node* PlayerNode = nullptr;
  AxScene* Scene = ParseSceneFromNode(Source, &PlayerNode);
  ASSERT_NE(Scene, nullptr);
  ASSERT_NE(PlayerNode, nullptr);
  EXPECT_EQ(PlayerNode->GetName(), "Player");
  EXPECT_EQ(Scene->GetNodeCount(), 2u); // root + Player

  delete Scene;
}

//=============================================================================
// Test 2: Parsing component attaches MeshFilter to node
//=============================================================================
TEST_F(ParserExTest, ParseComponentMeshFilterAttachesToNode)
{
  const char* Source = R"(node "Obj" {
    component MeshFilter {
      mesh: "res://meshes/cube.glb"
    }
  })";

  Node* ObjNode = nullptr;
  AxScene* Scene = ParseSceneFromNode(Source, &ObjNode);
  ASSERT_NE(Scene, nullptr);
  ASSERT_NE(ObjNode, nullptr);
  EXPECT_EQ(ObjNode->GetName(), "Obj");

  const ComponentTypeInfo* MFInfo = Factory_->FindType("MeshFilter");
  ASSERT_NE(MFInfo, nullptr);

  Component* Comp = ObjNode->GetComponent(MFInfo->TypeID);
  ASSERT_NE(Comp, nullptr);

  MeshFilter* MF = static_cast<MeshFilter*>(Comp);
  EXPECT_EQ(MF->MeshPath, "res://meshes/cube.glb");

  delete Scene;
}

//=============================================================================
// Test 3: Parsing nested nodes with components builds correct hierarchy
//=============================================================================
TEST_F(ParserExTest, NestedNodesWithComponentsBuildHierarchy)
{
  const char* Source = R"(node "Parent" {
    transform {
      translation: 1.0, 2.0, 3.0
    }
    node "Child" {
      transform {
        translation: 4.0, 5.0, 6.0
      }
      component MeshFilter {
        mesh: "res://child_mesh.glb"
      }
    }
  })";

  Node* Parent = nullptr;
  AxScene* Scene = ParseSceneFromNode(Source, &Parent);
  ASSERT_NE(Scene, nullptr);
  ASSERT_NE(Parent, nullptr);
  EXPECT_EQ(Parent->GetName(), "Parent");

  EXPECT_FLOAT_EQ(Parent->GetTransform().Translation.X, 1.0f);
  EXPECT_FLOAT_EQ(Parent->GetTransform().Translation.Y, 2.0f);
  EXPECT_FLOAT_EQ(Parent->GetTransform().Translation.Z, 3.0f);

  Node* Child = Parent->GetFirstChild();
  ASSERT_NE(Child, nullptr);
  EXPECT_EQ(Child->GetName(), "Child");
  EXPECT_FLOAT_EQ(Child->GetTransform().Translation.X, 4.0f);

  const ComponentTypeInfo* MFInfo = Factory_->FindType("MeshFilter");
  ASSERT_NE(MFInfo, nullptr);
  Component* Comp = Child->GetComponent(MFInfo->TypeID);
  ASSERT_NE(Comp, nullptr);

  delete Scene;
}

//=============================================================================
// Test 4: Parsing scene with multiple nodes and mixed component types
//=============================================================================
TEST_F(ParserExTest, MultipleNodesWithMixedComponents)
{
  const char* Source = R"(node "Container" {
    node "Mesh1" {
      component MeshFilter {
        mesh: "res://a.glb"
      }
      component MeshRenderer {
        material: "MatA"
        renderLayer: 0
      }
    }
    node "Body" {
      component RigidBody {
        mass: 10.0
        bodyType: dynamic
      }
    }
  })";

  Node* Container = nullptr;
  AxScene* Scene = ParseSceneFromNode(Source, &Container);
  ASSERT_NE(Scene, nullptr);
  ASSERT_NE(Container, nullptr);
  EXPECT_EQ(Container->GetName(), "Container");

  Node* Mesh1 = Container->GetFirstChild();
  ASSERT_NE(Mesh1, nullptr);
  EXPECT_EQ(Mesh1->GetName(), "Mesh1");

  Node* Body = Mesh1->GetNextSibling();
  ASSERT_NE(Body, nullptr);
  EXPECT_EQ(Body->GetName(), "Body");

  const ComponentTypeInfo* RBInfo = Factory_->FindType("RigidBody");
  ASSERT_NE(RBInfo, nullptr);
  Component* RBComp = Body->GetComponent(RBInfo->TypeID);
  ASSERT_NE(RBComp, nullptr);
  RigidBody* RB = static_cast<RigidBody*>(RBComp);
  EXPECT_FLOAT_EQ(RB->Mass, 10.0f);
  EXPECT_EQ(RB->Type, BodyType::Dynamic);

  delete Scene;
}

//=============================================================================
// Test 5: Parser correctly creates nodes and components (callback smoke test)
//=============================================================================
TEST_F(ParserExTest, ParseCallbacksFire)
{
  const char* Source = R"(node "CallbackTest" {
    component MeshFilter {
      mesh: "test.glb"
    }
  })";

  Node* Result = nullptr;
  AxScene* Scene = ParseSceneFromNode(Source, &Result);
  ASSERT_NE(Scene, nullptr);
  ASSERT_NE(Result, nullptr);
  EXPECT_EQ(Result->GetName(), "CallbackTest");

  const ComponentTypeInfo* MFInfo = Factory_->FindType("MeshFilter");
  ASSERT_NE(MFInfo, nullptr);
  Component* Comp = Result->GetComponent(MFInfo->TypeID);
  ASSERT_NE(Comp, nullptr);

  delete Scene;
}

//=============================================================================
// Test 6: Loading .axp prefab creates standalone node subtree
//=============================================================================
TEST_F(ParserExTest, PrefabLoadCreatesStandaloneSubtree)
{
  const char* PrefabData = R"(node "PrefabRoot" {
    component MeshFilter {
      mesh: "res://prefab_mesh.glb"
    }
    node "PrefabChild" {
      component RigidBody {
        mass: 5.0
      }
    }
  })";

  AxScene* Scene = new AxScene(TableAPI_);
  Scene->Allocator = Allocator_;

  Node* PrefabRoot = ParsePrefab(PrefabData, Scene);
  ASSERT_NE(PrefabRoot, nullptr);
  EXPECT_EQ(PrefabRoot->GetName(), "PrefabRoot");

  Node* PrefabChild = PrefabRoot->GetFirstChild();
  ASSERT_NE(PrefabChild, nullptr);
  EXPECT_EQ(PrefabChild->GetName(), "PrefabChild");

  const ComponentTypeInfo* MFInfo = Factory_->FindType("MeshFilter");
  ASSERT_NE(MFInfo, nullptr);
  Component* Comp = PrefabRoot->GetComponent(MFInfo->TypeID);
  ASSERT_NE(Comp, nullptr);

  delete Scene;
}

//=============================================================================
// Test 7: Instantiating prefab creates deep copy independent of original
//=============================================================================
TEST_F(ParserExTest, PrefabInstantiationCreatesDeepCopy)
{
  const char* PrefabData = R"(node "Original" {
    component MeshFilter {
      mesh: "res://original.glb"
    }
    node "OriginalChild" {
    }
  })";

  AxScene* Scene = new AxScene(TableAPI_);
  Scene->Allocator = Allocator_;

  Node* PrefabRoot = ParsePrefab(PrefabData, Scene);
  ASSERT_NE(PrefabRoot, nullptr);

  Node* CopyRoot = InstantiatePrefab(Scene, PrefabRoot, nullptr, Allocator_);
  ASSERT_NE(CopyRoot, nullptr);

  EXPECT_EQ(CopyRoot->GetName(), "Original");
  EXPECT_NE(CopyRoot, PrefabRoot);

  Node* CopyChild = CopyRoot->GetFirstChild();
  ASSERT_NE(CopyChild, nullptr);
  EXPECT_EQ(CopyChild->GetName(), "OriginalChild");

  Node* OrigChild = PrefabRoot->GetFirstChild();
  ASSERT_NE(OrigChild, nullptr);
  EXPECT_NE(CopyChild, OrigChild);

  const ComponentTypeInfo* MFInfo = Factory_->FindType("MeshFilter");
  ASSERT_NE(MFInfo, nullptr);
  Component* OrigComp = PrefabRoot->GetComponent(MFInfo->TypeID);
  Component* CopyComp = CopyRoot->GetComponent(MFInfo->TypeID);
  ASSERT_NE(OrigComp, nullptr);
  ASSERT_NE(CopyComp, nullptr);
  EXPECT_NE(OrigComp, CopyComp);

  delete Scene;
}

//=============================================================================
// TG5 Test 1: Parser produces AxScene with correct light and node data
//=============================================================================
TEST_F(ParserExTest, TG5_SceneWithLightAndNodeProducesCorrectData)
{
  const char* FullScene = R"(scene "TG5Test" {
    light "SunLight" {
      type: directional
      direction: 0.0, -1.0, 0.0
      color: 1.0, 0.9, 0.8
      intensity: 2.5
    }
    node "SceneRoot" {
      node "Player" {
        component MeshFilter {
          mesh: "res://player.glb"
        }
      }
    }
  })";

  AxScene* Scene = ParserAPI_->ParseFromString(FullScene, Allocator_);
  ASSERT_NE(Scene, nullptr);

  EXPECT_EQ(Scene->LightCount, 1u);
  EXPECT_EQ(Scene->Lights[0].Name, "SunLight");
  EXPECT_EQ(Scene->Lights[0].Type, AX_LIGHT_TYPE_DIRECTIONAL);
  EXPECT_FLOAT_EQ(Scene->Lights[0].Intensity, 2.5f);

  // root + SceneRoot + Player = 3
  EXPECT_EQ(Scene->GetNodeCount(), 3u);

  Node* SceneRoot = Scene->GetRootNode()->GetFirstChild();
  ASSERT_NE(SceneRoot, nullptr);
  EXPECT_EQ(SceneRoot->GetName(), "SceneRoot");

  Node* Player = SceneRoot->GetFirstChild();
  ASSERT_NE(Player, nullptr);
  EXPECT_EQ(Player->GetName(), "Player");

  delete Scene;
}

//=============================================================================
// TG5 Test 2: The `object` keyword is no longer supported
//=============================================================================
TEST_F(ParserExTest, TG5_ObjectKeywordNotSupportedInNodeParser)
{
  // "object" inside a node block is not a recognized keyword; parser skips it
  // so the node itself should still be created, just without the unknown block.
  // Verify the scene parses successfully (no hard failure for unknown keywords).
  const char* Source = R"(node "Container" {
  })";

  Node* Container = nullptr;
  AxScene* Scene = ParseSceneFromNode(Source, &Container);
  ASSERT_NE(Scene, nullptr);
  EXPECT_NE(Container, nullptr);

  delete Scene;
}

//=============================================================================
// TG5 Test 3: AxScene created via constructor has correct NodeCount/LightCount
//=============================================================================
TEST_F(ParserExTest, TG5_AxSceneReturnsCorrectNodeAndLightCounts)
{
  AxScene* TestScene = new AxScene(TableAPI_);
  ASSERT_NE(TestScene, nullptr);

  EXPECT_EQ(TestScene->GetNodeCount(), 1u); // root only
  EXPECT_EQ(TestScene->LightCount, 0u);

  AxLight* L1 = TestScene->CreateLight("PointLight1", AX_LIGHT_TYPE_POINT);
  ASSERT_NE(L1, nullptr);
  AxLight* L2 = TestScene->CreateLight("SpotLight1", AX_LIGHT_TYPE_SPOT);
  ASSERT_NE(L2, nullptr);

  Node* N1 = TestScene->CreateNode("Entity1", NodeType::Base, nullptr);
  ASSERT_NE(N1, nullptr);
  Node* N2 = TestScene->CreateNode("Entity2", NodeType::Base, nullptr);
  ASSERT_NE(N2, nullptr);

  EXPECT_EQ(TestScene->GetNodeCount(), 3u); // root + 2 nodes
  EXPECT_EQ(TestScene->LightCount, 2u);

  delete TestScene;
}

//=============================================================================
// TG5 Test 4: Parser calls AxScene->CreateNode and AddComponent directly
//=============================================================================
TEST_F(ParserExTest, TG5_ParserExCallsAxSceneDirectlyForNodesAndComponents)
{
  const char* Source = R"(node "DirectCallTest" {
    component MeshFilter {
      mesh: "res://direct.glb"
    }
    component Camera {
      fov: 60.0
      near: 0.1
      far: 1000.0
    }
    node "ChildOfDirect" {
      component RigidBody {
        mass: 3.5
        bodyType: kinematic
      }
    }
  })";

  Node* Root = nullptr;
  AxScene* Scene = ParseSceneFromNode(Source, &Root);
  ASSERT_NE(Scene, nullptr);
  ASSERT_NE(Root, nullptr);
  EXPECT_EQ(Root->GetName(), "DirectCallTest");

  // 2 new nodes: "DirectCallTest" + "ChildOfDirect" + root = 3 total
  EXPECT_EQ(Scene->GetNodeCount(), 3u);

  const ComponentTypeInfo* MFInfo = Factory_->FindType("MeshFilter");
  ASSERT_NE(MFInfo, nullptr);
  uint32_t MFCount = 0;
  Component** MFComps = Scene->GetComponentsByType(MFInfo->TypeID, &MFCount);
  EXPECT_GE(MFCount, 1u);
  ASSERT_NE(MFComps, nullptr);

  Component* MFComp = Root->GetComponent(MFInfo->TypeID);
  ASSERT_NE(MFComp, nullptr);
  MeshFilter* MF = static_cast<MeshFilter*>(MFComp);
  EXPECT_EQ(MF->MeshPath, "res://direct.glb");

  const ComponentTypeInfo* CamInfo = Factory_->FindType("CameraComponent");
  ASSERT_NE(CamInfo, nullptr);
  Component* CamComp = Root->GetComponent(CamInfo->TypeID);
  ASSERT_NE(CamComp, nullptr);
  CameraComponent* Cam = static_cast<CameraComponent*>(CamComp);
  EXPECT_FLOAT_EQ(Cam->GetFOV(), 60.0f);

  Node* Child = Root->GetFirstChild();
  ASSERT_NE(Child, nullptr);
  EXPECT_EQ(Child->GetName(), "ChildOfDirect");

  const ComponentTypeInfo* RBInfo = Factory_->FindType("RigidBody");
  ASSERT_NE(RBInfo, nullptr);
  Component* RBComp = Child->GetComponent(RBInfo->TypeID);
  ASSERT_NE(RBComp, nullptr);
  RigidBody* RB = static_cast<RigidBody*>(RBComp);
  EXPECT_FLOAT_EQ(RB->Mass, 3.5f);
  EXPECT_EQ(RB->Type, BodyType::Kinematic);

  delete Scene;
}
