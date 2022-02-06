#include "AxLinearAllocator.h"

#include <stdio.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define PAGE_LIMIT 1
#define PAGE_SIZE 4096

struct AxLinearAllocator
{
    struct AxLinearAllocatorStats Stats; // Allocator stats
    uint8_t *Offset;                     // Offset pointer, updated on each allocation.
    void *Arena;                         // Start of Heap pointer
};

// Helper Functions ///////////////////////////////////////////
inline size_t Align(size_t N)
{
    return (N + sizeof(intptr_t) - 1) & ~(sizeof(intptr_t) - 1);
}

// Requests (maps) memory from OS
static struct AxLinearAllocator *Create(size_t MaxSize, void *BaseAddress)
{
    if (MaxSize < 0) {
        return NULL;
    }

    // TODO(mdeforge): Let's not incur this penalty every time we call init...
    //SYSTEM_INFO SystemInfo;
    //GetSystemInfo(&SystemInfo);
    //DWORD PageSize = SystemInfo.dwPageSize; // Page size on this computer

    struct AxLinearAllocator *Allocator = malloc(sizeof(struct AxLinearAllocator));
    if (Allocator)
    {
        // Reserve pages in the virtual address space of this process.
        // NOTE(mdeforge): In order to use reserved memory we must later commit it,
        // an operation that instructs the OS to back the virtual memory with RAM.

        // MEM_COMMIT - A commitment from the OS to receive memory when
        // needed, but no sooner.
        Allocator->Arena = VirtualAlloc(BaseAddress, MaxSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Allocator->Offset = Allocator->Arena;
        Allocator->Stats = (struct AxLinearAllocatorStats){ 0 };
    }

    return (Allocator);
}

static void Destroy(struct AxLinearAllocator *Allocator)
{
    if (Allocator)
    {
        VirtualFree(Allocator->Arena, 0, MEM_RELEASE);

        free(Allocator);
        Allocator = NULL;
    }
}

static void *Alloc(struct AxLinearAllocator *Allocator, size_t Size, const char *File, uint32_t Line)
{
    if (Size <= 0) {
        return NULL;
    }

    size_t AdjustedSize = Align(Size);
    //VirtualAlloc(Allocator->Offset, AdjustedSize, MEM_COMMIT, PAGE_READWRITE);

    // Update Arena Info
    Allocator->Offset += AdjustedSize;
    Allocator->Stats.BytesUsed += AdjustedSize;

    // User payload
    return (Allocator->Offset);
}

static void Free(struct AxLinearAllocator *Allocator)
{
    if (Allocator)
    {
        //VirtualFree(Allocator->Arena, 0, MEM_DECOMMIT, PAGE_NOACCESS);
        Allocator->Offset = Allocator->Arena;
        Allocator->Stats = (struct AxLinearAllocatorStats){ 0 };
    }
}

struct AxLinearAllocatorAPI *AxLinearAllocatorAPI = &(struct AxLinearAllocatorAPI) {
    .Create = Create,
    .Destroy = Destroy,
    .Alloc = Alloc,
    .Free = Free
};