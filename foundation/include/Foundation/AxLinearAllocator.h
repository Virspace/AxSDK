#pragma once

#include "Foundation/Types.h"

/**
* A linear allocator is simpliest type of allocator. They work by keeping a pointer
* to the current memory address and moving it forward every allocation. All memory
* is freed at the same time.
*
* [xxxx][xxxx][        ]
* ^           ^
* Start       Offset Pointer
*
* Complexity: O(1)
*/

struct AxLinearAllocator;

struct AxLinearAllocatorStats
{
    uint64_t BytesUsed;
    uint64_t PageCount;
};

struct AxLinearAllocatorAPI
{
    struct AxLinearAllocator *(*Create)(size_t MaxSize, void *BaseAddress);
    void (*Destroy)(struct AxLinearAllocator *Allocator);
    void *(*Alloc)(struct AxLinearAllocator *Allocator, size_t Size, const char *File, uint32_t Line);
    void (*Free)(struct AxLinearAllocator *Allocator);
};

#if defined(AXON_LINKS_FOUNDATION)
extern struct AxLinearAllocatorAPI *AxLinearAllocatorAPI;
#endif