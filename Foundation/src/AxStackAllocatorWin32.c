#include "AxStackAllocator.h"
#include "AxAllocatorRegistry.h"
#include "AxAllocatorInfo.h"
#include "AxAllocUtils.h"

#include <stdio.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// TODO(mdeforge): If we add platform memory allocation functions to the platform api
//                 we can have one linear allocator source file that's not Win32 specific.

struct AxStackAllocator
{
    struct AxAllocatorInfo Info;
    size_t ByteOffset;            // Offset pointer, updated on each allocation
    void *Arena;                  // Start of the heap pointer
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

static struct AxStackAllocator *Create(size_t MaxSize, void *BaseAddress)
{
    if (MaxSize == 0) {
        return NULL;
    }

    // Get the system page size
    uint32_t PageSize, AllocatorGranularity;
    GetSysInfo(&PageSize, &AllocatorGranularity);

    // Reserve a block of MaxSize in the process's virtual address space for this heap.
    // The maximum size determines the total number of reserved pages.
    BaseAddress = VirtualAlloc(BaseAddress, MaxSize, MEM_RESERVE, PAGE_READWRITE);

    // Use some space at the beginning of the heap to store information
    VirtualAlloc(BaseAddress, sizeof(struct AxStackAllocator), MEM_COMMIT, PAGE_READWRITE);
    struct AxStackAllocator *Allocator = BaseAddress;

    // Construct the heap
    if (Allocator)
    {
        Allocator->Arena = (uint8_t *)BaseAddress + sizeof(struct AxStackAllocator);
        Allocator->ByteOffset = 0;
        Allocator->Info = (struct AxAllocatorInfo) {
            .PageSize = PageSize,
            .AllocationGranularity = AllocatorGranularity,
            .BytesCommitted = 0,
            .BytesReserved = MaxSize,
            .PagesCommitted = 0,
            .PagesReserved = MaxSize / (size_t)PageSize,
        };

        // Register with Allocator API
        AllocatorRegistryAPI->Register(&Allocator->Info);
    }

    return (Allocator);
}

static void Destroy(struct AxStackAllocator *Allocator)
{
    Assert(Allocator && "AxStackAllocator passed NULL allocator!");
    if (!Allocator) {
        return;
    }

    VirtualFree(Allocator, 0, MEM_RELEASE); // Will decommit and release
}

static void *Alloc(struct AxStackAllocator *Allocator, size_t Size, size_t Alignment, const char *File, uint32_t Line)
{
    const uint8_t *currentAddress = (uint8_t *)Allocator->Arena + Allocator->ByteOffset;
}

static void Free(struct AxStackAllocator *Allocator, const char *File, uint32_t Line)
{

}

static void Reset(struct AxStackAllocator *Allocator)
{

}