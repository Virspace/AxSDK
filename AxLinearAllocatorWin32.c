#include "AxLinearAllocator.h"
#include "AxAllocatorRegistry.h"
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

    // TODO(mdeforge): We need to check reserve size before we do this! We might go over!

    // Align allocation to multiple of pointer size
    void *CurrentOffset = (uint8_t *)Allocator->Arena + Allocator->Info.BytesAllocated;
    if (!IsAligned(CurrentOffset))
    {
        size_t NewAddress = RoundUpToPowerOfTwo((size_t)CurrentOffset, sizeof(void *));
        size_t AlignmentSize = NewAddress - (size_t)CurrentOffset;

        // We're just assuming this allocation is going to work out????
        Allocator->Info.BytesAllocated += AlignmentSize;
    }

    // Despite requested amount, actual memory committed will be a multiple of the page size so round to it
    // Don't worry, we'll carve up that allocated page and return what's needed
    size_t BytesRequestedRoundedToPageSize = RoundUpToPowerOfTwo(Size, Allocator->Info.PageSize);

    // TODO(mdeforge): Consider page boundaries! We may need to do this above like we do the padding
    // We don't want to allocate across a page boundary, if we will go over we need to commit a new page
    // then the allocation should move over to the new page.

    // TODO(mdeforge): We should test this allocator against HeapAlloc under the same unit test conditions

    // Check if we have the reserved bytes available to meet the rounded allocation request
        // TODO(mdeforge): Does this need to take into account remaining committed?????
    size_t BytesAvailable = Allocator->Info.BytesReserved - Allocator->Info.BytesAllocated;
    if (BytesRequestedRoundedToPageSize > BytesAvailable) {
        return (NULL);
    }

    // We have the reserved bytes, now calculate bytes left in page
    size_t BytesAllocatedRoundedToPageSize = RoundUpToPowerOfTwo(Allocator->Info.BytesAllocated, Allocator->Info.PageSize);
    int64_t BytesLeftInPage = (int64_t)(BytesAllocatedRoundedToPageSize - Allocator->Info.BytesAllocated - Size);

    // Check to see if we have enough memory committed for the allocation or if we need to commit more
    if (BytesLeftInPage < 0)
    {
        // Calculate bytes needed rounded to nearest multiple of the page size
        size_t BytesNeeded = RoundUpToPowerOfTwo(llabs(BytesLeftInPage), Allocator->Info.PageSize);

        // TODO(mdeforge): Do we need to round up the base address then?
        // Round up to nearest multiple of allocation request (this should be done earlier because it shifts the size!)


        // TODO(mdeforge): Check for virtual alloc failure
        VirtualAlloc((uint8_t *)Allocator->Arena + Allocator->Info.BytesAllocated, BytesNeeded, MEM_COMMIT, PAGE_READWRITE);

        // Does Info reserved memory go down as committed memory goes up?
        Allocator->Info.BytesCommitted += BytesNeeded;
        Allocator->Info.BytesReserved -= BytesNeeded;
    }
    // else
    // {
    //     // TODO(mdeforge): Consider putting BytesAllocated back in the AxLinearAllocator struct so that Info
    //     // can be compiled out during release builds? Or do we always want Info in there?
    // }

    void *Result = (uint8_t *)Allocator->Arena + Allocator->Info.BytesAllocated;

    // Update Arena Info
    //Allocator->Info.BytesAllocated += BytesRequestedRoundedToPageSize;
    Allocator->Info.BytesAllocated += Size;
    Allocator->Info.PagesCommitted = RoundUpToPowerOfTwo(Allocator->Info.BytesCommitted, Allocator->Info.PageSize) / 4096;

    // User payload
    return (Result);
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

static struct AxLinearAllocator *Create(const char *Name, size_t MaxSize)
{
    if (MaxSize == 0) {
        return NULL;
    }

    // Get the system page size and allocator granularity
    uint32_t PageSize, AllocatorGranularity;
    GetSysInfo(&PageSize, &AllocatorGranularity);

    // Memory reserved will be a multiple of the system allocation granularity
    // NOTE(mdeforge): We wouldn't round if VirtualAlloc's BaseAddress parameter was going to be set below
    MaxSize = RoundUpToPowerOfTwo(MaxSize, AllocatorGranularity);

    // Reserve a block of MaxSize in the process's virtual address space for this heap.
    // The maximum size determines the total number of reserved pages.
    // NOTE(mdeforge): Because the application and any shared libraries it uses are loaded at
    //                 a random address (a security feature called ASLR), we pass NULL as the
    //                 BaseAddress parameter in VirtualAlloc.
    void *BaseAddress = VirtualAlloc(BaseAddress, MaxSize, MEM_RESERVE, PAGE_READWRITE);

    // Check for alloc failure
    if (!BaseAddress) {
        return NULL;
    }

    // Use some space at the beginning of the heap to store information
    VirtualAlloc(BaseAddress, sizeof(struct AxLinearAllocator), MEM_COMMIT, PAGE_READWRITE);
    struct AxLinearAllocator *Allocator = BaseAddress;

    // Construct the heap
    if (Allocator)
    {
        Allocator->Arena = (uint8_t *)BaseAddress + sizeof(struct AxLinearAllocator);
        Allocator->Info = (struct AxAllocatorInfo) {
            .BaseAddress = Allocator->Arena,
            .PageSize = PageSize,
            .AllocationGranularity = AllocatorGranularity,
            .BytesReserved = MaxSize,
            .BytesCommitted = 0,
            .BytesAllocated = 0,
            .PagesReserved = MaxSize / (size_t)PageSize,
            .PagesCommitted = 0
        };

        // Copy name
        size_t s = ArrayCount(Allocator->Info.Name);
        strncpy(Allocator->Info.Name, Name, s);

        // Register with Allocator API
        AllocatorRegistryAPI->Register(&Allocator->Info);
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