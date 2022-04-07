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
struct AxAllocatorStats;

#define AXON_LINEAR_ALLOCATOR_API_NAME "AxonLinearAllocatorAPI"

struct AxLinearAllocatorAPI
{
    /**
     * @param MaxSize
     * @param BaseAddress
     */
    struct AxLinearAllocator *(*Create)(size_t MaxSize, void *BaseAddress);

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

    /**
     * Returns information on reserved and committed bytes and pages
     * @param Allocator
     * @return
     */
    struct AxAllocatorStats *(*Stats)(struct AxLinearAllocator *Allocator);
};

#if defined(AXON_LINKS_FOUNDATION)
extern struct AxLinearAllocatorAPI *AxLinearAllocatorAPI;
#endif