#pragma once

#include "Foundation/AxTypes.h"

#ifdef __cplusplus
extern  "C" {
#endif

/**
* A linear allocator is simpliest type of allocator. It works by keeping a pointer
* to the current memory address and moving it forward every allocation. All memory
* is freed at the same time.
*
* [xxxx][xxxx][        ]
* ^           ^
* Start       Offset Pointer
*
* Complexity: O(1)
*/

#define AXON_LINEAR_ALLOCATOR_API_NAME "AxonLinearAllocatorAPI"

typedef struct AxLinearAllocator AxLinearAllocator;

struct AxLinearAllocatorAPI
{
    /**
     * @param Name The name of the linear allocator.
     * @param MaxSize The maximum size in bytes to be reserved. Because the system must
     * reserve regions in multiples of the page size, the maximum size is always rounded
     * to the nearest system page size.
     * @return NULL if a free region does exist at that address, or the free
     * region is not large enough to accommodate MaxSize. Otherwise, a handle to the
     * lienar allocator.
     */
    struct AxLinearAllocator *(*Create)(const char *Name, size_t MaxSize);

    /**
     * @param Allocator
     */
    void (*Destroy)(struct AxLinearAllocator *Allocator);

    /**
     * Returns an aligned allocation.
     * @param Allocator The target linear allocator.
     * @param Size The size in bytes to be allocated.
     * @param File The file making the allocation, e.g. __FILE__.
     * @param Line The line making the allocation, e.g. __LINE__.
     * @return An aligned pointer to the allocated memory.
     */
    void *(*Alloc)(struct AxLinearAllocator *Allocator, size_t Size, const char *File, uint32_t Line);

    /**
     * @param Allocator The target linear allocator.
     * @param File The file making the allocation, e.g. __FILE__.
     * @param Line The line making the allocation, e.g. __LINE__.
     */
    void (*Free)(struct AxLinearAllocator *Allocator, const char *File, uint32_t Line);
};

#if defined(AXON_LINKS_FOUNDATION)
extern struct AxLinearAllocatorAPI *LinearAllocatorAPI;
#endif

#ifdef __cplusplus
}
#endif