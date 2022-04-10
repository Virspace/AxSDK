#include "AxLinearAllocator.h"
#include "AxAllocatorInfo.h"
#include "AxAllocUtils.h"

#include <stdio.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// TODO(mdeforge): If we add platform memory allocation functions to the platform api
//                 we can have one linear allocator source file that's not Win32 specific.

struct AxLinearAllocator
{
    struct AxAllocatorInfo Info;
    size_t ByteOffset;              // Offset pointer, updated on each allocation.
    void *Arena;                    // Start of Heap pointer
};

// TODO(mdeforge): Move to platform?
inline uint16_t GetSystemPageSize()
{
    SYSTEM_INFO SystemInfo;
    GetSystemInfo(&SystemInfo);

    // Seems safe to assume a uint16_t is large enough
    return ((uint16_t)SystemInfo.dwPageSize);
}

static void *Alloc(struct AxLinearAllocator *Allocator, size_t Size, const char *File, uint32_t Line)
{
    Assert(Allocator && "AxLinearAllocator passed NULL allocator!");
    if (!Allocator) {
        return (NULL);
    }

    // Adjust alignment and check against max size
    size_t BytesRequested = Align(Size);
    if (BytesRequested > Allocator->Info.BytesReserved) {
        return (NULL);
    }

    // Allocate
    VirtualAlloc((uint8_t *)Allocator->Arena + Allocator->ByteOffset, BytesRequested, MEM_COMMIT, PAGE_READWRITE);

    // Update Arena Info
    Allocator->ByteOffset += BytesRequested;
    Allocator->Info.BytesCommitted = Allocator->ByteOffset;
    Allocator->Info.PagesCommitted = RoundSizeToNearestPageMultiple(
        Allocator->Info.BytesCommitted, Allocator->Info.PageSize) / 4096;

    // User payload
    return ((uint8_t *)Allocator->Arena + Allocator->ByteOffset);
}

static void Free(struct AxLinearAllocator *Allocator, const char *File, uint32_t Line)
{
    Assert(Allocator && "AxLinearAllocator passed NULL allocator!");
    if (!Allocator) {
        return;
    }

    // If lpAddress is the base address returned by VirtualAlloc and dwSize is 0 (zero), the function decommits
    // the entire region that is allocated by VirtualAlloc. After that, the entire region is in the reserved state.
    VirtualFree(Allocator->Arena, 0, MEM_DECOMMIT);
    Allocator->ByteOffset = 0;
    Allocator->Info = (struct AxAllocatorInfo){ 0 };
}

static struct AxLinearAllocator *Create(size_t MaxSize, void *BaseAddress)
{
    // TODO(mdeforge): Do we need to check for stupid? Probably...
    //                 Could passing zero allow it to grow?
    if (MaxSize == 0) {
        return NULL;
    }

    // TODO(mdeforge): Maybe this can be determine and set by CMake?
    // Get the system page size
    uint16_t PageSize = GetSystemPageSize();

    // Round InitialSize up to the nearest multiple of the system page size.
    MaxSize = RoundSizeToNearestPageMultiple(MaxSize, PageSize);

    // Reserve a block of MaxSize in the process's virtual address space for this heap.
    // The maximum size determines the total number of reserved pages.
    BaseAddress = VirtualAlloc(BaseAddress, MaxSize, MEM_RESERVE, PAGE_READWRITE);

    // Use some space at the beginning of the heap to store information
    VirtualAlloc(BaseAddress, sizeof(struct AxLinearAllocator), MEM_COMMIT, PAGE_READWRITE);
    struct AxLinearAllocator *Allocator = BaseAddress;

    // Construct the heap
    if (Allocator)
    {
        Allocator->Arena = (uint8_t *)BaseAddress + sizeof(struct AxLinearAllocator);
        Allocator->ByteOffset = 0;
        Allocator->Info = (struct AxAllocatorInfo) {
            .BytesReserved = MaxSize,
            .BytesCommitted = 0,
            .PageSize = PageSize,
            .PagesReserved = MaxSize / (size_t)PageSize,
            .PagesCommitted = 0
        };

        // Register with Allocator API
        AllocatorInfoRegistryAPI->Register(&Allocator->Info);
    }

    return (Allocator);
}

static void Destroy(struct AxLinearAllocator *Allocator)
{
    Assert(Allocator && "AxLinearAllocator passed NULL allocator!");
    if (!Allocator) {
        return;
    }

    VirtualFree(Allocator, 0, MEM_RELEASE); // Will decommit and release
}

struct AxLinearAllocatorAPI *LinearAllocatorAPI = &(struct AxLinearAllocatorAPI) {
    .Create = Create,
    .Destroy = Destroy,
    .Alloc = Alloc,
    .Free = Free,
    //.Stats = Stats
};