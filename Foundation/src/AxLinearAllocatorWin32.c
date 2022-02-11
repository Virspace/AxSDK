#include "AxLinearAllocator.h"

#include <stdio.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define PAGE_LIMIT 1
#define PAGE_SIZE 4096

// TODO(mdeforge): If we add platform memory allocation functions to the platform api
//                 we can have one linear allocator source file that's not Win32 specific.

struct AxLinearAllocator
{
    // page size?
    struct AxLinearAllocatorStats Stats; // Allocator stats
    size_t Offset;                     // Offset pointer, updated on each allocation.
    void *Arena;                         // Start of Heap pointer
};

// TODO(mdeforge): Move to another file?
// Helper Functions ///////////////////////////////////////////
inline size_t Align(size_t N)
{
    return (N + sizeof(intptr_t) - 1) & ~(sizeof(intptr_t) - 1);
}

inline size_t RoundSizeToNearestPageMultiple(size_t Value, size_t Multiple)
{
    size_t Remainder = Value % Multiple;
    if (Remainder == 0) {
        return Value;
    }

    return (Value + Multiple - Remainder);
}

inline uint32_t GetSystemPageSize()
{
    SYSTEM_INFO SystemInfo;
    GetSystemInfo(&SystemInfo);

    return ((uint32_t)SystemInfo.dwPageSize);
}
///////////////////////////////////////////////////////////////

// Requests (maps) memory from OS
static struct AxLinearAllocator *Create(size_t MaxSize, void *BaseAddress)
{
    // TODO(mdeforge): Do we need to check for stupid? Probably...
    //                 Could passing zero allow it to grow?
    if (MaxSize == 0) {
        return NULL;
    }

    // TODO(mdeforge): Maybe this can be determine and set by CMake?
    // Get the system page size
    uint32_t PageSize = GetSystemPageSize();

    // Round InitialSize up to the nearest multiple of the system page size.
    MaxSize = RoundSizeToNearestPageMultiple(MaxSize, PageSize);

    // Reserve a block of MaxSize in the process's virtual address space for this heap.
    // The maximum size determines the total number of reserved pages.
    BaseAddress = VirtualAlloc(BaseAddress, MaxSize, MEM_RESERVE, PAGE_READWRITE);

    // Use some space at the beginning of the heap to store information
    struct AxLinearAllocator *Allocator = BaseAddress;

    // Construct the heap
    if (Allocator)
    {
        Allocator->Arena = (uint8_t *)BaseAddress + sizeof(struct AxLinearAllocator);
        Allocator->Offset = 0;
        Allocator->Stats = (struct AxLinearAllocatorStats) {
            .BytesUsed = 0,
            .MaxSize = MaxSize,
            .PageCount = MaxSize / PageSize
        };
    }

    return (Allocator);
}

static void Destroy(struct AxLinearAllocator *Allocator)
{
    if (!Allocator) {
        return;
    }

    VirtualFree(Allocator, 0, MEM_RELEASE); // Will decommit and release
}

static void *Alloc(struct AxLinearAllocator *Allocator, size_t Size, const char *File, uint32_t Line)
{
    if (!Allocator) {
        return (NULL);
    }

    // Adjust alignment and check against max size
    size_t AdjustedSize = Align(Size);
    if (AdjustedSize > Allocator->Stats.MaxSize) {
        return (NULL);
    }

    // Allocate
    VirtualAlloc((uint8_t *)Allocator->Arena + Allocator->Offset, AdjustedSize, MEM_COMMIT, PAGE_READWRITE);

    // Update Arena Info
    Allocator->Offset += AdjustedSize;
    Allocator->Stats.BytesUsed += AdjustedSize;

    // User payload
    return ((uint8_t *)Allocator->Arena + Allocator->Offset);
}

static void Free(struct AxLinearAllocator *Allocator, const char *File, uint32_t Line)
{
    if (!Allocator) {
        return;
    }

    // If lpAddress is the base address returned by VirtualAlloc and dwSize is 0 (zero), the function decommits
    // the entire region that is allocated by VirtualAlloc. After that, the entire region is in the reserved state.
    VirtualFree(Allocator->Arena, 0, MEM_DECOMMIT);
    Allocator->Offset = 0;
    Allocator->Stats = (struct AxLinearAllocatorStats){ 0 };
}

struct AxLinearAllocatorAPI *AxLinearAllocatorAPI = &(struct AxLinearAllocatorAPI) {
    .Create = Create,
    .Destroy = Destroy,
    .Alloc = Alloc,
    .Free = Free
};