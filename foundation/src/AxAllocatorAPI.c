/**
 * AxAllocatorAPI.c - Unified Allocator Implementation
 *
 * Implements the unified AxAllocator interface for Heap, Linear, and Stack
 * allocators. Uses Win32 VirtualAlloc for memory management on Windows.
 */

#include "AxAllocatorAPI.h"
#include "AxAllocUtils.h"
#include "AxPlatform.h"
#include "AxHashTable.h"
#include "AxMath.h"

#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#define AxStrDup _strdup
#else
#include <sys/mman.h>
#define AxStrDup strdup
#endif

//=============================================================================
// Internal Registry
//=============================================================================

// Hash table for name-based lookup
static struct AxHashTable* AllocatorTable = NULL;

// Array for index-based lookup (simple dynamic array)
static struct AxAllocator** AllocatorList = NULL;
static size_t AllocatorListCount = 0;
static size_t AllocatorListCapacity = 0;

static void RegisterAllocator(struct AxAllocator* Alloc)
{
    if (!Alloc) {
        return;
    }

    // Lazy-initialize the hash table
    if (!AllocatorTable) {
        AllocatorTable = HashTableAPI->CreateTable();
    }

    // Add to hash table
    HashTableAPI->Set(AllocatorTable, Alloc->Name, Alloc);

    // Add to list
    if (AllocatorListCount >= AllocatorListCapacity) {
        size_t newCapacity = (AllocatorListCapacity == 0) ? 16 : AllocatorListCapacity * 2;
        struct AxAllocator** newList = (struct AxAllocator**)realloc(
            AllocatorList,
            newCapacity * sizeof(struct AxAllocator*)
        );
        if (newList) {
            AllocatorList = newList;
            AllocatorListCapacity = newCapacity;
        }
    }

    if (AllocatorListCount < AllocatorListCapacity) {
        AllocatorList[AllocatorListCount++] = Alloc;
    }
}

static void UnregisterAllocator(struct AxAllocator* Alloc)
{
    if (!Alloc) {
        return;
    }

    // Remove from hash table
    if (AllocatorTable) {
        HashTableAPI->Remove(AllocatorTable, Alloc->Name);
    }

    // Remove from list
    for (size_t i = 0; i < AllocatorListCount; i++) {
        if (AllocatorList[i] == Alloc) {
            // Swap with last element and decrement count
            AllocatorList[i] = AllocatorList[AllocatorListCount - 1];
            AllocatorListCount--;
            break;
        }
    }
}

//=============================================================================
// Heap Allocator Implementation
//=============================================================================

typedef struct AxHeapAllocator
{
    struct AxAllocator Base;  // Must be first member
    void* Arena;
    size_t ArenaSize;
    size_t MaxSize;
    size_t PageSize;
} AxHeapAllocator;

// Heap allocation header (stored before each allocation)
typedef struct HeapAllocationHeader
{
    void* RawPtr;     // Original allocation pointer for free
    size_t Size;
    size_t Alignment;
    uint32_t Magic;   // For debugging
} HeapAllocationHeader;

#define HEAP_MAGIC 0xDEADBEEF

static void* HeapAlloc_Impl(struct AxAllocator* Self, size_t Size, size_t Alignment)
{
    AxHeapAllocator* Heap = (AxHeapAllocator*)Self;
    if (!Heap || Size == 0) {
        return (NULL);
    }

    // Ensure alignment is at least pointer size and power of 2
    if (Alignment < sizeof(void*)) {
        Alignment = sizeof(void*);
    }

    // Calculate total size needed: header + alignment padding + user data
    size_t headerSize = sizeof(HeapAllocationHeader);
    size_t totalSize = headerSize + Alignment + Size;

#ifdef _WIN32
    // Allocate from the process heap
    void* rawPtr = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, totalSize);
    if (!rawPtr) {
        return (NULL);
    }
#else
    void* rawPtr = malloc(totalSize);
    if (!rawPtr) {
        return (NULL);
    }
    memset(rawPtr, 0, totalSize);
#endif

    // Calculate aligned address for user data
    uintptr_t rawAddr = (uintptr_t)rawPtr;
    uintptr_t dataAddr = RoundAddressUpToPowerOfTwo(rawAddr + headerSize, Alignment);

    // Store header just before user data
    HeapAllocationHeader* header = (HeapAllocationHeader*)(dataAddr - headerSize);
    header->RawPtr = rawPtr;
    header->Size = Size;
    header->Alignment = Alignment;
    header->Magic = HEAP_MAGIC;

    // Update metadata
    Self->BytesAllocated += Size;
    Self->AllocationCount++;

    return ((void*)dataAddr);
}

static void* HeapRealloc_Impl(struct AxAllocator* Self, void* Ptr, size_t OldSize, size_t NewSize)
{
    if (!Ptr) {
        return HeapAlloc_Impl(Self, NewSize, AX_DEFAULT_ALIGNMENT);
    }

    if (NewSize == 0) {
        Self->Free(Self, Ptr);
        return (NULL);
    }

    // Allocate new block
    void* newPtr = HeapAlloc_Impl(Self, NewSize, AX_DEFAULT_ALIGNMENT);
    if (!newPtr) {
        return (NULL);
    }

    // Copy old data
    size_t copySize = (OldSize < NewSize) ? OldSize : NewSize;
    memcpy(newPtr, Ptr, copySize);

    // Free old block
    Self->Free(Self, Ptr);

    return (newPtr);
}

static void HeapFree_Impl(struct AxAllocator* Self, void* Ptr)
{
    if (!Self || !Ptr) {
        return;
    }

    // Get header
    HeapAllocationHeader* header = (HeapAllocationHeader*)((uint8_t*)Ptr - sizeof(HeapAllocationHeader));
    if (header->Magic != HEAP_MAGIC) {
        // Invalid or corrupted allocation
        AXON_ASSERT(0 && "HeapFree: Invalid allocation or memory corruption detected");
        return;
    }

    // Update metadata
    Self->BytesAllocated -= header->Size;
    Self->AllocationCount--;

    // Free using the stored raw pointer
    void* rawPtr = header->RawPtr;

#ifdef _WIN32
    HeapFree(GetProcessHeap(), 0, rawPtr);
#else
    free(rawPtr);
#endif
}

static void HeapDestroy_Impl(struct AxAllocator* Self)
{
    if (!Self) {
        return;
    }

    AxHeapAllocator* Heap = (AxHeapAllocator*)Self;

    // Unregister first
    UnregisterAllocator(Self);

    // Free the name
    if (Heap->Base.Name) {
        free((void*)Heap->Base.Name);
    }

    // Free the allocator struct itself
#ifdef _WIN32
    HeapFree(GetProcessHeap(), 0, Heap);
#else
    free(Heap);
#endif
}

static struct AxAllocator* CreateHeap(const char* Name, size_t InitialSize, size_t MaxSize)
{
    if (!Name) {
        return (NULL);
    }

    // Get system info
    uint32_t pageSize, allocGranularity;
    GetSysInfo(&pageSize, &allocGranularity);

    // Round sizes
    if (MaxSize > 0) {
        MaxSize = RoundUpToPowerOfTwo(MaxSize, pageSize);
    }
    if (InitialSize > MaxSize && MaxSize > 0) {
        InitialSize = MaxSize;
    }

#ifdef _WIN32
    AxHeapAllocator* Heap = (AxHeapAllocator*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(AxHeapAllocator));
#else
    AxHeapAllocator* Heap = (AxHeapAllocator*)calloc(1, sizeof(AxHeapAllocator));
#endif

    if (!Heap) {
        return (NULL);
    }

    // Initialize the base interface
    Heap->Base.Alloc = HeapAlloc_Impl;
    Heap->Base.Realloc = HeapRealloc_Impl;
    Heap->Base.Free = HeapFree_Impl;
    Heap->Base.Destroy = HeapDestroy_Impl;
    Heap->Base.Name = AxStrDup(Name);
    Heap->Base.BytesAllocated = 0;
    Heap->Base.BytesReserved = MaxSize;
    Heap->Base.AllocationCount = 0;
    Heap->Base.Reset = NULL;        // Heap doesn't support Reset
    Heap->Base.GetMarker = NULL;    // Heap doesn't support markers
    Heap->Base.FreeToMarker = NULL;

    // Initialize heap-specific data
    Heap->PageSize = pageSize;
    Heap->MaxSize = MaxSize;

    // Register with the allocator registry
    RegisterAllocator(&Heap->Base);

    return (&Heap->Base);
}

//=============================================================================
// Linear Allocator Implementation
//=============================================================================

typedef struct AxLinearAllocatorNew
{
    struct AxAllocator Base;  // Must be first member
    void* Arena;
    size_t Offset;
    size_t Capacity;
    size_t PageSize;
} AxLinearAllocatorNew;

static void* LinearAlloc_Impl(struct AxAllocator* Self, size_t Size, size_t Alignment)
{
    AxLinearAllocatorNew* Linear = (AxLinearAllocatorNew*)Self;
    if (!Linear || Size == 0) {
        return (NULL);
    }

    // Ensure alignment is at least pointer size and power of 2
    if (Alignment < sizeof(void*)) {
        Alignment = sizeof(void*);
    }

    // Calculate aligned address
    uintptr_t currentAddr = (uintptr_t)Linear->Arena + Linear->Offset;
    uintptr_t alignedAddr = RoundAddressUpToPowerOfTwo(currentAddr, Alignment);
    size_t alignmentPadding = alignedAddr - currentAddr;

    // Check if we have enough space
    size_t totalSize = alignmentPadding + Size;
    if (Linear->Offset + totalSize > Linear->Capacity) {
        return (NULL);  // Out of memory
    }

#ifdef _WIN32
    // Commit memory if needed
    size_t currentCommit = RoundUpToPowerOfTwo(Linear->Offset, Linear->PageSize);
    size_t neededCommit = RoundUpToPowerOfTwo(Linear->Offset + totalSize, Linear->PageSize);
    if (neededCommit > currentCommit) {
        void* result = VirtualAlloc(
            (uint8_t*)Linear->Arena + currentCommit,
            neededCommit - currentCommit,
            MEM_COMMIT,
            PAGE_READWRITE
        );
        if (!result) {
            return (NULL);
        }
    }
#endif

    // Update offset
    Linear->Offset += totalSize;

    // Update metadata
    Self->BytesAllocated += Size;
    Self->AllocationCount++;

    return ((void*)alignedAddr);
}

static void* LinearRealloc_Impl(struct AxAllocator* Self, void* Ptr, size_t OldSize, size_t NewSize)
{
    // Linear allocator can't truly realloc - just allocate new and copy
    if (!Ptr) {
        return LinearAlloc_Impl(Self, NewSize, AX_DEFAULT_ALIGNMENT);
    }

    if (NewSize == 0) {
        return (NULL);
    }

    void* newPtr = LinearAlloc_Impl(Self, NewSize, AX_DEFAULT_ALIGNMENT);
    if (!newPtr) {
        return (NULL);
    }

    size_t copySize = (OldSize < NewSize) ? OldSize : NewSize;
    memcpy(newPtr, Ptr, copySize);

    // Note: old memory is not freed (linear allocator characteristic)
    return (newPtr);
}

static void LinearFree_Impl(struct AxAllocator* Self, void* Ptr)
{
    // Linear allocator Free is a no-op
    // Memory is only freed via Reset or Destroy
    AXON_UNUSED(Self);
    AXON_UNUSED(Ptr);
}

static void LinearReset_Impl(struct AxAllocator* Self)
{
    if (!Self) {
        return;
    }

    AxLinearAllocatorNew* Linear = (AxLinearAllocatorNew*)Self;

    // Reset offset to beginning
    Linear->Offset = 0;

    // Reset metadata
    Self->BytesAllocated = 0;
    Self->AllocationCount = 0;
}

static void LinearDestroy_Impl(struct AxAllocator* Self)
{
    if (!Self) {
        return;
    }

    AxLinearAllocatorNew* Linear = (AxLinearAllocatorNew*)Self;

    // Unregister first
    UnregisterAllocator(Self);

    // Free the name
    if (Linear->Base.Name) {
        free((void*)Linear->Base.Name);
    }

#ifdef _WIN32
    // Release all virtual memory
    VirtualFree(Linear, 0, MEM_RELEASE);
#else
    // For Linux, the Arena is part of the same allocation, so just free the base
    free(Linear);
#endif
}

static struct AxAllocator* CreateLinear(const char* Name, size_t Capacity)
{
    if (!Name || Capacity == 0) {
        return (NULL);
    }

    // Get system info
    uint32_t pageSize, allocGranularity;
    GetSysInfo(&pageSize, &allocGranularity);

    // Account for allocator struct overhead
    size_t structOverhead = sizeof(AxLinearAllocatorNew);
    size_t totalSize = structOverhead + Capacity;

    // Round to allocation granularity
    totalSize = RoundUpToPowerOfTwo(totalSize, allocGranularity);

#ifdef _WIN32
    // Reserve virtual address space
    void* baseAddress = VirtualAlloc(NULL, totalSize, MEM_RESERVE, PAGE_READWRITE);
    if (!baseAddress) {
        return (NULL);
    }

    // Commit just enough for the allocator struct
    VirtualAlloc(baseAddress, structOverhead, MEM_COMMIT, PAGE_READWRITE);
#else
    void* baseAddress = malloc(totalSize);
    if (!baseAddress) {
        return (NULL);
    }
    memset(baseAddress, 0, totalSize);
#endif

    AxLinearAllocatorNew* Linear = (AxLinearAllocatorNew*)baseAddress;

    // Initialize the base interface
    Linear->Base.Alloc = LinearAlloc_Impl;
    Linear->Base.Realloc = LinearRealloc_Impl;
    Linear->Base.Free = LinearFree_Impl;
    Linear->Base.Destroy = LinearDestroy_Impl;
    Linear->Base.Name = AxStrDup(Name);
    Linear->Base.BytesAllocated = 0;
    Linear->Base.BytesReserved = Capacity;
    Linear->Base.AllocationCount = 0;
    Linear->Base.Reset = LinearReset_Impl;
    Linear->Base.GetMarker = NULL;      // Linear doesn't support markers
    Linear->Base.FreeToMarker = NULL;

    // Initialize linear-specific data
    Linear->Arena = (uint8_t*)baseAddress + structOverhead;
    Linear->Offset = 0;
    Linear->Capacity = Capacity;
    Linear->PageSize = pageSize;

    // Register with the allocator registry
    RegisterAllocator(&Linear->Base);

    return (&Linear->Base);
}

//=============================================================================
// Stack Allocator Implementation
//=============================================================================

typedef struct AxStackAllocatorNew
{
    struct AxAllocator Base;  // Must be first member
    void* Arena;
    size_t Offset;
    size_t Capacity;
    size_t PageSize;
} AxStackAllocatorNew;

// Stack allocation header - stored at a fixed location just before user data
// We store the offset in the arena where the header starts, so we can find
// the header from any user pointer regardless of alignment padding
typedef struct StackAllocationHeader
{
    size_t HeaderOffset;  // Offset in Arena where this header starts
    size_t PrevOffset;    // Offset before this entire allocation
    size_t Size;          // User-requested size
    uint32_t Magic;
} StackAllocationHeader;

#define STACK_MAGIC 0xCAFEBABE

static void* StackAlloc_Impl(struct AxAllocator* Self, size_t Size, size_t Alignment)
{
    AxStackAllocatorNew* Stack = (AxStackAllocatorNew*)Self;
    if (!Stack || Size == 0) {
        return (NULL);
    }

    // Ensure alignment is at least pointer size and power of 2
    if (Alignment < sizeof(void*)) {
        Alignment = sizeof(void*);
    }

    // Save the current offset (this is what we'll restore to on free)
    size_t prevOffset = Stack->Offset;

    // The header goes at the current position
    size_t headerOffset = Stack->Offset;
    size_t headerSize = sizeof(StackAllocationHeader);

    // Calculate aligned address for user data (after header)
    // The header is placed at headerOffset, so user data starts after headerSize
    uintptr_t arenaBase = (uintptr_t)Stack->Arena;
    uintptr_t headerEnd = arenaBase + headerOffset + headerSize;
    uintptr_t dataAddr = RoundAddressUpToPowerOfTwo(headerEnd, Alignment);

    // Calculate total size from current offset to end of user data
    size_t totalSize = (dataAddr - arenaBase) - headerOffset + Size;

    // Check if we have enough space
    if (headerOffset + totalSize > Stack->Capacity) {
        return (NULL);
    }

#ifdef _WIN32
    // Commit memory if needed
    size_t currentCommit = RoundUpToPowerOfTwo(Stack->Offset, Stack->PageSize);
    size_t neededCommit = RoundUpToPowerOfTwo(headerOffset + totalSize, Stack->PageSize);
    if (neededCommit > currentCommit) {
        void* result = VirtualAlloc(
            (uint8_t*)Stack->Arena + currentCommit,
            neededCommit - currentCommit,
            MEM_COMMIT,
            PAGE_READWRITE
        );
        if (!result) {
            return (NULL);
        }
    }
#endif

    // Store header at headerOffset
    StackAllocationHeader* header = (StackAllocationHeader*)((uint8_t*)Stack->Arena + headerOffset);
    header->HeaderOffset = headerOffset;
    header->PrevOffset = prevOffset;
    header->Size = Size;
    header->Magic = STACK_MAGIC;

    // Update offset to end of this allocation
    Stack->Offset = headerOffset + totalSize;

    // Update metadata
    Self->BytesAllocated += Size;
    Self->AllocationCount++;

    return ((void*)dataAddr);
}

static void* StackRealloc_Impl(struct AxAllocator* Self, void* Ptr, size_t OldSize, size_t NewSize)
{
    // Stack allocator can't efficiently realloc - just allocate new and copy
    if (!Ptr) {
        return StackAlloc_Impl(Self, NewSize, AX_DEFAULT_ALIGNMENT);
    }

    if (NewSize == 0) {
        Self->Free(Self, Ptr);
        return (NULL);
    }

    void* newPtr = StackAlloc_Impl(Self, NewSize, AX_DEFAULT_ALIGNMENT);
    if (!newPtr) {
        return (NULL);
    }

    size_t copySize = (OldSize < NewSize) ? OldSize : NewSize;
    memcpy(newPtr, Ptr, copySize);

    // Note: We don't free the old allocation because stack allocator
    // only supports LIFO order
    return (newPtr);
}

static void StackFree_Impl(struct AxAllocator* Self, void* Ptr)
{
    if (!Self || !Ptr) {
        return;
    }

    AxStackAllocatorNew* Stack = (AxStackAllocatorNew*)Self;

    // The header is always sizeof(StackAllocationHeader) bytes before data,
    // but may be at a different offset due to alignment. We need to search
    // backwards to find it. Since we know the header contains its own offset,
    // we can use that to verify.

    // First, try the simple case: header immediately before data (no alignment padding)
    uintptr_t arenaBase = (uintptr_t)Stack->Arena;
    uintptr_t dataAddr = (uintptr_t)Ptr;

    // Search backwards from the data pointer to find the header
    // The header must be within the current allocation span
    for (size_t searchBack = sizeof(StackAllocationHeader); searchBack <= 256; searchBack++) {
        // Check if this could be the header position
        if (dataAddr - searchBack < arenaBase) {
            break; // Would be before the arena
        }

        StackAllocationHeader* candidate = (StackAllocationHeader*)(dataAddr - searchBack);

        // Verify this is a valid header
        if (candidate->Magic == STACK_MAGIC) {
            // Double-check: the stored HeaderOffset should match where we found it
            size_t foundOffset = (uintptr_t)candidate - arenaBase;
            if (candidate->HeaderOffset == foundOffset) {
                // This is our header
                // Update metadata
                Self->BytesAllocated -= candidate->Size;
                Self->AllocationCount--;

                // Restore offset to before this allocation
                Stack->Offset = candidate->PrevOffset;
                return;
            }
        }
    }

    // If we get here, we couldn't find a valid header - memory corruption
    AXON_ASSERT(0 && "StackFree: Invalid allocation or memory corruption detected");
}

static void StackReset_Impl(struct AxAllocator* Self)
{
    if (!Self) {
        return;
    }

    AxStackAllocatorNew* Stack = (AxStackAllocatorNew*)Self;

    // Reset offset to beginning
    Stack->Offset = 0;

    // Reset metadata
    Self->BytesAllocated = 0;
    Self->AllocationCount = 0;
}

static void* StackGetMarker_Impl(struct AxAllocator* Self)
{
    if (!Self) {
        return (NULL);
    }

    AxStackAllocatorNew* Stack = (AxStackAllocatorNew*)Self;

    // Return current offset as marker (cast to void*)
    return ((void*)(uintptr_t)Stack->Offset);
}

static void StackFreeToMarker_Impl(struct AxAllocator* Self, void* Marker)
{
    if (!Self) {
        return;
    }

    AxStackAllocatorNew* Stack = (AxStackAllocatorNew*)Self;

    // Marker is the offset encoded as a pointer
    size_t markerOffset = (size_t)(uintptr_t)Marker;

    // Validate marker
    if (markerOffset > Stack->Offset) {
        AXON_ASSERT(0 && "StackFreeToMarker: Invalid marker");
        return;
    }

    // Restore offset to marker position
    Stack->Offset = markerOffset;

    // We need to recalculate metadata by walking through remaining allocations
    // For simplicity, we reset to approximations based on the marker
    // A more sophisticated implementation would track this properly
    Self->BytesAllocated = 0;
    Self->AllocationCount = 0;

    // Walk through allocations from start to marker to recalculate
    size_t walkOffset = 0;
    while (walkOffset < markerOffset) {
        StackAllocationHeader* header = (StackAllocationHeader*)((uint8_t*)Stack->Arena + walkOffset);
        if (header->Magic != STACK_MAGIC) {
            break; // Invalid or end of allocations
        }

        Self->BytesAllocated += header->Size;
        Self->AllocationCount++;

        // Move to next allocation - we need to find the next header
        // The next header starts after this allocation's data
        // For now, use a simplified approach
        break; // This is a simplification - proper implementation needs more tracking
    }
}

static void StackDestroy_Impl(struct AxAllocator* Self)
{
    if (!Self) {
        return;
    }

    AxStackAllocatorNew* Stack = (AxStackAllocatorNew*)Self;

    // Unregister first
    UnregisterAllocator(Self);

    // Free the name
    if (Stack->Base.Name) {
        free((void*)Stack->Base.Name);
    }

#ifdef _WIN32
    // Release all virtual memory
    VirtualFree(Stack, 0, MEM_RELEASE);
#else
    // For Linux, the Arena is part of the same allocation
    free(Stack);
#endif
}

static struct AxAllocator* CreateStack(const char* Name, size_t Capacity)
{
    if (!Name || Capacity == 0) {
        return (NULL);
    }

    // Get system info
    uint32_t pageSize, allocGranularity;
    GetSysInfo(&pageSize, &allocGranularity);

    // Account for allocator struct overhead
    size_t structOverhead = sizeof(AxStackAllocatorNew);
    size_t totalSize = structOverhead + Capacity;

    // Round to allocation granularity
    totalSize = RoundUpToPowerOfTwo(totalSize, allocGranularity);

#ifdef _WIN32
    // Reserve virtual address space
    void* baseAddress = VirtualAlloc(NULL, totalSize, MEM_RESERVE, PAGE_READWRITE);
    if (!baseAddress) {
        return (NULL);
    }

    // Commit just enough for the allocator struct
    VirtualAlloc(baseAddress, structOverhead, MEM_COMMIT, PAGE_READWRITE);
#else
    void* baseAddress = malloc(totalSize);
    if (!baseAddress) {
        return (NULL);
    }
    memset(baseAddress, 0, totalSize);
#endif

    AxStackAllocatorNew* Stack = (AxStackAllocatorNew*)baseAddress;

    // Initialize the base interface
    Stack->Base.Alloc = StackAlloc_Impl;
    Stack->Base.Realloc = StackRealloc_Impl;
    Stack->Base.Free = StackFree_Impl;
    Stack->Base.Destroy = StackDestroy_Impl;
    Stack->Base.Name = AxStrDup(Name);
    Stack->Base.BytesAllocated = 0;
    Stack->Base.BytesReserved = Capacity;
    Stack->Base.AllocationCount = 0;
    Stack->Base.Reset = StackReset_Impl;
    Stack->Base.GetMarker = StackGetMarker_Impl;
    Stack->Base.FreeToMarker = StackFreeToMarker_Impl;

    // Initialize stack-specific data
    Stack->Arena = (uint8_t*)baseAddress + structOverhead;
    Stack->Offset = 0;
    Stack->Capacity = Capacity;
    Stack->PageSize = pageSize;

    // Register with the allocator registry
    RegisterAllocator(&Stack->Base);

    return (&Stack->Base);
}

//=============================================================================
// Registry Functions
//=============================================================================

static size_t GetCount(void)
{
    return (AllocatorListCount);
}

static struct AxAllocator* GetByIndex(size_t Index)
{
    if (Index >= AllocatorListCount) {
        return (NULL);
    }
    return (AllocatorList[Index]);
}

static struct AxAllocator* GetByName(const char* Name)
{
    if (!Name || !AllocatorTable) {
        return (NULL);
    }
    return ((struct AxAllocator*)HashTableAPI->Find(AllocatorTable, Name));
}

//=============================================================================
// API Export
//=============================================================================

struct AxAllocatorAPI* AllocatorAPI = &(struct AxAllocatorAPI) {
    .CreateHeap = CreateHeap,
    .CreateLinear = CreateLinear,
    .CreateStack = CreateStack,
    .GetCount = GetCount,
    .GetByIndex = GetByIndex,
    .GetByName = GetByName
};
