#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxAllocUtils.h"
#include "Foundation/AxAllocator.h"
#include "Foundation/AxAllocatorAPI.h"

#include <vector>

// Scene Loading Pattern Tests for Linear Allocator
// These tests specifically validate allocator behavior for scene loading use cases
class SceneLoadingAllocatorTest : public testing::Test
{
    protected:
        struct AxAllocator *SceneAllocator;

        void SetUp()
        {
            // Create allocator sized for typical scene data (8MB)
            SceneAllocator = AllocatorAPI->CreateLinear("SceneTest", Megabytes(8));
        }

        void TearDown()
        {
            SceneAllocator->Destroy(SceneAllocator);
        }
};

TEST_F(SceneLoadingAllocatorTest, HierarchicalObjectPattern)
{
    // Simulate typical scene object allocation pattern
    // This test validates allocator performance for scene graph structures

    // Root object (simulating AxSceneObject structure)
    void *RootObject = AxAlloc(SceneAllocator, 64);
    EXPECT_NE(RootObject, nullptr);
    EXPECT_TRUE(IsAligned((uintptr_t)RootObject));

    // Child objects with varying sizes to test real-world alignment scenarios
    void *Child1 = AxAlloc(SceneAllocator, 64);
    void *Child2 = AxAlloc(SceneAllocator, 128);
    void *Child3 = AxAlloc(SceneAllocator, 256);

    EXPECT_NE(Child1, nullptr);
    EXPECT_NE(Child2, nullptr);
    EXPECT_NE(Child3, nullptr);
    EXPECT_TRUE(IsAligned((uintptr_t)Child1));
    EXPECT_TRUE(IsAligned((uintptr_t)Child2));
    EXPECT_TRUE(IsAligned((uintptr_t)Child3));

    // Verify objects are allocated sequentially (important for cache locality)
    EXPECT_LT((uintptr_t)RootObject, (uintptr_t)Child1);
    EXPECT_LT((uintptr_t)Child1, (uintptr_t)Child2);
    EXPECT_LT((uintptr_t)Child2, (uintptr_t)Child3);
}

TEST_F(SceneLoadingAllocatorTest, AssetPathStringPattern)
{
    // Scene files contain many string allocations for asset paths and object names
    // This test validates allocator performance for typical string allocation patterns
    std::vector<void*> StringAllocs;

    // Allocate various string sizes typical for scene data:
    // - Object names: 16-64 bytes
    // - Asset paths: 128-256 bytes
    size_t StringSizes[] = {16, 32, 64, 128, 256};

    // Simulate loading a scene with 50 named objects and asset references
    for (size_t i = 0; i < 50; ++i) {
        size_t StringSize = StringSizes[i % 5];
        void *StringData = AxAlloc(SceneAllocator, StringSize);
        EXPECT_NE(StringData, nullptr);
        EXPECT_TRUE(IsAligned((uintptr_t)StringData));
        StringAllocs.push_back(StringData);
    }

    // Verify all allocations are sequential and properly aligned
    // This is critical for scene loading performance
    for (size_t i = 1; i < StringAllocs.size(); ++i) {
        EXPECT_LT((uintptr_t)StringAllocs[i-1], (uintptr_t)StringAllocs[i]);
    }
}

TEST_F(SceneLoadingAllocatorTest, CompleteSceneLoadingWorkflow)
{
    // Simulate a complete scene loading workflow with mixed allocation patterns
    // This represents realistic scene loading: objects, transforms, strings, and metadata

    // 1. Allocate scene header (scene name, metadata)
    void *SceneHeader = AxAlloc(SceneAllocator, 128);
    EXPECT_NE(SceneHeader, nullptr);

    // 2. Allocate scene objects (varying sizes for different object types)
    std::vector<void*> SceneObjects;
    for (int i = 0; i < 20; ++i) {
        // Mix of small and large objects (32-512 bytes)
        size_t ObjectSize = (i % 3 == 0) ? 512 : ((i % 2 == 0) ? 128 : 64);
        void *Object = AxAlloc(SceneAllocator, ObjectSize);
        EXPECT_NE(Object, nullptr);
        EXPECT_TRUE(IsAligned((uintptr_t)Object));
        SceneObjects.push_back(Object);
    }

    // 3. Allocate asset path strings (typical scene has many asset references)
    std::vector<void*> AssetPaths;
    for (int i = 0; i < 30; ++i) {
        // Asset paths: mesh files, texture files, material files
        size_t PathSize = 256; // Typical max path length
        void *Path = AxAlloc(SceneAllocator, PathSize);
        EXPECT_NE(Path, nullptr);
        EXPECT_TRUE(IsAligned((uintptr_t)Path));
        AssetPaths.push_back(Path);
    }

    // 4. Allocate transform data (matrices, vectors)
    std::vector<void*> Transforms;
    for (int i = 0; i < 25; ++i) {
        // Transform structures (vectors, matrices, quaternions)
        size_t TransformSize = (i % 2 == 0) ? 64 : 128; // Mix of different transform sizes
        void *Transform = AxAlloc(SceneAllocator, TransformSize);
        EXPECT_NE(Transform, nullptr);
        EXPECT_TRUE(IsAligned((uintptr_t)Transform));
        Transforms.push_back(Transform);
    }

    // Verify all allocations maintain proper sequence and alignment
    // This ensures scene data has good memory locality for rendering
    uintptr_t LastAddress = (uintptr_t)SceneHeader;

    for (void* obj : SceneObjects) {
        EXPECT_LT(LastAddress, (uintptr_t)obj);
        LastAddress = (uintptr_t)obj;
    }

    for (void* path : AssetPaths) {
        EXPECT_LT(LastAddress, (uintptr_t)path);
        LastAddress = (uintptr_t)path;
    }

    for (void* transform : Transforms) {
        EXPECT_LT(LastAddress, (uintptr_t)transform);
        LastAddress = (uintptr_t)transform;
    }

    // Verify we can reset and reuse the allocator for another scene
    SceneAllocator->Reset(SceneAllocator);

    // Should be able to load another scene
    void *NewSceneHeader = AxAlloc(SceneAllocator, 128);
    EXPECT_NE(NewSceneHeader, nullptr);
    EXPECT_TRUE(IsAligned((uintptr_t)NewSceneHeader));
}

TEST_F(SceneLoadingAllocatorTest, LargeSceneCapacity)
{
    // Test allocator capacity for large scenes (stress test for scene loading)
    // This validates the 8MB allocator can handle substantial scene data

    std::vector<void*> Allocations;
    size_t TotalAllocated = 0;
    const size_t ChunkSize = Kilobytes(64); // Reasonable scene object size

    // Allocate until we've used most of the available space
    while (TotalAllocated < Megabytes(7)) { // Leave 1MB headroom
        void *Chunk = AxAlloc(SceneAllocator, ChunkSize);
        if (!Chunk) {
            break; // Out of space
        }

        EXPECT_TRUE(IsAligned((uintptr_t)Chunk));
        Allocations.push_back(Chunk);
        TotalAllocated += ChunkSize;
    }

    // Should have allocated a substantial amount (at least 6MB)
    EXPECT_GT(TotalAllocated, Megabytes(6));

    // Verify we can still allocate small objects (typical for scene metadata)
    void *SmallObj = AxAlloc(SceneAllocator, 32);
    EXPECT_NE(SmallObj, nullptr);
    EXPECT_TRUE(IsAligned((uintptr_t)SmallObj));
}
