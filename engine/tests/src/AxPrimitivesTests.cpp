/**
 * AxPrimitivesTests.cpp - Tests for Procedural Primitive Mesh Generation
 *
 * Tests cover:
 * - Task Group 1: Infrastructure (null-safety)
 * - Task Group 2: Geometry generation (vertex/index counts, normals, UVs, tangents)
 * - Task Group 3: ResourceAPI integration (tested indirectly via geometry output)
 * - Task Group 4: SceneParser primitive syntax
 * - Task Group 6: Edge cases and gap analysis
 */

#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxHashTable.h"
#include "Foundation/AxAllocator.h"
#include "Foundation/AxAllocatorAPI.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxMath.h"
#include "AxEngine/AxPrimitives.h"
#include "AxEngine/AxNode.h"
#include "AxEngine/AxTypedNodes.h"
#include "AxEngine/AxSceneTree.h"
#include "AxEngine/AxSceneParser.h"
#include "AxOpenGL/AxOpenGLTypes.h"
#include "AxResource/AxResourceTypes.h"

#include <cstdlib>
#include <cstring>

//=============================================================================
// Task Group 1: Infrastructure Tests (null-safety - no registry initialized)
//=============================================================================

// Test 1.1c: BoxMesh().CreateModel() returns AX_INVALID_HANDLE when ResourceAPI unavailable
TEST(PrimitivesNullSafetyTest, CreateBoxMeshReturnsInvalidHandleWithoutResourceAPI)
{
    AxModelHandle Handle = BoxMesh().CreateModel();
    EXPECT_FALSE(AX_HANDLE_IS_VALID(Handle));
}

// Test 1.1d: PlaneMesh().CreateModel() returns AX_INVALID_HANDLE when ResourceAPI unavailable
TEST(PrimitivesNullSafetyTest, CreatePlaneMeshReturnsInvalidHandleWithoutResourceAPI)
{
    AxModelHandle Handle = PlaneMesh().CreateModel();
    EXPECT_FALSE(AX_HANDLE_IS_VALID(Handle));
}

//=============================================================================
// Test Fixture for Geometry Generation Tests
//=============================================================================

class PrimitivesGeometryTest : public testing::Test
{
protected:
    void FreeMeshData(GeneratedMeshData& Data)
    {
        free(Data.Vertices);
        free(Data.Indices);
        Data.Vertices = nullptr;
        Data.Indices = nullptr;
    }
};

//=============================================================================
// Task Group 2: Geometry Generation Tests
//=============================================================================

// Test 2.1a: BoxMesh produces exactly 24 vertices and 36 indices
TEST_F(PrimitivesGeometryTest, BoxProduces24Vertices36Indices)
{
    GeneratedMeshData Data = BoxMesh().GenerateGeometry();

    ASSERT_NE(Data.Vertices, nullptr);
    ASSERT_NE(Data.Indices, nullptr);
    EXPECT_EQ(Data.VertexCount, 24u);
    EXPECT_EQ(Data.IndexCount, 36u);

    FreeMeshData(Data);
}

// Test 2.1b: Box vertices have non-zero normals all normalized to length 1.0
TEST_F(PrimitivesGeometryTest, BoxNormalsAreUnitLength)
{
    GeneratedMeshData Data = BoxMesh().GenerateGeometry();
    ASSERT_NE(Data.Vertices, nullptr);

    for (uint32_t i = 0; i < Data.VertexCount; ++i) {
        float Len = Vec3Length(Data.Vertices[i].Normal);
        EXPECT_NEAR(Len, 1.0f, 0.001f) << "Normal at vertex " << i << " is not unit length";
    }

    FreeMeshData(Data);
}

// Test 2.1c: Sphere with Rings=4, Segments=8 produces expected vertex count
TEST_F(PrimitivesGeometryTest, SphereVertexCountMatchesFormula)
{
    GeneratedMeshData Data = SphereMesh().SetRings(4).SetSegments(8).GenerateGeometry();
    ASSERT_NE(Data.Vertices, nullptr);

    uint32_t ExpectedVerts = (4 + 1) * (8 + 1); // 45
    EXPECT_EQ(Data.VertexCount, ExpectedVerts);

    FreeMeshData(Data);
}

// Test 2.1d: Sphere normals are unit length (spot-check first and last vertex)
TEST_F(PrimitivesGeometryTest, SphereNormalsAreUnitLength)
{
    GeneratedMeshData Data = SphereMesh().GenerateGeometry();
    ASSERT_NE(Data.Vertices, nullptr);
    ASSERT_GT(Data.VertexCount, 1u);

    // First vertex
    float FirstLen = Vec3Length(Data.Vertices[0].Normal);
    EXPECT_NEAR(FirstLen, 1.0f, 0.001f);

    // Last vertex
    float LastLen = Vec3Length(Data.Vertices[Data.VertexCount - 1].Normal);
    EXPECT_NEAR(LastLen, 1.0f, 0.001f);

    FreeMeshData(Data);
}

// Test 2.1e: Plane with SubdivisionsX=2, SubdivisionsZ=2 produces 9 vertices
TEST_F(PrimitivesGeometryTest, PlaneVertexCountMatchesFormula)
{
    GeneratedMeshData Data = PlaneMesh().SetSubdivisionsX(2).SetSubdivisionsZ(2).GenerateGeometry();
    ASSERT_NE(Data.Vertices, nullptr);

    uint32_t ExpectedVerts = (2 + 1) * (2 + 1); // 9
    EXPECT_EQ(Data.VertexCount, ExpectedVerts);

    uint32_t ExpectedIndices = 2 * 2 * 6; // 24
    EXPECT_EQ(Data.IndexCount, ExpectedIndices);

    FreeMeshData(Data);
}

// Test 2.1f: Cylinder with no caps produces only side wall vertices
TEST_F(PrimitivesGeometryTest, CylinderNoCapsProducesSideOnlyVertices)
{
    GeneratedMeshData Data = CylinderMesh()
        .SetSegments(8)
        .SetCapTop(false)
        .SetCapBottom(false)
        .GenerateGeometry();
    ASSERT_NE(Data.Vertices, nullptr);

    uint32_t ExpectedSideVerts = (8 + 1) * 2; // 18
    EXPECT_EQ(Data.VertexCount, ExpectedSideVerts);

    FreeMeshData(Data);
}

// Test 2.1g: Capsule returns valid data with default params
TEST_F(PrimitivesGeometryTest, CapsuleDefaultParamsProducesValidData)
{
    GeneratedMeshData Data = CapsuleMesh().GenerateGeometry();

    ASSERT_NE(Data.Vertices, nullptr);
    ASSERT_NE(Data.Indices, nullptr);
    EXPECT_GT(Data.VertexCount, 0u);
    EXPECT_GT(Data.IndexCount, 0u);

    FreeMeshData(Data);
}

// Test 2.1h: All five primitives produce valid data with default parameters
TEST_F(PrimitivesGeometryTest, AllFiveDefaultParamsProduceValidData)
{
    // Box
    {
        GeneratedMeshData D = BoxMesh().GenerateGeometry();
        EXPECT_NE(D.Vertices, nullptr);
        EXPECT_GT(D.VertexCount, 0u);
        EXPECT_GT(D.IndexCount, 0u);
        FreeMeshData(D);
    }
    // Sphere
    {
        GeneratedMeshData D = SphereMesh().GenerateGeometry();
        EXPECT_NE(D.Vertices, nullptr);
        EXPECT_GT(D.VertexCount, 0u);
        EXPECT_GT(D.IndexCount, 0u);
        FreeMeshData(D);
    }
    // Plane
    {
        GeneratedMeshData D = PlaneMesh().GenerateGeometry();
        EXPECT_NE(D.Vertices, nullptr);
        EXPECT_GT(D.VertexCount, 0u);
        EXPECT_GT(D.IndexCount, 0u);
        FreeMeshData(D);
    }
    // Cylinder
    {
        GeneratedMeshData D = CylinderMesh().GenerateGeometry();
        EXPECT_NE(D.Vertices, nullptr);
        EXPECT_GT(D.VertexCount, 0u);
        EXPECT_GT(D.IndexCount, 0u);
        FreeMeshData(D);
    }
    // Capsule
    {
        GeneratedMeshData D = CapsuleMesh().GenerateGeometry();
        EXPECT_NE(D.Vertices, nullptr);
        EXPECT_GT(D.VertexCount, 0u);
        EXPECT_GT(D.IndexCount, 0u);
        FreeMeshData(D);
    }
}

//=============================================================================
// Task Group 2 Additional: Tangent W component is 1.0 for all vertices
//=============================================================================

TEST_F(PrimitivesGeometryTest, BoxTangentWIsOne)
{
    GeneratedMeshData Data = BoxMesh().GenerateGeometry();
    ASSERT_NE(Data.Vertices, nullptr);

    for (uint32_t i = 0; i < Data.VertexCount; ++i) {
        EXPECT_FLOAT_EQ(Data.Vertices[i].Tangent.W, 1.0f)
            << "Tangent W at vertex " << i << " is not 1.0";
    }

    FreeMeshData(Data);
}

TEST_F(PrimitivesGeometryTest, SphereTangentWIsOne)
{
    GeneratedMeshData Data = SphereMesh().GenerateGeometry();
    ASSERT_NE(Data.Vertices, nullptr);

    for (uint32_t i = 0; i < Data.VertexCount; ++i) {
        EXPECT_FLOAT_EQ(Data.Vertices[i].Tangent.W, 1.0f)
            << "Tangent W at vertex " << i << " is not 1.0";
    }

    FreeMeshData(Data);
}

//=============================================================================
// Task Group 2 Additional: Plane normals are all (0,1,0)
//=============================================================================

TEST_F(PrimitivesGeometryTest, PlaneNormalsAllFaceUp)
{
    GeneratedMeshData Data = PlaneMesh().GenerateGeometry();
    ASSERT_NE(Data.Vertices, nullptr);

    for (uint32_t i = 0; i < Data.VertexCount; ++i) {
        EXPECT_FLOAT_EQ(Data.Vertices[i].Normal.X, 0.0f);
        EXPECT_FLOAT_EQ(Data.Vertices[i].Normal.Y, 1.0f);
        EXPECT_FLOAT_EQ(Data.Vertices[i].Normal.Z, 0.0f);
    }

    FreeMeshData(Data);
}

//=============================================================================
// Task Group 2 Additional: Plane UVs span [0,1]
//=============================================================================

TEST_F(PrimitivesGeometryTest, PlaneUVsSpanZeroToOne)
{
    GeneratedMeshData Data = PlaneMesh()
        .SetWidth(5.0f).SetDepth(5.0f)
        .SetSubdivisionsX(4).SetSubdivisionsZ(4)
        .GenerateGeometry();
    ASSERT_NE(Data.Vertices, nullptr);

    float MinU = 1.0f, MaxU = 0.0f;
    float MinV = 1.0f, MaxV = 0.0f;
    for (uint32_t i = 0; i < Data.VertexCount; ++i) {
        float U = Data.Vertices[i].TexCoord.U;
        float V = Data.Vertices[i].TexCoord.V;
        if (U < MinU) MinU = U;
        if (U > MaxU) MaxU = U;
        if (V < MinV) MinV = V;
        if (V > MaxV) MaxV = V;
    }

    EXPECT_NEAR(MinU, 0.0f, 0.001f);
    EXPECT_NEAR(MaxU, 1.0f, 0.001f);
    EXPECT_NEAR(MinV, 0.0f, 0.001f);
    EXPECT_NEAR(MaxV, 1.0f, 0.001f);

    FreeMeshData(Data);
}

//=============================================================================
// Task Group 2 Additional: Cylinder with both caps has correct vertex count
//=============================================================================

TEST_F(PrimitivesGeometryTest, CylinderWithBothCapsVertexCount)
{
    GeneratedMeshData Data = CylinderMesh().SetSegments(8).GenerateGeometry();
    ASSERT_NE(Data.Vertices, nullptr);

    uint32_t SideVerts = (8 + 1) * 2;         // 18
    uint32_t TopCapVerts = 8 + 2;              // 10 (center + rim)
    uint32_t BottomCapVerts = 8 + 2;           // 10
    uint32_t ExpectedTotal = SideVerts + TopCapVerts + BottomCapVerts; // 38

    EXPECT_EQ(Data.VertexCount, ExpectedTotal);

    FreeMeshData(Data);
}

//=============================================================================
// Task Group 4: SceneParser Primitive Syntax Tests
//=============================================================================

class PrimitivesParserTest : public testing::Test
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

        Allocator_ = AllocatorAPI_->CreateHeap("TestPrimParserAlloc", 64 * 1024, 256 * 1024);
        ASSERT_NE(Allocator_, nullptr);

        Parser_.Init(AxonGlobalAPIRegistry);
    }

    void TearDown() override
    {
        Parser_.Term();

        if (Allocator_) {
            Allocator_->Destroy(Allocator_);
            Allocator_ = nullptr;
        }

        AxonTermGlobalAPIRegistry();
    }

    SceneTree* ParseSceneFromNode(const char* NodeSource, Node** OutNode = nullptr)
    {
        std::string SceneSource = "scene \"Test\" {\n";
        SceneSource.append(NodeSource);
        SceneSource.append("\n}");

        SceneTree* Tree = Parser_.ParseFromString(SceneSource.c_str(), Allocator_);
        if (Tree && OutNode) {
            *OutNode = Tree->GetRootNode()->GetFirstChild();
        }
        return (Tree);
    }

    AxHashTableAPI*   TableAPI_{nullptr};
    AxAllocatorAPI*   AllocatorAPI_{nullptr};
    AxAllocator*      Allocator_{nullptr};
    SceneParser       Parser_;
};

// Test 4.1a: Parsing primitive box sets MeshPath empty and flags primitive
TEST_F(PrimitivesParserTest, PrimitiveBoxSyntaxParsesCorrectly)
{
    const char* Source = R"(node "Floor" MeshInstance {
        mesh: primitive box { width: 2.0 height: 0.1 depth: 2.0 }
    })";

    Node* FloorNode = nullptr;
    SceneTree* Tree = ParseSceneFromNode(Source, &FloorNode);
    ASSERT_NE(Tree, nullptr);
    ASSERT_NE(FloorNode, nullptr);
    EXPECT_EQ(FloorNode->GetType(), NodeType::MeshInstance);

    MeshInstance* MI = static_cast<MeshInstance*>(FloorNode);
    // MeshPath should be empty (primitive, not file-based)
    EXPECT_EQ(MI->MeshPath[0], '\0');

    delete Tree;
}

// Test 4.1b: Parsing primitive sphere with no parameters uses defaults
TEST_F(PrimitivesParserTest, PrimitiveSphereEmptyParamsUsesDefaults)
{
    const char* Source = R"(node "Ball" MeshInstance {
        mesh: primitive sphere { }
    })";

    Node* BallNode = nullptr;
    SceneTree* Tree = ParseSceneFromNode(Source, &BallNode);
    ASSERT_NE(Tree, nullptr);
    ASSERT_NE(BallNode, nullptr);
    EXPECT_EQ(BallNode->GetType(), NodeType::MeshInstance);

    MeshInstance* MI = static_cast<MeshInstance*>(BallNode);
    EXPECT_EQ(MI->MeshPath[0], '\0');

    delete Tree;
}

// Test 4.1c: Parsing primitive plane reads float parameters correctly
TEST_F(PrimitivesParserTest, PrimitivePlaneReadsFloatParams)
{
    const char* Source = R"(node "Ground" MeshInstance {
        mesh: primitive plane { width: 10.0 depth: 10.0 }
    })";

    Node* GroundNode = nullptr;
    SceneTree* Tree = ParseSceneFromNode(Source, &GroundNode);
    ASSERT_NE(Tree, nullptr);
    ASSERT_NE(GroundNode, nullptr);
    EXPECT_EQ(GroundNode->GetType(), NodeType::MeshInstance);

    MeshInstance* MI = static_cast<MeshInstance*>(GroundNode);
    EXPECT_EQ(MI->MeshPath[0], '\0');

    delete Tree;
}

// Test 4.1d: Parsing primitive cylinder with mixed params
TEST_F(PrimitivesParserTest, PrimitiveCylinderMixedParams)
{
    const char* Source = R"(node "Pillar" MeshInstance {
        mesh: primitive cylinder { radius: 0.3 height: 2.0 segments: 24 }
    })";

    Node* PillarNode = nullptr;
    SceneTree* Tree = ParseSceneFromNode(Source, &PillarNode);
    ASSERT_NE(Tree, nullptr);
    ASSERT_NE(PillarNode, nullptr);
    EXPECT_EQ(PillarNode->GetType(), NodeType::MeshInstance);

    MeshInstance* MI = static_cast<MeshInstance*>(PillarNode);
    EXPECT_EQ(MI->MeshPath[0], '\0');

    delete Tree;
}

// Test 4.1e: Parsing primitive capsule produces a valid primitive node
TEST_F(PrimitivesParserTest, PrimitiveCapsuleProducesValidNode)
{
    const char* Source = R"(node "Capsule" MeshInstance {
        mesh: primitive capsule { radius: 0.25 height: 1.5 rings: 8 segments: 16 }
    })";

    Node* CapsuleNode = nullptr;
    SceneTree* Tree = ParseSceneFromNode(Source, &CapsuleNode);
    ASSERT_NE(Tree, nullptr);
    ASSERT_NE(CapsuleNode, nullptr);
    EXPECT_EQ(CapsuleNode->GetType(), NodeType::MeshInstance);

    MeshInstance* MI = static_cast<MeshInstance*>(CapsuleNode);
    EXPECT_EQ(MI->MeshPath[0], '\0');

    delete Tree;
}

//=============================================================================
// Task Group 4 Additional: Existing mesh path syntax still works
//=============================================================================

TEST_F(PrimitivesParserTest, ExistingMeshPathSyntaxStillWorks)
{
    const char* Source = R"(node "Model" MeshInstance {
        mesh: "models/scene.glb"
    })";

    Node* ModelNode = nullptr;
    SceneTree* Tree = ParseSceneFromNode(Source, &ModelNode);
    ASSERT_NE(Tree, nullptr);
    ASSERT_NE(ModelNode, nullptr);
    EXPECT_EQ(ModelNode->GetType(), NodeType::MeshInstance);

    MeshInstance* MI = static_cast<MeshInstance*>(ModelNode);
    EXPECT_STREQ(MI->MeshPath, "models/scene.glb");

    delete Tree;
}

//=============================================================================
// Task Group 4 Additional: Mixed primitives and file meshes in one scene
//=============================================================================

TEST_F(PrimitivesParserTest, MixedPrimitivesAndFileMeshes)
{
    const char* Source = R"(scene "Mixed" {
        node "Sponza" MeshInstance {
            mesh: "models/atrium.glb"
        }
        node "GroundPlane" MeshInstance {
            mesh: primitive plane { width: 10.0 depth: 10.0 }
        }
        node "Marker" MeshInstance {
            mesh: primitive sphere { }
        }
    })";

    SceneTree* Tree = Parser_.ParseFromString(Source, Allocator_);
    ASSERT_NE(Tree, nullptr);

    // Should have root + 3 nodes = 4
    EXPECT_EQ(Tree->GetNodeCount(), 4u);

    // Verify Sponza has a mesh path
    Node* Sponza = Tree->GetRootNode()->GetFirstChild();
    ASSERT_NE(Sponza, nullptr);
    EXPECT_STREQ(static_cast<MeshInstance*>(Sponza)->MeshPath, "models/atrium.glb");

    // Verify GroundPlane has empty mesh path (primitive)
    Node* Ground = Sponza->GetNextSibling();
    ASSERT_NE(Ground, nullptr);
    EXPECT_EQ(static_cast<MeshInstance*>(Ground)->MeshPath[0], '\0');

    // Verify Marker has empty mesh path (primitive)
    Node* Marker = Ground->GetNextSibling();
    ASSERT_NE(Marker, nullptr);
    EXPECT_EQ(static_cast<MeshInstance*>(Marker)->MeshPath[0], '\0');

    delete Tree;
}

//=============================================================================
// Task Group 6: Edge Cases and Gap Analysis
//=============================================================================

// Edge case: zero-dimension box should not crash
TEST_F(PrimitivesGeometryTest, ZeroDimensionBoxDoesNotCrash)
{
    GeneratedMeshData Data = BoxMesh().SetWidth(0.0f).SetHeight(0.0f).SetDepth(0.0f).GenerateGeometry();
    ASSERT_NE(Data.Vertices, nullptr);
    EXPECT_EQ(Data.VertexCount, 24u);
    EXPECT_EQ(Data.IndexCount, 36u);

    FreeMeshData(Data);
}

// Edge case: very high subdivision count for sphere
TEST_F(PrimitivesGeometryTest, HighSubdivisionSphereProducesValidMesh)
{
    GeneratedMeshData Data = SphereMesh().SetRadius(1.0f).SetRings(64).SetSegments(128).GenerateGeometry();
    ASSERT_NE(Data.Vertices, nullptr);
    ASSERT_NE(Data.Indices, nullptr);

    uint32_t ExpectedVerts = (64 + 1) * (128 + 1);
    EXPECT_EQ(Data.VertexCount, ExpectedVerts);

    // Spot check: first vertex normal should be unit length
    float Len = Vec3Length(Data.Vertices[0].Normal);
    EXPECT_NEAR(Len, 1.0f, 0.001f);

    FreeMeshData(Data);
}

// Edge case: Capsule height less than 2*radius (cylinder portion collapses to 0)
TEST_F(PrimitivesGeometryTest, CapsuleHeightLessThanDiameterDoesNotCrash)
{
    GeneratedMeshData Data = CapsuleMesh()
        .SetRadius(1.0f).SetHeight(0.5f)
        .SetRings(4).SetSegments(8)
        .GenerateGeometry();
    ASSERT_NE(Data.Vertices, nullptr);
    ASSERT_NE(Data.Indices, nullptr);
    EXPECT_GT(Data.VertexCount, 0u);
    EXPECT_GT(Data.IndexCount, 0u);

    FreeMeshData(Data);
}

// Box UVs should span [0,1] on each face
TEST_F(PrimitivesGeometryTest, BoxUVsInValidRange)
{
    GeneratedMeshData Data = BoxMesh().GenerateGeometry();
    ASSERT_NE(Data.Vertices, nullptr);

    for (uint32_t i = 0; i < Data.VertexCount; ++i) {
        float U = Data.Vertices[i].TexCoord.U;
        float V = Data.Vertices[i].TexCoord.V;
        EXPECT_GE(U, 0.0f) << "UV U out of range at vertex " << i;
        EXPECT_LE(U, 1.0f) << "UV U out of range at vertex " << i;
        EXPECT_GE(V, 0.0f) << "UV V out of range at vertex " << i;
        EXPECT_LE(V, 1.0f) << "UV V out of range at vertex " << i;
    }

    FreeMeshData(Data);
}

// Sphere UVs should span [0,1]
TEST_F(PrimitivesGeometryTest, SphereUVsInValidRange)
{
    GeneratedMeshData Data = SphereMesh().GenerateGeometry();
    ASSERT_NE(Data.Vertices, nullptr);

    for (uint32_t i = 0; i < Data.VertexCount; ++i) {
        float U = Data.Vertices[i].TexCoord.U;
        float V = Data.Vertices[i].TexCoord.V;
        EXPECT_GE(U, -0.001f) << "UV U out of range at vertex " << i;
        EXPECT_LE(U, 1.001f) << "UV U out of range at vertex " << i;
        EXPECT_GE(V, -0.001f) << "UV V out of range at vertex " << i;
        EXPECT_LE(V, 1.001f) << "UV V out of range at vertex " << i;
    }

    FreeMeshData(Data);
}

// Cylinder side normals should be horizontal (Y=0) and unit length
TEST_F(PrimitivesGeometryTest, CylinderSideNormalsAreHorizontalAndUnitLength)
{
    GeneratedMeshData Data = CylinderMesh()
        .SetSegments(16)
        .SetCapTop(false)
        .SetCapBottom(false)
        .GenerateGeometry();
    ASSERT_NE(Data.Vertices, nullptr);

    for (uint32_t i = 0; i < Data.VertexCount; ++i) {
        EXPECT_FLOAT_EQ(Data.Vertices[i].Normal.Y, 0.0f)
            << "Side normal Y should be 0 at vertex " << i;
        float Len = Vec3Length(Data.Vertices[i].Normal);
        EXPECT_NEAR(Len, 1.0f, 0.001f)
            << "Side normal not unit length at vertex " << i;
    }

    FreeMeshData(Data);
}

// All indices should be within vertex count bounds
TEST_F(PrimitivesGeometryTest, AllIndicesWithinBoundsForAllPrimitives)
{
    auto CheckBounds = [](const GeneratedMeshData& D, const char* Name) {
        for (uint32_t i = 0; i < D.IndexCount; ++i) {
            EXPECT_LT(D.Indices[i], D.VertexCount)
                << Name << " index " << i << " out of bounds: "
                << D.Indices[i] << " >= " << D.VertexCount;
        }
    };

    {
        GeneratedMeshData D = BoxMesh().GenerateGeometry();
        ASSERT_NE(D.Vertices, nullptr);
        CheckBounds(D, "Box");
        free(D.Vertices); free(D.Indices);
    }
    {
        GeneratedMeshData D = SphereMesh().GenerateGeometry();
        ASSERT_NE(D.Vertices, nullptr);
        CheckBounds(D, "Sphere");
        free(D.Vertices); free(D.Indices);
    }
    {
        GeneratedMeshData D = PlaneMesh().GenerateGeometry();
        ASSERT_NE(D.Vertices, nullptr);
        CheckBounds(D, "Plane");
        free(D.Vertices); free(D.Indices);
    }
    {
        GeneratedMeshData D = CylinderMesh().GenerateGeometry();
        ASSERT_NE(D.Vertices, nullptr);
        CheckBounds(D, "Cylinder");
        free(D.Vertices); free(D.Indices);
    }
    {
        GeneratedMeshData D = CapsuleMesh().GenerateGeometry();
        ASSERT_NE(D.Vertices, nullptr);
        CheckBounds(D, "Capsule");
        free(D.Vertices); free(D.Indices);
    }
}
