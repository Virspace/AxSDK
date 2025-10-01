#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxAllocUtils.h"
#include "Foundation/AxAllocatorRegistry.h"
#include "Foundation/AxAllocationData.h"
#include "Foundation/AxAllocatorInfo.h"
#include "Foundation/AxLinearAllocator.h"

static uintptr_t AlignedAddress1 = 0x000001bb20350000;
static uintptr_t AlignedAddress2 = 0x000001bb20350008;
static uintptr_t UnalignedAddress = 0x000001bb20350001;
static uintptr_t NextPage = 0x000001bb20351000;

TEST(AllocUtils, RoundAddressUp)
{
    EXPECT_EQ(RoundAddressUpToPowerOfTwo(UnalignedAddress, 4096), NextPage);
}

TEST(AllocUtils, RoundAddressDown)
{
    EXPECT_EQ(RoundAddressDownToPowerOfTwo(UnalignedAddress, 4096), AlignedAddress1);
}

TEST(AllocUtils, AddressDistance)
{
    size_t Size = 7;
    uintptr_t End = UnalignedAddress + Size;
    EXPECT_EQ(AddressDistance(UnalignedAddress, End), 7);
}

TEST(AllocUtils, IsStraddlingBoundary)
{
    EXPECT_EQ(IsStraddlingBoundary(AlignedAddress1, 7, 4096), true);
}

TEST(AllocUtils, IsAligned)
{
    uintptr_t Start = 0xFF1040; // true
    uintptr_t End = 0xFF1048; // true

    EXPECT_EQ(IsAligned(Start), true);

    // Test every address in-between
    for (uintptr_t Current = Start + 1; Current < End; Current++) {
        EXPECT_EQ(IsAligned(Current), false);
    }

    EXPECT_EQ(IsAligned(End), true);
}

TEST(AllocUtils, AlignOffset)
{
    EXPECT_EQ(AlignOffset(AlignedAddress1), 0);
    EXPECT_EQ(AlignOffset(UnalignedAddress), 7);
}

TEST(AllocUtils, AlignAddress)
{
    EXPECT_EQ(AlignAddress(UnalignedAddress), AlignedAddress2);
}

class LinearAllocatorTest : public testing::Test
{
    protected:
        AxLinearAllocator *LinearAllocator;

        void SetUp()
        {
            LinearAllocator = LinearAllocatorAPI->Create("LinearTest", Megabytes(4));
        }

        void TearDown()
        {
            LinearAllocatorAPI->Destroy(LinearAllocator);
        }
};

TEST_F(LinearAllocatorTest, Length)
{
    size_t Length = AllocatorRegistryAPI->Length();
    EXPECT_EQ(Length, 1);
}

TEST(LinearAllocatorBasicTest, AllocatorCreationAndDestruction)
{
    // Basic test without fixture to isolate the issue
    AxLinearAllocator *TestAllocator = LinearAllocatorAPI->Create("BasicTest", Megabytes(1));
    EXPECT_NE(TestAllocator, nullptr) << "Failed to create allocator";

    if (TestAllocator) {
        size_t Length = AllocatorRegistryAPI->Length();
        EXPECT_GE(Length, 1) << "Registry should contain at least our allocator";

        auto *Data = AllocatorRegistryAPI->GetAllocatorDataByName("BasicTest");
        EXPECT_NE(Data, nullptr) << "Should find our allocator by name";

        LinearAllocatorAPI->Destroy(TestAllocator);

        // After destroy, the allocator should be gone
        Data = AllocatorRegistryAPI->GetAllocatorDataByName("BasicTest");
        EXPECT_EQ(Data, nullptr) << "Allocator should be unregistered after destroy";
    }
}

TEST_F(LinearAllocatorTest, AllocatorRegistryCleanup)
{
    // Ensure allocator registry properly unregisters destroyed allocators

    // First verify our fixture allocator exists
    auto *LinearData = AllocatorRegistryAPI->GetAllocatorDataByName("LinearTest");
    EXPECT_NE(LinearData, nullptr) << "Fixture allocator 'LinearTest' should exist";

    size_t InitialLength = AllocatorRegistryAPI->Length();
    EXPECT_GE(InitialLength, 1) << "Registry should contain at least the fixture allocator";

    // Create a temporary allocator with a unique name
    AxLinearAllocator *TempAllocator = LinearAllocatorAPI->Create("CleanupTest", Megabytes(1));
    EXPECT_NE(TempAllocator, nullptr) << "Failed to create temporary allocator";

    if (TempAllocator == nullptr) {
        return; // Skip rest of test if allocator creation failed
    }

    // Registry should have one more allocator
    size_t LengthAfterCreate = AllocatorRegistryAPI->Length();
    EXPECT_EQ(LengthAfterCreate, InitialLength + 1);

    // Verify both allocators exist
    LinearData = AllocatorRegistryAPI->GetAllocatorDataByName("LinearTest");
    auto *TempData = AllocatorRegistryAPI->GetAllocatorDataByName("CleanupTest");
    EXPECT_NE(LinearData, nullptr);
    EXPECT_NE(TempData, nullptr);

    // Destroy the temporary allocator
    LinearAllocatorAPI->Destroy(TempAllocator);

    // Registry should be back to initial count
    size_t LengthAfterDestroy = AllocatorRegistryAPI->Length();
    EXPECT_EQ(LengthAfterDestroy, InitialLength);

    // Verify LinearTest still exists and TempTest is gone
    LinearData = AllocatorRegistryAPI->GetAllocatorDataByName("LinearTest");
    TempData = AllocatorRegistryAPI->GetAllocatorDataByName("CleanupTest");
    EXPECT_NE(LinearData, nullptr);
    EXPECT_EQ(TempData, nullptr);
}

TEST_F(LinearAllocatorTest, Info)
{
    AxAllocatorData *Data = AllocatorRegistryAPI->GetAllocatorDataByName("LinearTest");

    // Quick check on basic values
    EXPECT_EQ(AllocatorDataAPI->PageSize(Data), Kilobytes(4));
    EXPECT_EQ(AllocatorDataAPI->AllocationGranularity(Data), Kilobytes(64));

    // Check creation of base
    EXPECT_EQ(AllocatorDataAPI->BytesReserved(Data), Megabytes(4));
    EXPECT_EQ(AllocatorDataAPI->BytesCommitted(Data), 0);
    EXPECT_EQ(AllocatorDataAPI->PagesReserved(Data), 1024);
    EXPECT_EQ(AllocatorDataAPI->PagesCommitted(Data), 0);

    // Check allocation
    void *Mem1 = LinearAllocatorAPI->Alloc(LinearAllocator, Megabytes(1), __FILE__, __LINE__);
    EXPECT_EQ(AllocatorDataAPI->BytesReserved(Data), Megabytes(4));
    EXPECT_EQ(AllocatorDataAPI->BytesCommitted(Data), Megabytes(1));
    EXPECT_EQ(AllocatorDataAPI->PagesReserved(Data), 1024);
    EXPECT_EQ(AllocatorDataAPI->PagesCommitted(Data), 256);
    EXPECT_TRUE(IsAligned((uintptr_t)Mem1));

    // Check allocation
    void *Mem2 = LinearAllocatorAPI->Alloc(LinearAllocator, Megabytes(2), __FILE__, __LINE__);
    EXPECT_EQ(AllocatorDataAPI->BytesReserved(Data), Megabytes(4));
    EXPECT_EQ(AllocatorDataAPI->BytesCommitted(Data), Megabytes(3));
    EXPECT_EQ(AllocatorDataAPI->PagesReserved(Data), 1024);
    EXPECT_EQ(AllocatorDataAPI->PagesCommitted(Data), 768);
    EXPECT_TRUE(IsAligned((uintptr_t)Mem2));

    // Check allocation
    void *Mem3 = LinearAllocatorAPI->Alloc(LinearAllocator, Kilobytes(64), __FILE__, __LINE__);
    EXPECT_EQ(AllocatorDataAPI->BytesReserved(Data), Megabytes(4));
    EXPECT_EQ(AllocatorDataAPI->BytesCommitted(Data), Megabytes(3) + Kilobytes(64));
    EXPECT_EQ(AllocatorDataAPI->PagesReserved(Data), 1024);
    EXPECT_EQ(AllocatorDataAPI->PagesCommitted(Data), 784);
    EXPECT_TRUE(IsAligned((uintptr_t)Mem3));

    // Check allocation data
    for (size_t i = 0; i < AllocatorDataAPI->NumAllocs(Data); ++i) {
        AxAllocationData AllocData;
        if (AllocatorDataAPI->GetAllocationDataByIndex(Data, i, &AllocData)) {
            switch (i) {
                case 0:
                    EXPECT_EQ(AllocData.Size, Megabytes(1)); break;
                case 1:
                    EXPECT_EQ(AllocData.Size, Megabytes(2)); break;
                case 2:
                    EXPECT_EQ(AllocData.Size, Kilobytes(64)); break;
            }
        }
    }
}

// Additional comprehensive tests for linear allocator functionality
TEST_F(LinearAllocatorTest, SmallAllocationAlignment)
{
    // Test that small allocations (1-7 bytes) are properly aligned
    // This test revealed critical alignment bugs in the implementation
    for (size_t size = 1; size <= 7; ++size) {
        void *Mem = LinearAllocatorAPI->Alloc(LinearAllocator, size, __FILE__, __LINE__);
        EXPECT_NE(Mem, nullptr);
        EXPECT_TRUE(IsAligned((uintptr_t)Mem));
    }
}

TEST_F(LinearAllocatorTest, MemoryExhaustionHandling)
{
    // Test behavior when approaching memory limits
    std::vector<void*> Allocations;

    // Allocate in 1MB chunks until we approach the limit (4MB total)
    for (int i = 0; i < 3; ++i) { // Should succeed (3MB < 4MB limit)
        void *Mem = LinearAllocatorAPI->Alloc(LinearAllocator, Megabytes(1), __FILE__, __LINE__);
        EXPECT_NE(Mem, nullptr);
        Allocations.push_back(Mem);
    }

    // This allocation should fail (would exceed 4MB limit)
    void *OverflowMem = LinearAllocatorAPI->Alloc(LinearAllocator, Megabytes(2), __FILE__, __LINE__);
    EXPECT_EQ(OverflowMem, nullptr);

    // Smaller allocation should still work in remaining space
    void *SmallMem = LinearAllocatorAPI->Alloc(LinearAllocator, Kilobytes(512), __FILE__, __LINE__);
    EXPECT_NE(SmallMem, nullptr);
}

TEST_F(LinearAllocatorTest, ZeroSizeAllocation)
{
    // Test edge case: zero-size allocation
    void *ZeroMem = LinearAllocatorAPI->Alloc(LinearAllocator, 0, __FILE__, __LINE__);

    // Should return valid pointer or handle gracefully
    // Implementation may choose to return NULL or a minimal allocation
    if (ZeroMem != nullptr) {
        EXPECT_TRUE(IsAligned((uintptr_t)ZeroMem));
    }
}

TEST_F(LinearAllocatorTest, PageBoundaryAllocation)
{
    // Allocate memory that crosses page boundaries
    AxAllocatorData *Data = AllocatorRegistryAPI->GetAllocatorDataByName("LinearTest");
    size_t PageSize = AllocatorDataAPI->PageSize(Data);

    // First allocate most of a page
    void *Mem1 = LinearAllocatorAPI->Alloc(LinearAllocator, PageSize - 64, __FILE__, __LINE__);
    EXPECT_NE(Mem1, nullptr);

    // Then allocate something that will cross the boundary
    void *Mem2 = LinearAllocatorAPI->Alloc(LinearAllocator, 128, __FILE__, __LINE__);
    EXPECT_NE(Mem2, nullptr);
    EXPECT_TRUE(IsAligned((uintptr_t)Mem2));

    // Verify committed memory increased appropriately
    EXPECT_GT(AllocatorDataAPI->BytesCommitted(Data), PageSize);
}

TEST_F(LinearAllocatorTest, ResetAndReuse)
{
    // Allocate some memory
    void *Mem1 = LinearAllocatorAPI->Alloc(LinearAllocator, Kilobytes(64), __FILE__, __LINE__);
    void *Mem2 = LinearAllocatorAPI->Alloc(LinearAllocator, Kilobytes(128), __FILE__, __LINE__);

    EXPECT_NE(Mem1, nullptr);
    EXPECT_NE(Mem2, nullptr);

    AxAllocatorData *Data = AllocatorRegistryAPI->GetAllocatorDataByName("LinearTest");
    size_t BytesAllocatedBefore = AllocatorDataAPI->NumAllocs(Data);
    EXPECT_GT(BytesAllocatedBefore, 0);

    // Free all allocations (linear allocator frees everything at once)
    LinearAllocatorAPI->Free(LinearAllocator, __FILE__, __LINE__);

    // Verify allocator has been reset
    EXPECT_EQ(AllocatorDataAPI->NumAllocs(Data), 0);

    // Should be able to allocate again
    void *NewMem = LinearAllocatorAPI->Alloc(LinearAllocator, Kilobytes(32), __FILE__, __LINE__);
    EXPECT_NE(NewMem, nullptr);
    EXPECT_TRUE(IsAligned((uintptr_t)NewMem));
}

TEST_F(LinearAllocatorTest, AlignmentConsistency)
{
    // Test that all allocations maintain consistent alignment
    std::vector<void*> Allocations;

    // Mix of odd and even sizes to test alignment
    size_t TestSizes[] = {1, 3, 7, 15, 31, 63, 127, 255};

    for (size_t size : TestSizes) {
        void *Mem = LinearAllocatorAPI->Alloc(LinearAllocator, size, __FILE__, __LINE__);
        if (Mem) {
            EXPECT_TRUE(IsAligned((uintptr_t)Mem));
            Allocations.push_back(Mem);
        }
    }

    // Verify no overlapping allocations
    for (size_t i = 1; i < Allocations.size(); ++i) {
        EXPECT_LT((uintptr_t)Allocations[i-1], (uintptr_t)Allocations[i]);
    }
}

// Edge case tests that don't require the LinearAllocatorTest fixture
class LinearAllocatorEdgeCaseTest : public testing::Test
{
};

TEST_F(LinearAllocatorEdgeCaseTest, CreateWithZeroSize)
{
    // Test creating allocator with zero size
    AxLinearAllocator *ZeroAllocator = LinearAllocatorAPI->Create("ZeroTest", 0);
    EXPECT_EQ(ZeroAllocator, nullptr); // Should fail gracefully
}

TEST_F(LinearAllocatorEdgeCaseTest, NullAllocatorHandling)
{
    // Test that functions handle NULL allocator gracefully
    void *Mem = LinearAllocatorAPI->Alloc(nullptr, 64, __FILE__, __LINE__);
    EXPECT_EQ(Mem, nullptr);

    // Free should not crash with NULL allocator
    LinearAllocatorAPI->Free(nullptr, __FILE__, __LINE__);
}

// ============================================================================
// Vigorous Testing Suite for Linear Allocator
// ============================================================================

TEST_F(LinearAllocatorTest, DataCorruptionDetection)
{
    // Test for data corruption by writing patterns and verifying they persist
    struct TestPattern {
        void* ptr;
        size_t size;
        uint8_t pattern;
    };

    std::vector<TestPattern> allocations;

    // Allocate several chunks with different patterns
    for (int i = 0; i < 10; ++i) {
        size_t size = 64 + (i * 32); // Varying sizes: 64, 96, 128, etc.
        void* ptr = LinearAllocatorAPI->Alloc(LinearAllocator, size, __FILE__, __LINE__);
        ASSERT_NE(ptr, nullptr);

        uint8_t pattern = 0xAA + i; // Different pattern for each allocation
        memset(ptr, pattern, size);

        allocations.push_back({ptr, size, pattern});
    }

    // Verify all patterns are still intact (no corruption)
    for (const auto& alloc : allocations) {
        uint8_t* bytes = (uint8_t*)alloc.ptr;
        for (size_t j = 0; j < alloc.size; ++j) {
            EXPECT_EQ(bytes[j], alloc.pattern)
                << "Data corruption detected at allocation " << &alloc - &allocations[0]
                << ", byte " << j;
        }
    }
}

TEST_F(LinearAllocatorTest, ExtensiveSequentialAllocation)
{
    // Test many sequential allocations of various sizes
    std::vector<void*> allocations;
    size_t totalAllocated = 0;

    // Pattern: 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, then repeat
    for (int round = 0; round < 3; ++round) {
        for (size_t size = 1; size <= 1024; size *= 2) {
            if (totalAllocated + size >= Megabytes(3)) break; // Stay under limit

            void* ptr = LinearAllocatorAPI->Alloc(LinearAllocator, size, __FILE__, __LINE__);
            ASSERT_NE(ptr, nullptr) << "Failed to allocate " << size << " bytes in round " << round;
            EXPECT_TRUE(IsAligned((uintptr_t)ptr));

            // Write a recognizable pattern
            memset(ptr, 0xCC, size);
            allocations.push_back(ptr);
            totalAllocated += size;
        }
    }

    // Verify all allocations are still accessible and contain expected data
    for (void* ptr : allocations) {
        uint8_t* bytes = (uint8_t*)ptr;
        EXPECT_EQ(bytes[0], 0xCC) << "First byte corrupted in allocation " << ptr;
    }
}

TEST_F(LinearAllocatorTest, StressTestManySmallAllocations)
{
    // Stress test with many small allocations
    const int numAllocations = 1000;
    std::vector<void*> allocations;
    allocations.reserve(numAllocations);

    // Allocate many small chunks
    for (int i = 0; i < numAllocations; ++i) {
        size_t size = 1 + (i % 63); // Sizes 1-64 bytes
        void* ptr = LinearAllocatorAPI->Alloc(LinearAllocator, size, __FILE__, __LINE__);

        if (!ptr) {
            // Memory exhausted - this is expected behavior
            break;
        }

        EXPECT_TRUE(IsAligned((uintptr_t)ptr));

        // Write unique pattern to each allocation
        uint8_t pattern = (uint8_t)(i & 0xFF);
        memset(ptr, pattern, size);
        allocations.push_back(ptr);
    }

    EXPECT_GT(allocations.size(), 100) << "Should be able to allocate at least 100 small chunks";

    // Verify no corruption occurred
    for (size_t i = 0; i < allocations.size(); ++i) {
        uint8_t* bytes = (uint8_t*)allocations[i];
        uint8_t expectedPattern = (uint8_t)(i & 0xFF);
        EXPECT_EQ(bytes[0], expectedPattern) << "Corruption in allocation " << i;
    }
}

TEST_F(LinearAllocatorTest, LargeAllocationIntegrity)
{
    // Test large allocations and verify data integrity
    const size_t largeSize = Megabytes(1);
    void* largePtr = LinearAllocatorAPI->Alloc(LinearAllocator, largeSize, __FILE__, __LINE__);
    ASSERT_NE(largePtr, nullptr);

    // Fill with pattern
    uint32_t* words = (uint32_t*)largePtr;
    size_t numWords = largeSize / sizeof(uint32_t);

    for (size_t i = 0; i < numWords; ++i) {
        words[i] = (uint32_t)(0xDEADBEEF + i);
    }

    // Verify pattern
    for (size_t i = 0; i < numWords; ++i) {
        EXPECT_EQ(words[i], (uint32_t)(0xDEADBEEF + i))
            << "Large allocation corruption at word " << i;
    }

    // Allocate something small after the large allocation
    void* smallPtr = LinearAllocatorAPI->Alloc(LinearAllocator, 64, __FILE__, __LINE__);
    ASSERT_NE(smallPtr, nullptr);
    memset(smallPtr, 0xFF, 64);

    // Verify large allocation wasn't affected
    for (size_t i = 0; i < 10; ++i) { // Check first 10 words
        EXPECT_EQ(words[i], (uint32_t)(0xDEADBEEF + i))
            << "Large allocation corrupted after subsequent allocation";
    }
}

TEST_F(LinearAllocatorTest, AllocationAddressMonotonicity)
{
    // Verify that allocation addresses are always increasing
    std::vector<void*> allocations;

    for (int i = 0; i < 50; ++i) {
        void* ptr = LinearAllocatorAPI->Alloc(LinearAllocator, 32, __FILE__, __LINE__);
        if (!ptr) break;

        allocations.push_back(ptr);

        // Each allocation should be at a higher address than the previous
        if (allocations.size() > 1) {
            EXPECT_LT((uintptr_t)allocations[allocations.size()-2],
                     (uintptr_t)allocations[allocations.size()-1])
                << "Allocation " << i << " not at higher address than previous";
        }
    }
}

TEST_F(LinearAllocatorTest, ResetPreservesCapacity)
{
    // Verify that reset doesn't affect allocator capacity
    AxAllocatorData *Data = AllocatorRegistryAPI->GetAllocatorDataByName("LinearTest");
    size_t originalReserved = AllocatorDataAPI->BytesReserved(Data);
    size_t originalPages = AllocatorDataAPI->PagesReserved(Data);

    // Make some allocations
    void* ptr1 = LinearAllocatorAPI->Alloc(LinearAllocator, Megabytes(1), __FILE__, __LINE__);
    void* ptr2 = LinearAllocatorAPI->Alloc(LinearAllocator, Megabytes(1), __FILE__, __LINE__);
    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);

    // Reset
    LinearAllocatorAPI->Free(LinearAllocator, __FILE__, __LINE__);

    // Capacity should be unchanged
    EXPECT_EQ(AllocatorDataAPI->BytesReserved(Data), originalReserved);
    EXPECT_EQ(AllocatorDataAPI->PagesReserved(Data), originalPages);

    // Should be able to allocate the same amount again
    void* newPtr1 = LinearAllocatorAPI->Alloc(LinearAllocator, Megabytes(1), __FILE__, __LINE__);
    void* newPtr2 = LinearAllocatorAPI->Alloc(LinearAllocator, Megabytes(1), __FILE__, __LINE__);
    EXPECT_NE(newPtr1, nullptr);
    EXPECT_NE(newPtr2, nullptr);
}

TEST_F(LinearAllocatorTest, MaximumAllocationSize)
{
    // Test allocation at the maximum possible size
    AxAllocatorData *Data = AllocatorRegistryAPI->GetAllocatorDataByName("LinearTest");
    size_t maxSize = AllocatorDataAPI->BytesReserved(Data);

    // Should be able to allocate the full advertised capacity
    void* maxPtr = LinearAllocatorAPI->Alloc(LinearAllocator, maxSize, __FILE__, __LINE__);
    EXPECT_NE(maxPtr, nullptr) << "Should be able to allocate full maximum size";

    if (maxPtr) {
        // Verify we can write to the entire allocation
        memset(maxPtr, 0x42, maxSize);

        // Verify first and last bytes
        uint8_t* bytes = (uint8_t*)maxPtr;
        EXPECT_EQ(bytes[0], 0x42);
        EXPECT_EQ(bytes[maxSize - 1], 0x42);
    }
}