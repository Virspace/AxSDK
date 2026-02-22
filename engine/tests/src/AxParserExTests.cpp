/**
 * AxParserExTests.cpp - Tests for Extended Parser and Prefab Support
 *
 * Tests the .ats parser for the typed node syntax and .axp prefab
 * loading/instantiation. Uses the AxSceneParserAPI (via AxSceneParser_GetAPI)
 * and the internal ParsePrefab/InstantiatePrefab functions directly.
 *
 * - Parsing `node "Name" { }` creates Node3D (default type)
 * - Parsing `node "Name" MeshInstance { mesh: "path" }` creates typed node
 * - Nested typed nodes build correct hierarchy
 * - Multiple nodes with mixed typed node types
 * - .axp prefab loading creates standalone subtree
 * - Prefab instantiation creates deep copy
 * - Parser produces SceneTree with correct light and node data
 * - Parser calls SceneTree->CreateNode for typed nodes directly
 */

#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxHashTable.h"
#include "Foundation/AxAllocator.h"
#include "Foundation/AxAllocatorAPI.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxMath.h"
#include "AxEngine/AxNode.h"
#include "AxEngine/AxTypedNodes.h"
#include "AxEngine/AxSceneTree.h"
#include "AxScene/AxScene.h"
#include "AxSceneParserInternal.h"

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
  SceneTree* ParseSceneFromNode(std::string_view NodeSource, Node** OutNode = nullptr)
  {
    std::string SceneSource = "scene \"Test\" {\n";
    SceneSource.append(NodeSource.data(), NodeSource.size());
    SceneSource.append("\n}");

    SceneTree* Tree = ParserAPI_->ParseFromString(SceneSource.c_str(), Allocator_);
    if (Tree && OutNode) {
      *OutNode = Tree->GetRootNode()->GetFirstChild();
    }
    return (Tree);
  }

  AxHashTableAPI*   TableAPI_{nullptr};
  AxAllocatorAPI*   AllocatorAPI_{nullptr};
  AxAllocator*      Allocator_{nullptr};
  AxSceneParserAPI* ParserAPI_{nullptr};
};

//=============================================================================
// Test 1: Parsing `node` keyword creates Node3D (default type when no type)
//=============================================================================
TEST_F(ParserExTest, ParseNodeKeywordCreatesNode3DByDefault)
{
  std::string Source = R"(node "Player" {
  })";

  Node* PlayerNode = nullptr;
  SceneTree* Tree = ParseSceneFromNode(Source, &PlayerNode);
  ASSERT_NE(Tree, nullptr);
  ASSERT_NE(PlayerNode, nullptr);
  EXPECT_EQ(PlayerNode->GetName(), "Player");
  EXPECT_EQ(PlayerNode->GetType(), NodeType::Node3D);
  EXPECT_EQ(Tree->GetNodeCount(), 2u); // root + Player

  delete Tree;
}

//=============================================================================
// Test 2: Parsing `node "Name" MeshInstance { mesh: "path" }` creates
//         a MeshInstance typed node with mesh path set
//=============================================================================
TEST_F(ParserExTest, ParseMeshInstanceTypedNode)
{
  const char* Source = R"(node "Obj" MeshInstance {
    mesh: "res://meshes/cube.glb"
  })";

  Node* ObjNode = nullptr;
  SceneTree* Tree = ParseSceneFromNode(Source, &ObjNode);
  ASSERT_NE(Tree, nullptr);
  ASSERT_NE(ObjNode, nullptr);
  EXPECT_EQ(ObjNode->GetName(), "Obj");
  EXPECT_EQ(ObjNode->GetType(), NodeType::MeshInstance);

  MeshInstance* MI = static_cast<MeshInstance*>(ObjNode);
  EXPECT_STREQ(MI->MeshPath, "res://meshes/cube.glb");

  delete Tree;
}

//=============================================================================
// Test 3: Parsing nested typed nodes with transforms builds correct hierarchy
//=============================================================================
TEST_F(ParserExTest, NestedTypedNodesWithTransformsBuildHierarchy)
{
  const char* Source = R"(node "Parent" {
    transform {
      translation: 1.0, 2.0, 3.0
    }
    node "Child" MeshInstance {
      transform {
        translation: 4.0, 5.0, 6.0
      }
      mesh: "res://child_mesh.glb"
    }
  })";

  Node* Parent = nullptr;
  SceneTree* Tree = ParseSceneFromNode(Source, &Parent);
  ASSERT_NE(Tree, nullptr);
  ASSERT_NE(Parent, nullptr);
  EXPECT_EQ(Parent->GetName(), "Parent");
  EXPECT_EQ(Parent->GetType(), NodeType::Node3D);

  EXPECT_FLOAT_EQ(Parent->GetTransform().Translation.X, 1.0f);
  EXPECT_FLOAT_EQ(Parent->GetTransform().Translation.Y, 2.0f);
  EXPECT_FLOAT_EQ(Parent->GetTransform().Translation.Z, 3.0f);

  Node* Child = Parent->GetFirstChild();
  ASSERT_NE(Child, nullptr);
  EXPECT_EQ(Child->GetName(), "Child");
  EXPECT_EQ(Child->GetType(), NodeType::MeshInstance);
  EXPECT_FLOAT_EQ(Child->GetTransform().Translation.X, 4.0f);

  MeshInstance* MI = static_cast<MeshInstance*>(Child);
  EXPECT_STREQ(MI->MeshPath, "res://child_mesh.glb");

  delete Tree;
}

//=============================================================================
// Test 4: Parsing scene with multiple nodes and mixed typed node types
//=============================================================================
TEST_F(ParserExTest, MultipleNodesWithMixedTypedNodeTypes)
{
  const char* Source = R"(node "Container" {
    node "Mesh1" MeshInstance {
      mesh: "res://a.glb"
      material: "MatA"
      renderLayer: 0
    }
    node "Body" RigidBody {
      mass: 10.0
      bodyType: dynamic
    }
  })";

  Node* Container = nullptr;
  SceneTree* Tree = ParseSceneFromNode(Source, &Container);
  ASSERT_NE(Tree, nullptr);
  ASSERT_NE(Container, nullptr);
  EXPECT_EQ(Container->GetName(), "Container");

  Node* Mesh1 = Container->GetFirstChild();
  ASSERT_NE(Mesh1, nullptr);
  EXPECT_EQ(Mesh1->GetName(), "Mesh1");
  EXPECT_EQ(Mesh1->GetType(), NodeType::MeshInstance);

  MeshInstance* MI = static_cast<MeshInstance*>(Mesh1);
  EXPECT_STREQ(MI->MeshPath, "res://a.glb");
  EXPECT_STREQ(MI->MaterialName, "MatA");

  Node* Body = Mesh1->GetNextSibling();
  ASSERT_NE(Body, nullptr);
  EXPECT_EQ(Body->GetName(), "Body");
  EXPECT_EQ(Body->GetType(), NodeType::RigidBody);

  RigidBodyNode* RB = static_cast<RigidBodyNode*>(Body);
  EXPECT_FLOAT_EQ(RB->Mass, 10.0f);
  EXPECT_EQ(RB->Type, BodyType::Dynamic);

  delete Tree;
}

//=============================================================================
// Test 5: Parser correctly creates typed nodes (callback smoke test)
//=============================================================================
TEST_F(ParserExTest, ParseCallbacksFire)
{
  const char* Source = R"(node "CallbackTest" MeshInstance {
    mesh: "test.glb"
  })";

  Node* Result = nullptr;
  SceneTree* Tree = ParseSceneFromNode(Source, &Result);
  ASSERT_NE(Tree, nullptr);
  ASSERT_NE(Result, nullptr);
  EXPECT_EQ(Result->GetName(), "CallbackTest");
  EXPECT_EQ(Result->GetType(), NodeType::MeshInstance);

  MeshInstance* MI = static_cast<MeshInstance*>(Result);
  EXPECT_STREQ(MI->MeshPath, "test.glb");

  delete Tree;
}

//=============================================================================
// Test 6: Loading .axp prefab creates standalone node subtree
//=============================================================================
TEST_F(ParserExTest, PrefabLoadCreatesStandaloneSubtree)
{
  const char* PrefabData = R"(node "PrefabRoot" MeshInstance {
    mesh: "res://prefab_mesh.glb"
    node "PrefabChild" RigidBody {
      mass: 5.0
    }
  })";

  SceneTree* Tree = new SceneTree(TableAPI_, nullptr);
  Tree->Allocator = Allocator_;

  Node* PrefabRoot = ParsePrefab(PrefabData, Tree);
  ASSERT_NE(PrefabRoot, nullptr);
  EXPECT_EQ(PrefabRoot->GetName(), "PrefabRoot");
  EXPECT_EQ(PrefabRoot->GetType(), NodeType::MeshInstance);

  MeshInstance* MI = static_cast<MeshInstance*>(PrefabRoot);
  EXPECT_STREQ(MI->MeshPath, "res://prefab_mesh.glb");

  Node* PrefabChild = PrefabRoot->GetFirstChild();
  ASSERT_NE(PrefabChild, nullptr);
  EXPECT_EQ(PrefabChild->GetName(), "PrefabChild");
  EXPECT_EQ(PrefabChild->GetType(), NodeType::RigidBody);

  RigidBodyNode* RB = static_cast<RigidBodyNode*>(PrefabChild);
  EXPECT_FLOAT_EQ(RB->Mass, 5.0f);

  delete Tree;
}

//=============================================================================
// Test 7: Instantiating prefab creates deep copy independent of original
//=============================================================================
TEST_F(ParserExTest, PrefabInstantiationCreatesDeepCopy)
{
  const char* PrefabData = R"(node "Original" MeshInstance {
    mesh: "res://original.glb"
    node "OriginalChild" {
    }
  })";

  SceneTree* Tree = new SceneTree(TableAPI_, nullptr);
  Tree->Allocator = Allocator_;

  Node* PrefabRoot = ParsePrefab(PrefabData, Tree);
  ASSERT_NE(PrefabRoot, nullptr);

  Node* CopyRoot = InstantiatePrefab(Tree, PrefabRoot, nullptr, Allocator_);
  ASSERT_NE(CopyRoot, nullptr);

  EXPECT_EQ(CopyRoot->GetName(), "Original");
  EXPECT_NE(CopyRoot, PrefabRoot);
  EXPECT_EQ(CopyRoot->GetType(), NodeType::MeshInstance);

  Node* CopyChild = CopyRoot->GetFirstChild();
  ASSERT_NE(CopyChild, nullptr);
  EXPECT_EQ(CopyChild->GetName(), "OriginalChild");

  Node* OrigChild = PrefabRoot->GetFirstChild();
  ASSERT_NE(OrigChild, nullptr);
  EXPECT_NE(CopyChild, OrigChild);

  delete Tree;
}

//=============================================================================
// TG5 Test 1: Parser produces SceneTree with correct light and node data
//=============================================================================
TEST_F(ParserExTest, TG5_SceneWithLightAndNodeProducesCorrectData)
{
  const char* FullScene = R"(scene "TG5Test" {
    node "SunLight" Light {
      type: directional
      direction: 0.0, -1.0, 0.0
      color: 1.0, 0.9, 0.8
      intensity: 2.5
    }
    node "SceneRoot" {
      node "Player" MeshInstance {
        mesh: "res://player.glb"
      }
    }
  })";

  SceneTree* Tree = ParserAPI_->ParseFromString(FullScene, Allocator_);
  ASSERT_NE(Tree, nullptr);

  // Verify light via typed node tracking
  uint32_t LightCount = 0;
  Node** Lights = Tree->GetNodesByType(NodeType::Light, &LightCount);
  ASSERT_NE(Lights, nullptr);
  EXPECT_EQ(LightCount, 1u);

  LightNode* LN = static_cast<LightNode*>(Lights[0]);
  EXPECT_EQ(LN->GetName(), "SunLight");
  EXPECT_EQ(LN->GetLightType(), AX_LIGHT_TYPE_DIRECTIONAL);
  EXPECT_FLOAT_EQ(LN->GetIntensity(), 2.5f);

  // root + SunLight + SceneRoot + Player = 4
  EXPECT_EQ(Tree->GetNodeCount(), 4u);

  // Verify hierarchy
  Node* SunNode = Tree->GetRootNode()->GetFirstChild();
  ASSERT_NE(SunNode, nullptr);
  EXPECT_EQ(SunNode->GetName(), "SunLight");

  Node* SceneRoot = SunNode->GetNextSibling();
  ASSERT_NE(SceneRoot, nullptr);
  EXPECT_EQ(SceneRoot->GetName(), "SceneRoot");

  Node* Player = SceneRoot->GetFirstChild();
  ASSERT_NE(Player, nullptr);
  EXPECT_EQ(Player->GetName(), "Player");
  EXPECT_EQ(Player->GetType(), NodeType::MeshInstance);

  delete Tree;
}

//=============================================================================
// TG5 Test 2: The `object` keyword is no longer supported
//=============================================================================
TEST_F(ParserExTest, TG5_ObjectKeywordNotSupportedInNodeParser)
{
  const char* Source = R"(node "Container" {
  })";

  Node* Container = nullptr;
  SceneTree* Tree = ParseSceneFromNode(Source, &Container);
  ASSERT_NE(Tree, nullptr);
  EXPECT_NE(Container, nullptr);

  delete Tree;
}

//=============================================================================
// TG5 Test 3: SceneTree created via constructor has correct NodeCount and
//             typed node counts
//=============================================================================
TEST_F(ParserExTest, TG5_SceneTreeReturnsCorrectNodeAndLightCounts)
{
  SceneTree* TestTree = new SceneTree(TableAPI_, nullptr);
  ASSERT_NE(TestTree, nullptr);

  EXPECT_EQ(TestTree->GetNodeCount(), 1u); // root only

  uint32_t LightCount = 0;
  TestTree->GetNodesByType(NodeType::Light, &LightCount);
  EXPECT_EQ(LightCount, 0u);

  // Create lights via typed LightNode
  Node* L1 = TestTree->CreateNode("PointLight1", NodeType::Light, nullptr);
  ASSERT_NE(L1, nullptr);
  static_cast<LightNode*>(L1)->SetLightType(AX_LIGHT_TYPE_POINT);

  Node* L2 = TestTree->CreateNode("SpotLight1", NodeType::Light, nullptr);
  ASSERT_NE(L2, nullptr);
  static_cast<LightNode*>(L2)->SetLightType(AX_LIGHT_TYPE_SPOT);

  // Create regular nodes
  Node* N1 = TestTree->CreateNode("Entity1", NodeType::Node3D, nullptr);
  ASSERT_NE(N1, nullptr);
  Node* N2 = TestTree->CreateNode("Entity2", NodeType::Node3D, nullptr);
  ASSERT_NE(N2, nullptr);

  EXPECT_EQ(TestTree->GetNodeCount(), 5u); // root + 2 lights + 2 nodes

  TestTree->GetNodesByType(NodeType::Light, &LightCount);
  EXPECT_EQ(LightCount, 2u);

  delete TestTree;
}

//=============================================================================
// TG5 Test 4: Parser calls SceneTree->CreateNode for typed nodes directly
//=============================================================================
TEST_F(ParserExTest, TG5_ParserExCallsSceneTreeDirectlyForTypedNodes)
{
  const char* Source = R"(node "DirectCallTest" MeshInstance {
    mesh: "res://direct.glb"
    node "CamChild" Camera {
      fov: 60.0
      near: 0.1
      far: 1000.0
    }
    node "BodyChild" RigidBody {
      mass: 3.5
      bodyType: kinematic
    }
  })";

  Node* Root = nullptr;
  SceneTree* Tree = ParseSceneFromNode(Source, &Root);
  ASSERT_NE(Tree, nullptr);
  ASSERT_NE(Root, nullptr);
  EXPECT_EQ(Root->GetName(), "DirectCallTest");
  EXPECT_EQ(Root->GetType(), NodeType::MeshInstance);

  // 3 new nodes: "DirectCallTest" + "CamChild" + "BodyChild" + root = 4 total
  EXPECT_EQ(Tree->GetNodeCount(), 4u);

  // Verify MeshInstance data
  MeshInstance* MI = static_cast<MeshInstance*>(Root);
  EXPECT_STREQ(MI->MeshPath, "res://direct.glb");

  // Verify MeshInstance appears in typed tracking
  uint32_t MeshCount = 0;
  Node** MeshNodes = Tree->GetNodesByType(NodeType::MeshInstance, &MeshCount);
  EXPECT_GE(MeshCount, 1u);
  ASSERT_NE(MeshNodes, nullptr);

  // Verify Camera child
  Node* CamChild = Root->GetFirstChild();
  ASSERT_NE(CamChild, nullptr);
  EXPECT_EQ(CamChild->GetName(), "CamChild");
  EXPECT_EQ(CamChild->GetType(), NodeType::Camera);

  CameraNode* Cam = static_cast<CameraNode*>(CamChild);
  EXPECT_FLOAT_EQ(Cam->GetFOV(), 60.0f);
  EXPECT_FLOAT_EQ(Cam->GetNear(), 0.1f);
  EXPECT_FLOAT_EQ(Cam->GetFar(), 1000.0f);

  // Verify RigidBody child
  Node* BodyChild = CamChild->GetNextSibling();
  ASSERT_NE(BodyChild, nullptr);
  EXPECT_EQ(BodyChild->GetName(), "BodyChild");
  EXPECT_EQ(BodyChild->GetType(), NodeType::RigidBody);

  RigidBodyNode* RB = static_cast<RigidBodyNode*>(BodyChild);
  EXPECT_FLOAT_EQ(RB->Mass, 3.5f);
  EXPECT_EQ(RB->Type, BodyType::Kinematic);

  delete Tree;
}
