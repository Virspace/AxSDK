/**
 * ResourceAPITests.cpp - Tests for Resource API Core Functionality
 *
 * Tests:
 * - Initialize/Shutdown lifecycle
 * - Handle validity and generation tracking
 * - Reference counting (acquire/release)
 * - Deferred destruction
 * - Resource loading (graceful failure without GPU context)
 * - Model loading (Phase 4)
 * - Statistics and debugging (Phase 4)
 *
 * Phase 3: Updated tests to verify that resource loading functions
 * fail gracefully when dependencies (AxOpenGLAPI, real files) are not available.
 *
 * Phase 4: Added tests for LoadModel, ReleaseModel, GetStats, and ref count getters.
 */

#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "AxEngine/AxRenderTypes.h"
#include "Foundation/AxAllocatorAPI.h"
#include "Foundation/AxAllocator.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxPlugin.h"
#include "AxResource/AxResource.h"
#include "AxResource/AxResourceTypes.h"

#include <cstring>

//=============================================================================
// Test Fixture
//=============================================================================

class ResourceAPITest : public testing::Test
{
protected:
    struct AxAllocator* TestAllocator;
    struct AxResourceAPI* API;
    struct AxAllocatorAPI* AllocatorAPI;
    struct AxPluginAPI* PluginAPI;

    void SetUp() override
    {
        // Initialize global API registry
        AxonInitGlobalAPIRegistry();
        AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);

        // Get PluginAPI to load the Resource plugin
        PluginAPI = (struct AxPluginAPI*)AxonGlobalAPIRegistry->Get(AXON_PLUGIN_API_NAME);
        ASSERT_NE(PluginAPI, nullptr) << "PluginAPI not available";

        // Get the AllocatorAPI from the global registry
        AllocatorAPI = (struct AxAllocatorAPI*)AxonGlobalAPIRegistry->Get(AXON_ALLOCATOR_API_NAME);
        ASSERT_NE(AllocatorAPI, nullptr) << "AllocatorAPI not available";

        // Load the Resource plugin
        PluginAPI->Load("libAxResource.dll", false);

        // Get the ResourceAPI from the global registry
        API = (struct AxResourceAPI*)AxonGlobalAPIRegistry->Get(AXON_RESOURCE_API_NAME);
        ASSERT_NE(API, nullptr) << "ResourceAPI not available";

        // Create a heap allocator for testing
        TestAllocator = AllocatorAPI->CreateHeap("ResourceTest", Megabytes(8), Megabytes(16));
        ASSERT_NE(TestAllocator, nullptr) << "Failed to create test allocator";
    }

    void TearDown() override
    {
        // Shutdown if initialized
        if (API && API->IsInitialized()) {
            API->Shutdown();
        }

        // Destroy test allocator
        if (TestAllocator) {
            TestAllocator->Destroy(TestAllocator);
            TestAllocator = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests (Task 8)
//=============================================================================

TEST_F(ResourceAPITest, InitializeWithValidParameters)
{
    // Initialize should succeed with valid registry and allocator
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    EXPECT_TRUE(API->IsInitialized());
}

TEST_F(ResourceAPITest, InitializeWithNullRegistry)
{
    // Initialize should fail gracefully with null registry
    API->Initialize(nullptr, TestAllocator, nullptr);

    // Should not be initialized
    EXPECT_FALSE(API->IsInitialized());
}

TEST_F(ResourceAPITest, InitializeWithNullAllocator)
{
    // Initialize should fail gracefully with null allocator
    API->Initialize(AxonGlobalAPIRegistry, nullptr, nullptr);

    // Should not be initialized
    EXPECT_FALSE(API->IsInitialized());
}

TEST_F(ResourceAPITest, InitializeIdempotent)
{
    // Multiple Initialize calls should be safe
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);
    EXPECT_TRUE(API->IsInitialized());

    // Second initialize should succeed (or be a no-op)
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);
    EXPECT_TRUE(API->IsInitialized());
}

TEST_F(ResourceAPITest, ShutdownWhenNotInitialized)
{
    // Shutdown when not initialized should be safe
    EXPECT_FALSE(API->IsInitialized());
    API->Shutdown();
    EXPECT_FALSE(API->IsInitialized());
}

TEST_F(ResourceAPITest, ShutdownReleasesResources)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);
    EXPECT_TRUE(API->IsInitialized());

    API->Shutdown();
    EXPECT_FALSE(API->IsInitialized());
}

TEST_F(ResourceAPITest, InitializeWithCustomOptions)
{
    struct AxResourceInitOptions Options;
    memset(&Options, 0, sizeof(Options));
    Options.InitialTextureCapacity = 128;
    Options.InitialMeshCapacity = 128;
    Options.InitialShaderCapacity = 64;
    Options.InitialMaterialCapacity = 128;

    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, &Options);
    EXPECT_TRUE(API->IsInitialized());
}

//=============================================================================
// Handle Validity Tests (Task 9)
//=============================================================================

TEST_F(ResourceAPITest, InvalidHandleReturnsInvalid)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    AxTextureHandle Invalid = AX_INVALID_HANDLE;
    EXPECT_FALSE(API->IsTextureValid(Invalid));
    EXPECT_FALSE(API->IsMeshValid(Invalid));
    EXPECT_FALSE(API->IsShaderValid(Invalid));
    EXPECT_FALSE(API->IsMaterialValid(Invalid));
}

TEST_F(ResourceAPITest, ZeroInitializedHandleIsInvalid)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    AxTextureHandle Handle;
    Handle.Index = 0;
    Handle.Generation = 0;
    EXPECT_FALSE(API->IsTextureValid(Handle));
}

TEST_F(ResourceAPITest, HandleWithIndexZeroIsInvalid)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    // Even with a valid-looking generation, index 0 is reserved as invalid
    AxTextureHandle Handle;
    Handle.Index = 0;
    Handle.Generation = 1;
    EXPECT_FALSE(API->IsTextureValid(Handle));
}

TEST_F(ResourceAPITest, HandleMacrosWork)
{
    AxTextureHandle A;
    A.Index = 1;
    A.Generation = 1;

    AxTextureHandle B;
    B.Index = 1;
    B.Generation = 1;

    AxTextureHandle C;
    C.Index = 2;
    C.Generation = 1;

    AxTextureHandle Invalid = AX_INVALID_HANDLE;

    EXPECT_TRUE(AX_HANDLE_EQUALS(A, B));
    EXPECT_FALSE(AX_HANDLE_EQUALS(A, C));
    EXPECT_TRUE(AX_HANDLE_IS_VALID(A));
    EXPECT_FALSE(AX_HANDLE_IS_VALID(Invalid));
}

//=============================================================================
// Reference Counting Tests (Task 10)
//=============================================================================

TEST_F(ResourceAPITest, AcquireInvalidHandleReturnsInvalid)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    AxTextureHandle Invalid = AX_INVALID_HANDLE;
    AxTextureHandle Result = API->AcquireTexture(Invalid);
    EXPECT_EQ(Result.Index, 0u);
    EXPECT_EQ(Result.Generation, 0u);
}

TEST_F(ResourceAPITest, ReleaseInvalidHandleSafe)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    // Should not crash
    AxTextureHandle Invalid = AX_INVALID_HANDLE;
    API->ReleaseTexture(Invalid);
    API->ReleaseMesh(Invalid);
    API->ReleaseShader(Invalid);
    API->ReleaseMaterial(Invalid);
}

TEST_F(ResourceAPITest, GetRefCountOfInvalidHandle)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    AxTextureHandle Invalid = AX_INVALID_HANDLE;
    EXPECT_EQ(API->GetTextureRefCount(Invalid), 0u);
    EXPECT_EQ(API->GetMeshRefCount(Invalid), 0u);
    EXPECT_EQ(API->GetShaderRefCount(Invalid), 0u);
    EXPECT_EQ(API->GetMaterialRefCount(Invalid), 0u);
}

//=============================================================================
// Deferred Destruction Tests (Task 11)
//=============================================================================

TEST_F(ResourceAPITest, InitialPendingReleaseCountIsZero)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    EXPECT_EQ(API->GetPendingReleaseCount(), 0u);
}

TEST_F(ResourceAPITest, ProcessPendingReleasesWhenEmpty)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    // Should not crash when there's nothing to process
    API->ProcessPendingReleases();
    EXPECT_EQ(API->GetPendingReleaseCount(), 0u);
}

TEST_F(ResourceAPITest, ProcessPendingReleasesLimitedWhenEmpty)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    // Should not crash when there's nothing to process
    API->ProcessPendingReleasesLimited(10);
    EXPECT_EQ(API->GetPendingReleaseCount(), 0u);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(ResourceAPITest, GetStatsWithNullPointerSafe)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    // Should not crash
    API->GetStats(nullptr);
}

TEST_F(ResourceAPITest, GetStatsInitialValues)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    struct AxResourceStats Stats;
    memset(&Stats, 0, sizeof(Stats));
    API->GetStats(&Stats);

    EXPECT_EQ(Stats.TexturesLoaded, 0u);
    EXPECT_EQ(Stats.MeshesLoaded, 0u);
    EXPECT_EQ(Stats.ShadersLoaded, 0u);
    EXPECT_EQ(Stats.MaterialsLoaded, 0u);
    EXPECT_EQ(Stats.PendingDeletions, 0u);
}

//=============================================================================
// Getter Tests (when handle is invalid)
//=============================================================================

TEST_F(ResourceAPITest, GetTextureWithInvalidHandleReturnsNull)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    AxTextureHandle Invalid = AX_INVALID_HANDLE;
    EXPECT_EQ(API->GetTexture(Invalid), nullptr);
}

TEST_F(ResourceAPITest, GetMeshWithInvalidHandleReturnsNull)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    AxMeshHandle Invalid = AX_INVALID_HANDLE;
    EXPECT_EQ(API->GetMesh(Invalid), nullptr);
}

TEST_F(ResourceAPITest, GetShaderWithInvalidHandleReturnsNull)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    AxShaderHandle Invalid = AX_INVALID_HANDLE;
    EXPECT_EQ(API->GetShader(Invalid), nullptr);
}

TEST_F(ResourceAPITest, GetShaderProgramWithInvalidHandle)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    AxShaderHandle Invalid = AX_INVALID_HANDLE;
    EXPECT_EQ(API->GetShaderProgram(Invalid), 0u);
}

TEST_F(ResourceAPITest, GetMaterialWithInvalidHandleReturnsNull)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    AxMaterialHandle Invalid = AX_INVALID_HANDLE;
    EXPECT_EQ(API->GetMaterial(Invalid), nullptr);
}

TEST_F(ResourceAPITest, GetInfoWithInvalidHandleReturnsFalse)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    struct AxTextureInfo TexInfo;
    struct AxMeshInfo MeshInfo;
    struct AxShaderInfo ShaderInfo;

    EXPECT_FALSE(API->GetTextureInfo(AX_INVALID_HANDLE, &TexInfo));
    EXPECT_FALSE(API->GetMeshInfo(AX_INVALID_HANDLE, &MeshInfo));
    EXPECT_FALSE(API->GetShaderInfo(AX_INVALID_HANDLE, &ShaderInfo));
}

TEST_F(ResourceAPITest, GetInfoWithNullOutputReturnsFalse)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    AxTextureHandle Handle;
    Handle.Index = 1;
    Handle.Generation = 1;

    EXPECT_FALSE(API->GetTextureInfo(Handle, nullptr));
    EXPECT_FALSE(API->GetMeshInfo(Handle, nullptr));
    EXPECT_FALSE(API->GetShaderInfo(Handle, nullptr));
}

//=============================================================================
// Load Function Tests (Phase 3 - fail gracefully without GPU/files)
//=============================================================================

TEST_F(ResourceAPITest, LoadTextureFailsWithoutFile)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    // Without real file, should return invalid handle (file not found)
    AxTextureHandle Handle = API->LoadTexture("nonexistent.png", nullptr);
    EXPECT_FALSE(AX_HANDLE_IS_VALID(Handle));
}

TEST_F(ResourceAPITest, LoadTextureFailsWithNullPath)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    // Null path should return invalid handle
    AxTextureHandle Handle = API->LoadTexture(nullptr, nullptr);
    EXPECT_FALSE(AX_HANDLE_IS_VALID(Handle));
}

TEST_F(ResourceAPITest, LoadTextureFailsWithEmptyPath)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    // Empty path should return invalid handle
    AxTextureHandle Handle = API->LoadTexture("", nullptr);
    EXPECT_FALSE(AX_HANDLE_IS_VALID(Handle));
}

TEST_F(ResourceAPITest, LoadMeshFailsWithoutFile)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    // Without real file, should return invalid handle (file not found)
    AxMeshHandle Handle = API->LoadMesh("nonexistent.gltf", nullptr);
    EXPECT_FALSE(AX_HANDLE_IS_VALID(Handle));
}

TEST_F(ResourceAPITest, LoadShaderFailsWithoutFiles)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    // Without real files or RenderAPI, should return invalid handle
    AxShaderHandle Handle = API->LoadShader("vert.glsl", "frag.glsl", nullptr);
    EXPECT_FALSE(AX_HANDLE_IS_VALID(Handle));
}

TEST_F(ResourceAPITest, CreateMaterialWithNullDescFails)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    // Null description should return invalid handle
    AxMaterialHandle Handle = API->CreateMaterial(nullptr);
    EXPECT_FALSE(AX_HANDLE_IS_VALID(Handle));
}

TEST_F(ResourceAPITest, CreateMaterialSucceeds)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    // Valid material description should succeed
    struct AxMaterialDesc Desc;
    memset(&Desc, 0, sizeof(Desc));
    strncpy(Desc.Name, "TestMaterial", sizeof(Desc.Name) - 1);
    Desc.Type = AX_MATERIAL_TYPE_PBR;
    Desc.PBR.BaseColorFactor.X = 1.0f;
    Desc.PBR.BaseColorFactor.Y = 1.0f;
    Desc.PBR.BaseColorFactor.Z = 1.0f;
    Desc.PBR.BaseColorFactor.W = 1.0f;
    Desc.PBR.MetallicFactor = 0.0f;
    Desc.PBR.RoughnessFactor = 0.5f;

    AxMaterialHandle Handle = API->CreateMaterial(&Desc);
    EXPECT_TRUE(AX_HANDLE_IS_VALID(Handle));

    // Verify material is valid
    EXPECT_TRUE(API->IsMaterialValid(Handle));

    // Verify we can get the material back
    const struct AxMaterialDesc* Retrieved = API->GetMaterial(Handle);
    EXPECT_NE(Retrieved, nullptr);
    if (Retrieved) {
        EXPECT_STREQ(Retrieved->Name, "TestMaterial");
        EXPECT_EQ(Retrieved->Type, AX_MATERIAL_TYPE_PBR);
        EXPECT_FLOAT_EQ(Retrieved->PBR.RoughnessFactor, 0.5f);
    }

    // Verify ref count is 1
    EXPECT_EQ(API->GetMaterialRefCount(Handle), 1u);
}

TEST_F(ResourceAPITest, MaterialAcquireReleaseCycle)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    // Create a material
    struct AxMaterialDesc Desc;
    memset(&Desc, 0, sizeof(Desc));
    strncpy(Desc.Name, "RefCountTest", sizeof(Desc.Name) - 1);
    Desc.Type = AX_MATERIAL_TYPE_PBR;

    AxMaterialHandle Handle = API->CreateMaterial(&Desc);
    EXPECT_TRUE(AX_HANDLE_IS_VALID(Handle));
    EXPECT_EQ(API->GetMaterialRefCount(Handle), 1u);

    // Acquire should increment refcount
    AxMaterialHandle Acquired = API->AcquireMaterial(Handle);
    EXPECT_TRUE(AX_HANDLE_EQUALS(Handle, Acquired));
    EXPECT_EQ(API->GetMaterialRefCount(Handle), 2u);

    // Release should decrement refcount
    API->ReleaseMaterial(Handle);
    EXPECT_EQ(API->GetMaterialRefCount(Handle), 1u);

    // Material should still be valid
    EXPECT_TRUE(API->IsMaterialValid(Handle));

    // Final release should queue for deletion
    API->ReleaseMaterial(Handle);
    EXPECT_EQ(API->GetPendingReleaseCount(), 1u);

    // Process pending releases
    API->ProcessPendingReleases();
    EXPECT_EQ(API->GetPendingReleaseCount(), 0u);

    // Handle should now be invalid
    EXPECT_FALSE(API->IsMaterialValid(Handle));
}

//=============================================================================
// Model Loading Tests (Phase 4)
//=============================================================================

TEST_F(ResourceAPITest, LoadModelWithNullPathFails)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    // Null path should return invalid handle
    AxModelHandle Handle = API->LoadModel(nullptr);
    EXPECT_FALSE(AX_HANDLE_IS_VALID(Handle));
}

TEST_F(ResourceAPITest, LoadModelWithNonexistentFileFails)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    // Non-existent file should return invalid handle
    AxModelHandle Handle = API->LoadModel("nonexistent.gltf");
    EXPECT_FALSE(AX_HANDLE_IS_VALID(Handle));
}

TEST_F(ResourceAPITest, LoadModelWithEmptyPathFails)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    // Empty path should return invalid handle
    AxModelHandle Handle = API->LoadModel("");
    EXPECT_FALSE(AX_HANDLE_IS_VALID(Handle));
}

TEST_F(ResourceAPITest, ReleaseModelWithInvalidHandleSafe)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    // Should not crash with invalid handle
    API->ReleaseModel(AX_INVALID_HANDLE);
}

TEST_F(ResourceAPITest, IsModelValidReturnsFalseForInvalidHandle)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    // Invalid handle should return false
    EXPECT_FALSE(API->IsModelValid(AX_INVALID_HANDLE));
}

TEST_F(ResourceAPITest, GetModelReturnsNullForInvalidHandle)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    // Invalid handle should return nullptr
    const struct AxModelData* Model = API->GetModel(AX_INVALID_HANDLE);
    EXPECT_EQ(Model, nullptr);
}

TEST_F(ResourceAPITest, GetModelCountReturnsZeroInitially)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    // No models loaded yet
    EXPECT_EQ(API->GetModelCount(), 0u);
}

TEST_F(ResourceAPITest, ModelDataStructureInitialization)
{
    // Test that AxModelData is properly structured
    struct AxModelData Model;
    memset(&Model, 0, sizeof(Model));

    // Verify structure has expected fields and sizes
    EXPECT_EQ(Model.MeshCount, 0u);
    EXPECT_EQ(Model.TextureCount, 0u);
    EXPECT_EQ(Model.MaterialCount, 0u);

    // Verify max limits are defined
    EXPECT_GT(AX_MODEL_MAX_MESHES, 0u);
    EXPECT_GT(AX_MODEL_MAX_TEXTURES, 0u);
    EXPECT_GT(AX_MODEL_MAX_MATERIALS, 0u);
}

//=============================================================================
// Statistics After Material Operations
//=============================================================================

TEST_F(ResourceAPITest, StatsUpdateAfterMaterialCreate)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    struct AxResourceStats StatsBefore;
    API->GetStats(&StatsBefore);
    EXPECT_EQ(StatsBefore.MaterialsLoaded, 0u);

    // Create a material
    struct AxMaterialDesc Desc;
    memset(&Desc, 0, sizeof(Desc));
    Desc.Type = AX_MATERIAL_TYPE_PBR;
    AxMaterialHandle Handle = API->CreateMaterial(&Desc);
    EXPECT_TRUE(AX_HANDLE_IS_VALID(Handle));

    struct AxResourceStats StatsAfter;
    API->GetStats(&StatsAfter);
    EXPECT_EQ(StatsAfter.MaterialsLoaded, 1u);
}

TEST_F(ResourceAPITest, StatsUpdateAfterMaterialRelease)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    // Create and release a material
    struct AxMaterialDesc Desc;
    memset(&Desc, 0, sizeof(Desc));
    Desc.Type = AX_MATERIAL_TYPE_PBR;
    AxMaterialHandle Handle = API->CreateMaterial(&Desc);

    struct AxResourceStats StatsBeforeRelease;
    API->GetStats(&StatsBeforeRelease);
    EXPECT_EQ(StatsBeforeRelease.MaterialsLoaded, 1u);
    EXPECT_EQ(StatsBeforeRelease.PendingDeletions, 0u);

    // Release - should queue for deletion
    API->ReleaseMaterial(Handle);

    struct AxResourceStats StatsAfterRelease;
    API->GetStats(&StatsAfterRelease);
    EXPECT_EQ(StatsAfterRelease.MaterialsLoaded, 1u); // Still loaded until processed
    EXPECT_EQ(StatsAfterRelease.PendingDeletions, 1u);

    // Process pending releases
    API->ProcessPendingReleases();

    struct AxResourceStats StatsAfterProcess;
    API->GetStats(&StatsAfterProcess);
    EXPECT_EQ(StatsAfterProcess.MaterialsLoaded, 0u);
    EXPECT_EQ(StatsAfterProcess.PendingDeletions, 0u);
}

//=============================================================================
// Statistics Accuracy Tests (Phase 4 - Task 17)
//=============================================================================

TEST_F(ResourceAPITest, StatsReflectMultipleMaterials)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    // Create multiple materials
    struct AxMaterialDesc Desc;
    memset(&Desc, 0, sizeof(Desc));
    Desc.Type = AX_MATERIAL_TYPE_PBR;

    AxMaterialHandle Handle1 = API->CreateMaterial(&Desc);
    AxMaterialHandle Handle2 = API->CreateMaterial(&Desc);
    AxMaterialHandle Handle3 = API->CreateMaterial(&Desc);

    EXPECT_TRUE(AX_HANDLE_IS_VALID(Handle1));
    EXPECT_TRUE(AX_HANDLE_IS_VALID(Handle2));
    EXPECT_TRUE(AX_HANDLE_IS_VALID(Handle3));

    struct AxResourceStats Stats;
    API->GetStats(&Stats);
    EXPECT_EQ(Stats.MaterialsLoaded, 3u);

    // Release one
    API->ReleaseMaterial(Handle2);
    API->ProcessPendingReleases();

    API->GetStats(&Stats);
    EXPECT_EQ(Stats.MaterialsLoaded, 2u);

    // Release remaining
    API->ReleaseMaterial(Handle1);
    API->ReleaseMaterial(Handle3);
    API->ProcessPendingReleases();

    API->GetStats(&Stats);
    EXPECT_EQ(Stats.MaterialsLoaded, 0u);
}

TEST_F(ResourceAPITest, RefCountAccuracyForMaterials)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    struct AxMaterialDesc Desc;
    memset(&Desc, 0, sizeof(Desc));
    Desc.Type = AX_MATERIAL_TYPE_PBR;

    AxMaterialHandle Handle = API->CreateMaterial(&Desc);
    EXPECT_EQ(API->GetMaterialRefCount(Handle), 1u);

    // Multiple acquires
    API->AcquireMaterial(Handle);
    EXPECT_EQ(API->GetMaterialRefCount(Handle), 2u);

    API->AcquireMaterial(Handle);
    EXPECT_EQ(API->GetMaterialRefCount(Handle), 3u);

    API->AcquireMaterial(Handle);
    EXPECT_EQ(API->GetMaterialRefCount(Handle), 4u);

    // Release back down
    API->ReleaseMaterial(Handle);
    EXPECT_EQ(API->GetMaterialRefCount(Handle), 3u);

    API->ReleaseMaterial(Handle);
    EXPECT_EQ(API->GetMaterialRefCount(Handle), 2u);

    API->ReleaseMaterial(Handle);
    EXPECT_EQ(API->GetMaterialRefCount(Handle), 1u);

    // Final release queues for deletion
    API->ReleaseMaterial(Handle);
    EXPECT_EQ(API->GetPendingReleaseCount(), 1u);
}

TEST_F(ResourceAPITest, ProcessPendingReleasesLimitedWorks)
{
    API->Initialize(AxonGlobalAPIRegistry, TestAllocator, nullptr);

    // Create multiple materials
    struct AxMaterialDesc Desc;
    memset(&Desc, 0, sizeof(Desc));
    Desc.Type = AX_MATERIAL_TYPE_PBR;

    AxMaterialHandle Handles[5];
    for (int i = 0; i < 5; i++) {
        Handles[i] = API->CreateMaterial(&Desc);
        EXPECT_TRUE(AX_HANDLE_IS_VALID(Handles[i]));
    }

    // Release all - they go to pending queue
    for (int i = 0; i < 5; i++) {
        API->ReleaseMaterial(Handles[i]);
    }

    EXPECT_EQ(API->GetPendingReleaseCount(), 5u);

    // Process only 2
    API->ProcessPendingReleasesLimited(2);
    EXPECT_EQ(API->GetPendingReleaseCount(), 3u);

    // Process only 2 more
    API->ProcessPendingReleasesLimited(2);
    EXPECT_EQ(API->GetPendingReleaseCount(), 1u);

    // Process remaining
    API->ProcessPendingReleasesLimited(10);
    EXPECT_EQ(API->GetPendingReleaseCount(), 0u);
}
