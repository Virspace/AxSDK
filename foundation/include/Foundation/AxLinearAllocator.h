#pragma once

#ifdef __cplusplus
extern  "C" {
#endif

#include "Foundation/Types.h"

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
     * @param MaxSize The maximum size in bytes to be reserved. Because the system must
     * reserve regions in multiples of the page size, the maximum size is always rounded
     * to the nearest system page size.
     * @param BaseAddress If NULL, reserves a region of virtual memory anywhere in the
     * processes address space. Otherwise, specifies the address at which the virtual
     * memory should be reserved. Regions are always reserved on the allocation granularity
     * boundary. If the specified address is not on the boundary, the system will round down
     * to the nearest multiple of the granularity. Due to ASLR the program itself as well as
     * any shared library it uses are loaded at a random address. Therefore it is possible to
     * have an address collision. Because of this, it is RECOMMENDED you use NULL unless
     * you know what you're doing.
     * @return NULL if a free region does exist at that address, or the free
     * region is not large enough to accommodate MaxSize. Otherwise, a handle to the
     * lienar allocator.
     */
    struct AxLinearAllocator *(*Create)(const char *Name, size_t MaxSize, void *BaseAddress);

    /**
     * @param Allocator
     */
    void (*Destroy)(struct AxLinearAllocator *Allocator);

    /**
     * @param Allocator
     * @param Size
     * @param File
     * @param Line
     * @return
     */
    void *(*Alloc)(struct AxLinearAllocator *Allocator, size_t Size, const char *File, uint32_t Line);

    /**
     * @param Allocator
     * @param File
     * @param Line
     * @return
     */
    void (*Free)(struct AxLinearAllocator *Allocator, const char *File, uint32_t Line);
};

#if defined(AXON_LINKS_FOUNDATION)
extern struct AxLinearAllocatorAPI *LinearAllocatorAPI;
#endif

#ifdef __cplusplus
}
#endif