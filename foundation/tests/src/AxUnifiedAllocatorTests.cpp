/**
 * AxUnifiedAllocatorTests.cpp - Tests for the unified AxAllocator interface
 *
 * These tests verify the new unified allocator system that provides a common
 * interface for Heap, Linear, and Stack allocators through AxAllocatorAPI.
 */

#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxAllocator.h"
#include "Foundation/AxAllocatorAPI.h"
#include <cstring>
#include <vector>

//=============================================================================
// Heap Allocator Tests (Unified Interface)
//=============================================================================

class UnifiedHeapAllocatorTest : public testing::Test
{
protected:
    struct AxAllocator* Heap;

    void SetUp() override
    {
        Heap = AllocatorAPI->CreateHeap("TestHeap", Kilobytes(64), Megabytes(1));
        ASSERT_NE(Heap, nullptr) << "Failed to create heap allocator";
    }

    void TearDown() override
    {
        if (Heap) {
            Heap->Destroy(Heap);
            Heap = nullptr;
        }
    }
};

TEST_F(UnifiedHeapAllocatorTest, CreateAndDestroy)
{
    // Heap was created in SetUp, verify it exists
    EXPECT_NE(Heap, nullptr);
    EXPECT_STREQ(Heap->Name, "TestHeap");
    EXPECT_EQ(Heap->BytesAllocated, 0);
    EXPECT_EQ(Heap->AllocationCount, 0);
    // Destroy will be called in TearDown
}

TEST_F(UnifiedHeapAllocatorTest, BasicAllocation)
{
    void* ptr = AxAlloc(Heap, 256);
    ASSERT_NE(ptr, nullptr);

    // Write to memory to ensure it's usable
    memset(ptr, 0xAA, 256);

    AxFree(Heap, ptr);
}

TEST_F(UnifiedHeapAllocatorTest, AllocationAlignment)
{
    // Test 16-byte alignment (default)
    void* ptr16 = AxAllocAligned(Heap, 100, 16);
    ASSERT_NE(ptr16, nullptr);
    EXPECT_EQ((uintptr_t)ptr16 % 16, 0) << "Allocation not 16-byte aligned";

    // Test 32-byte alignment
    void* ptr32 = AxAllocAligned(Heap, 100, 32);
    ASSERT_NE(ptr32, nullptr);
    EXPECT_EQ((uintptr_t)ptr32 % 32, 0) << "Allocation not 32-byte aligned";

    // Test 64-byte alignment
    void* ptr64 = AxAllocAligned(Heap, 100, 64);
    ASSERT_NE(ptr64, nullptr);
    EXPECT_EQ((uintptr_t)ptr64 % 64, 0) << "Allocation not 64-byte aligned";

    AxFree(Heap, ptr16);
    AxFree(Heap, ptr32);
    AxFree(Heap, ptr64);
}

TEST_F(UnifiedHeapAllocatorTest, Reallocation)
{
    // Allocate and fill with pattern
    void* ptr = AxAlloc(Heap, 64);
    ASSERT_NE(ptr, nullptr);
    memset(ptr, 0xBB, 64);

    // Reallocate to larger size
    void* newPtr = AxRealloc(Heap, ptr, 64, 128);
    ASSERT_NE(newPtr, nullptr);

    // Verify original data preserved
    uint8_t* bytes = (uint8_t*)newPtr;
    for (int i = 0; i < 64; i++) {
        EXPECT_EQ(bytes[i], 0xBB) << "Data corrupted at byte " << i;
    }

    AxFree(Heap, newPtr);
}

TEST_F(UnifiedHeapAllocatorTest, ReallocationShrink)
{
    // Allocate and fill with pattern
    void* ptr = AxAlloc(Heap, 256);
    ASSERT_NE(ptr, nullptr);
    memset(ptr, 0xCC, 256);

    // Reallocate to smaller size
    void* newPtr = AxRealloc(Heap, ptr, 256, 64);
    ASSERT_NE(newPtr, nullptr);

    // Verify data in new (smaller) range preserved
    uint8_t* bytes = (uint8_t*)newPtr;
    for (int i = 0; i < 64; i++) {
        EXPECT_EQ(bytes[i], 0xCC) << "Data corrupted at byte " << i;
    }

    AxFree(Heap, newPtr);
}

TEST_F(UnifiedHeapAllocatorTest, MultipleAllocations)
{
    std::vector<void*> allocations;

    // Allocate many blocks
    for (int i = 0; i < 100; i++) {
        void* ptr = AxAlloc(Heap, 64 + i);
        ASSERT_NE(ptr, nullptr) << "Allocation " << i << " failed";
        memset(ptr, (uint8_t)i, 64 + i);
        allocations.push_back(ptr);
    }

    // Free in random order (every other one first, then rest)
    for (size_t i = 0; i < allocations.size(); i += 2) {
        AxFree(Heap, allocations[i]);
        allocations[i] = nullptr;
    }
    for (size_t i = 1; i < allocations.size(); i += 2) {
        AxFree(Heap, allocations[i]);
        allocations[i] = nullptr;
    }
}

TEST_F(UnifiedHeapAllocatorTest, MetadataTracking)
{
    EXPECT_EQ(Heap->BytesAllocated, 0);
    EXPECT_EQ(Heap->AllocationCount, 0);

    void* ptr1 = AxAlloc(Heap, 100);
    EXPECT_GE(Heap->BytesAllocated, 100);
    EXPECT_EQ(Heap->AllocationCount, 1);

    void* ptr2 = AxAlloc(Heap, 200);
    EXPECT_GE(Heap->BytesAllocated, 300);
    EXPECT_EQ(Heap->AllocationCount, 2);

    AxFree(Heap, ptr1);
    EXPECT_EQ(Heap->AllocationCount, 1);

    AxFree(Heap, ptr2);
    EXPECT_EQ(Heap->AllocationCount, 0);
}

TEST_F(UnifiedHeapAllocatorTest, NameAssignment)
{
    EXPECT_STREQ(Heap->Name, "TestHeap");
}

TEST_F(UnifiedHeapAllocatorTest, TypeSpecificOperationsAreNull)
{
    // Heap allocator should not support Reset, GetMarker, FreeToMarker
    EXPECT_EQ(Heap->Reset, nullptr);
    EXPECT_EQ(Heap->GetMarker, nullptr);
    EXPECT_EQ(Heap->FreeToMarker, nullptr);
}

//=============================================================================
// Linear Allocator Tests (Unified Interface)
//=============================================================================

class UnifiedLinearAllocatorTest : public testing::Test
{
protected:
    struct AxAllocator* Linear;

    void SetUp() override
    {
        Linear = AllocatorAPI->CreateLinear("TestLinear", Megabytes(4));
        ASSERT_NE(Linear, nullptr) << "Failed to create linear allocator";
    }

    void TearDown() override
    {
        if (Linear) {
            Linear->Destroy(Linear);
            Linear = nullptr;
        }
    }
};

TEST_F(UnifiedLinearAllocatorTest, CreateAndDestroy)
{
    EXPECT_NE(Linear, nullptr);
    EXPECT_STREQ(Linear->Name, "TestLinear");
    EXPECT_EQ(Linear->BytesAllocated, 0);
    EXPECT_EQ(Linear->AllocationCount, 0);
}

TEST_F(UnifiedLinearAllocatorTest, SequentialAllocation)
{
    void* ptr1 = AxAlloc(Linear, 100);
    void* ptr2 = AxAlloc(Linear, 100);
    void* ptr3 = AxAlloc(Linear, 100);

    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);
    ASSERT_NE(ptr3, nullptr);

    // Addresses should be increasing (sequential allocation)
    EXPECT_LT((uintptr_t)ptr1, (uintptr_t)ptr2);
    EXPECT_LT((uintptr_t)ptr2, (uintptr_t)ptr3);
}

TEST_F(UnifiedLinearAllocatorTest, FreeIsNoOp)
{
    void* ptr1 = AxAlloc(Linear, 100);
    size_t allocatedBefore = Linear->BytesAllocated;

    // Free should not reclaim memory in linear allocator
    AxFree(Linear, ptr1);

    // BytesAllocated should remain the same (Free is a no-op)
    EXPECT_EQ(Linear->BytesAllocated, allocatedBefore);

    // Should still be able to allocate
    void* ptr2 = AxAlloc(Linear, 100);
    EXPECT_NE(ptr2, nullptr);
}

TEST_F(UnifiedLinearAllocatorTest, Reset)
{
    // Allocate some memory
    void* ptr1 = AxAlloc(Linear, 1000);
    void* ptr2 = AxAlloc(Linear, 1000);
    EXPECT_NE(ptr1, nullptr);
    EXPECT_NE(ptr2, nullptr);
    EXPECT_GT(Linear->BytesAllocated, 0);
    EXPECT_GT(Linear->AllocationCount, 0);

    // Reset should clear all allocations
    ASSERT_NE(Linear->Reset, nullptr) << "Linear allocator should support Reset";
    Linear->Reset(Linear);

    EXPECT_EQ(Linear->BytesAllocated, 0);
    EXPECT_EQ(Linear->AllocationCount, 0);

    // Should be able to allocate again from the beginning
    void* ptr3 = AxAlloc(Linear, 1000);
    EXPECT_NE(ptr3, nullptr);
}

TEST_F(UnifiedLinearAllocatorTest, CapacityLimit)
{
    // Try to allocate more than capacity
    void* ptr = AxAlloc(Linear, Megabytes(5)); // Capacity is 4MB
    EXPECT_EQ(ptr, nullptr) << "Should return NULL when exceeding capacity";
}

TEST_F(UnifiedLinearAllocatorTest, MetadataTracking)
{
    EXPECT_EQ(Linear->BytesAllocated, 0);
    EXPECT_EQ(Linear->AllocationCount, 0);

    void* ptr1 = AxAlloc(Linear, 100);
    EXPECT_GE(Linear->BytesAllocated, 100);
    EXPECT_EQ(Linear->AllocationCount, 1);

    void* ptr2 = AxAlloc(Linear, 200);
    EXPECT_GE(Linear->BytesAllocated, 300);
    EXPECT_EQ(Linear->AllocationCount, 2);

    // Reset should clear metadata
    Linear->Reset(Linear);
    EXPECT_EQ(Linear->BytesAllocated, 0);
    EXPECT_EQ(Linear->AllocationCount, 0);
}

TEST_F(UnifiedLinearAllocatorTest, TypeSpecificOperations)
{
    // Linear allocator should support Reset but not GetMarker/FreeToMarker
    EXPECT_NE(Linear->Reset, nullptr);
    EXPECT_EQ(Linear->GetMarker, nullptr);
    EXPECT_EQ(Linear->FreeToMarker, nullptr);
}

TEST_F(UnifiedLinearAllocatorTest, AlignedAllocations)
{
    // All allocations should be properly aligned
    for (int i = 0; i < 10; i++) {
        void* ptr = AxAlloc(Linear, 1 + i); // Varying sizes
        ASSERT_NE(ptr, nullptr);
        EXPECT_EQ((uintptr_t)ptr % AX_DEFAULT_ALIGNMENT, 0)
            << "Allocation " << i << " not properly aligned";
    }
}

//=============================================================================
// Stack Allocator Tests (Unified Interface)
//=============================================================================

class UnifiedStackAllocatorTest : public testing::Test
{
protected:
    struct AxAllocator* Stack;

    void SetUp() override
    {
        Stack = AllocatorAPI->CreateStack("TestStack", Megabytes(1));
        ASSERT_NE(Stack, nullptr) << "Failed to create stack allocator";
    }

    void TearDown() override
    {
        if (Stack) {
            Stack->Destroy(Stack);
            Stack = nullptr;
        }
    }
};

TEST_F(UnifiedStackAllocatorTest, CreateAndDestroy)
{
    EXPECT_NE(Stack, nullptr);
    EXPECT_STREQ(Stack->Name, "TestStack");
    EXPECT_EQ(Stack->BytesAllocated, 0);
    EXPECT_EQ(Stack->AllocationCount, 0);
}

TEST_F(UnifiedStackAllocatorTest, LIFOAllocation)
{
    // Allocate A, B, C
    void* ptrA = AxAlloc(Stack, 100);
    void* ptrB = AxAlloc(Stack, 200);
    void* ptrC = AxAlloc(Stack, 300);

    ASSERT_NE(ptrA, nullptr);
    ASSERT_NE(ptrB, nullptr);
    ASSERT_NE(ptrC, nullptr);

    // Fill with patterns
    memset(ptrA, 0xAA, 100);
    memset(ptrB, 0xBB, 200);
    memset(ptrC, 0xCC, 300);

    // Free in LIFO order: C, B, A
    // After freeing C, B and A should still have their data
    AxFree(Stack, ptrC);

    uint8_t* bytesA = (uint8_t*)ptrA;
    uint8_t* bytesB = (uint8_t*)ptrB;
    EXPECT_EQ(bytesA[0], 0xAA);
    EXPECT_EQ(bytesB[0], 0xBB);

    AxFree(Stack, ptrB);
    EXPECT_EQ(bytesA[0], 0xAA);

    AxFree(Stack, ptrA);
}

TEST_F(UnifiedStackAllocatorTest, MarkerFree)
{
    // Allocate some initial memory
    void* ptr1 = AxAlloc(Stack, 100);
    void* ptr2 = AxAlloc(Stack, 100);
    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);

    // Get marker
    ASSERT_NE(Stack->GetMarker, nullptr) << "Stack allocator should support GetMarker";
    void* marker = Stack->GetMarker(Stack);

    // Allocate more
    void* ptr3 = AxAlloc(Stack, 100);
    void* ptr4 = AxAlloc(Stack, 100);
    ASSERT_NE(ptr3, nullptr);
    ASSERT_NE(ptr4, nullptr);

    size_t allocatedBefore = Stack->BytesAllocated;
    size_t countBefore = Stack->AllocationCount;

    // Free to marker (should free ptr3 and ptr4)
    ASSERT_NE(Stack->FreeToMarker, nullptr) << "Stack allocator should support FreeToMarker";
    Stack->FreeToMarker(Stack, marker);

    // Should have freed the allocations after the marker
    EXPECT_LT(Stack->BytesAllocated, allocatedBefore);
    EXPECT_LT(Stack->AllocationCount, countBefore);
}

TEST_F(UnifiedStackAllocatorTest, Reset)
{
    void* ptr1 = AxAlloc(Stack, 1000);
    void* ptr2 = AxAlloc(Stack, 1000);
    EXPECT_NE(ptr1, nullptr);
    EXPECT_NE(ptr2, nullptr);
    EXPECT_GT(Stack->BytesAllocated, 0);

    // Reset should free all
    ASSERT_NE(Stack->Reset, nullptr) << "Stack allocator should support Reset";
    Stack->Reset(Stack);

    EXPECT_EQ(Stack->BytesAllocated, 0);
    EXPECT_EQ(Stack->AllocationCount, 0);
}

TEST_F(UnifiedStackAllocatorTest, CapacityLimit)
{
    // Try to allocate more than capacity
    void* ptr = AxAlloc(Stack, Megabytes(2)); // Capacity is 1MB
    EXPECT_EQ(ptr, nullptr) << "Should return NULL when exceeding capacity";
}

TEST_F(UnifiedStackAllocatorTest, TypeSpecificOperations)
{
    // Stack allocator should support all type-specific operations
    EXPECT_NE(Stack->Reset, nullptr);
    EXPECT_NE(Stack->GetMarker, nullptr);
    EXPECT_NE(Stack->FreeToMarker, nullptr);
}

//=============================================================================
// Allocator Registry Tests (Unified Interface)
//=============================================================================

TEST(UnifiedAllocatorRegistry, CreateRegistersAllocator)
{
    size_t initialCount = AllocatorAPI->GetCount();

    struct AxAllocator* heap = AllocatorAPI->CreateHeap("RegistryTest", Kilobytes(64), Megabytes(1));
    ASSERT_NE(heap, nullptr);

    EXPECT_EQ(AllocatorAPI->GetCount(), initialCount + 1);

    heap->Destroy(heap);
}

TEST(UnifiedAllocatorRegistry, DestroyUnregisters)
{
    size_t initialCount = AllocatorAPI->GetCount();

    struct AxAllocator* heap = AllocatorAPI->CreateHeap("UnregisterTest", Kilobytes(64), Megabytes(1));
    ASSERT_NE(heap, nullptr);
    EXPECT_EQ(AllocatorAPI->GetCount(), initialCount + 1);

    heap->Destroy(heap);

    EXPECT_EQ(AllocatorAPI->GetCount(), initialCount);
}

TEST(UnifiedAllocatorRegistry, GetByName)
{
    struct AxAllocator* heap = AllocatorAPI->CreateHeap("NameLookupTest", Kilobytes(64), Megabytes(1));
    ASSERT_NE(heap, nullptr);

    struct AxAllocator* found = AllocatorAPI->GetByName("NameLookupTest");
    EXPECT_EQ(found, heap);

    // Non-existent name should return NULL
    struct AxAllocator* notFound = AllocatorAPI->GetByName("NonExistent");
    EXPECT_EQ(notFound, nullptr);

    heap->Destroy(heap);
}

TEST(UnifiedAllocatorRegistry, GetByIndex)
{
    size_t initialCount = AllocatorAPI->GetCount();

    struct AxAllocator* heap1 = AllocatorAPI->CreateHeap("IndexTest1", Kilobytes(64), Megabytes(1));
    struct AxAllocator* heap2 = AllocatorAPI->CreateHeap("IndexTest2", Kilobytes(64), Megabytes(1));
    ASSERT_NE(heap1, nullptr);
    ASSERT_NE(heap2, nullptr);

    // Should be able to retrieve both by index
    bool found1 = false;
    bool found2 = false;
    size_t count = AllocatorAPI->GetCount();
    for (size_t i = 0; i < count; i++) {
        struct AxAllocator* alloc = AllocatorAPI->GetByIndex(i);
        if (alloc == heap1) found1 = true;
        if (alloc == heap2) found2 = true;
    }
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found2);

    // Out of range should return NULL
    struct AxAllocator* outOfRange = AllocatorAPI->GetByIndex(count + 100);
    EXPECT_EQ(outOfRange, nullptr);

    heap1->Destroy(heap1);
    heap2->Destroy(heap2);
}

TEST(UnifiedAllocatorRegistry, MultipleAllocatorTypes)
{
    size_t initialCount = AllocatorAPI->GetCount();

    struct AxAllocator* heap = AllocatorAPI->CreateHeap("TypeTestHeap", Kilobytes(64), Megabytes(1));
    struct AxAllocator* linear = AllocatorAPI->CreateLinear("TypeTestLinear", Megabytes(1));
    struct AxAllocator* stack = AllocatorAPI->CreateStack("TypeTestStack", Megabytes(1));

    ASSERT_NE(heap, nullptr);
    ASSERT_NE(linear, nullptr);
    ASSERT_NE(stack, nullptr);

    EXPECT_EQ(AllocatorAPI->GetCount(), initialCount + 3);

    // All should be findable by name
    EXPECT_EQ(AllocatorAPI->GetByName("TypeTestHeap"), heap);
    EXPECT_EQ(AllocatorAPI->GetByName("TypeTestLinear"), linear);
    EXPECT_EQ(AllocatorAPI->GetByName("TypeTestStack"), stack);

    heap->Destroy(heap);
    linear->Destroy(linear);
    stack->Destroy(stack);

    EXPECT_EQ(AllocatorAPI->GetCount(), initialCount);
}

//=============================================================================
// Edge Case Tests (Unified Interface)
//=============================================================================

TEST(UnifiedAllocatorEdgeCases, CreateWithZeroCapacity)
{
    struct AxAllocator* linear = AllocatorAPI->CreateLinear("ZeroCapacity", 0);
    EXPECT_EQ(linear, nullptr) << "Should fail with zero capacity";

    struct AxAllocator* stack = AllocatorAPI->CreateStack("ZeroCapacity", 0);
    EXPECT_EQ(stack, nullptr) << "Should fail with zero capacity";
}

TEST(UnifiedAllocatorEdgeCases, FreeNullPointer)
{
    struct AxAllocator* heap = AllocatorAPI->CreateHeap("FreeNullTest", Kilobytes(64), Megabytes(1));
    ASSERT_NE(heap, nullptr);

    // Freeing NULL should not crash
    AxFree(heap, nullptr);

    heap->Destroy(heap);
}

TEST(UnifiedAllocatorEdgeCases, ZeroSizeAllocation)
{
    struct AxAllocator* heap = AllocatorAPI->CreateHeap("ZeroAllocTest", Kilobytes(64), Megabytes(1));
    ASSERT_NE(heap, nullptr);

    // Zero-size allocation behavior is implementation-defined
    void* ptr = AxAlloc(heap, 0);
    // Should either return NULL or a valid minimal allocation

    if (ptr != nullptr) {
        AxFree(heap, ptr);
    }

    heap->Destroy(heap);
}

//=============================================================================
// Polymorphic Usage Tests (Unified Interface)
//=============================================================================

// Helper function that works with any allocator
static void* UseAllocator(struct AxAllocator* alloc, size_t size)
{
    return AxAlloc(alloc, size);
}

TEST(UnifiedPolymorphicUsage, AnyAllocatorCanBeUsed)
{
    struct AxAllocator* heap = AllocatorAPI->CreateHeap("PolyHeap", Kilobytes(64), Megabytes(1));
    struct AxAllocator* linear = AllocatorAPI->CreateLinear("PolyLinear", Megabytes(1));
    struct AxAllocator* stack = AllocatorAPI->CreateStack("PolyStack", Megabytes(1));

    // Same function works with all allocator types
    void* ptr1 = UseAllocator(heap, 100);
    void* ptr2 = UseAllocator(linear, 100);
    void* ptr3 = UseAllocator(stack, 100);

    EXPECT_NE(ptr1, nullptr);
    EXPECT_NE(ptr2, nullptr);
    EXPECT_NE(ptr3, nullptr);

    // Write to all to verify they're usable
    memset(ptr1, 0xAA, 100);
    memset(ptr2, 0xBB, 100);
    memset(ptr3, 0xCC, 100);

    AxFree(heap, ptr1);
    // linear Free is no-op
    AxFree(stack, ptr3);

    heap->Destroy(heap);
    linear->Destroy(linear);
    stack->Destroy(stack);
}
