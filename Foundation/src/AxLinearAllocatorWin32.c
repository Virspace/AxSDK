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
    struct AxAllocatorInfo Info;  // Allocator info
    void *Arena;                  // Start of heap pointer
};

// TODO(mdeforge): Move to platform?
inline void GetSysInfo(uint32_t *PageSize, uint32_t *AllocationGranularity)
{
    SYSTEM_INFO SystemInfo;
    GetSystemInfo(&SystemInfo);

    // Seems safe to assume a uint16_t is large enough
    *PageSize = (uint32_t)SystemInfo.dwPageSize;
    *AllocationGranularity = (uint32_t)SystemInfo.dwAllocationGranularity;
}

static void *Alloc(struct AxLinearAllocator *Allocator, size_t Size, const char *File, uint32_t Line)
{
    Assert(Allocator && "AxLinearAllocator passed NULL allocator!");
    if (!Allocator) {
        return (NULL);
    }

    // Memory committed will be a multiple of the page size, so round to it
    size_t BytesRequestedRoundedToPageSize = RoundSizeToNearestMultiple(Size, Allocator->Info.PageSize);

    // Check if we have the bytes available to meet the allocation request
    size_t BytesAvailable = Allocator->Info.BytesReserved - Allocator->Info.BytesAllocated;
    if (BytesRequestedRoundedToPageSize > BytesAvailable) {
        return (NULL);
    }

    // TODO(mdeforge): Double-check types
    // Check to see if we need to commit more memory or if we already have some committed that we can return.
    size_t BytesAllocatedRoundedToPageSize = RoundSizeToNearestMultiple(Allocator->Info.BytesAllocated, Allocator->Info.PageSize);
    int64_t BytesLeftInPage = (int64_t)(BytesAllocatedRoundedToPageSize - Allocator->Info.BytesAllocated - Size);
    if (BytesLeftInPage < 0)
    {
        // Figure out how many pages we need to commit
        size_t PagesNeeded = RoundSizeToNearestMultiple(llabs(BytesLeftInPage), Allocator->Info.PageSize);
        size_t BytesNeeded = PagesNeeded * Allocator->Info.PageSize;

        // TODO(mdeforge): Do we need to round up the base address then?

        // TODO(mdeforge): Check for virtual alloc failure
        VirtualAlloc((uint8_t *)Allocator->Arena + Allocator->Info.BytesAllocated, BytesNeeded, MEM_COMMIT, PAGE_READWRITE);

        // Does Info reserved memory go down as committed memory goes up?
        Allocator->Info.BytesReserved -= BytesRequestedRoundedToPageSize;
    }
    // else
    // {
    //     // TODO(mdeforge): Consider putting BytesAllocated back in the AxLinearAllocator struct so that Info
    //     // can be compiled out during release builds? Or do we always want Info in there?
    // }

    // Update Arena Info
    Allocator->Info.BytesAllocated += BytesRequestedRoundedToPageSize;

    // Update stats
    Allocator->Info.BytesAllocated += Size;
    Allocator->Info.BytesCommitted = Allocator->Info.BytesAllocated;
    Allocator->Info.PagesCommitted = RoundSizeToNearestMultiple(Allocator->Info.BytesCommitted, Allocator->Info.PageSize) / 4096;

    // User payload
    return ((uint8_t *)Allocator->Arena + Allocator->Info.BytesAllocated);
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
    Allocator->Info = (struct AxAllocatorInfo){ 0 }; // TODO(mdeforge): Can we re-zero-initialize a struct?
}

static struct AxLinearAllocator *Create(size_t MaxSize, void *BaseAddress)
{
    if (MaxSize == 0) {
        return NULL;
    }

    // Get the system page size and allocator granularity
    uint32_t PageSize, AllocatorGranularity;
    GetSysInfo(&PageSize, &AllocatorGranularity);

    // Memory reserved will be a multiple of the system allocation granularity
    MaxSize = RoundSizeToNearestMultiple(MaxSize, AllocatorGranularity);

    // TODO(mdeforge): Check for virtual alloc failure, like maybe the base address is not available
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
        Allocator->Info = (struct AxAllocatorInfo) {
            .PageSize = PageSize,
            .AllocationGranularity = AllocatorGranularity,
            .BytesReserved = MaxSize,
            .BytesCommitted = 0,
            .BytesAllocated = 0,
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