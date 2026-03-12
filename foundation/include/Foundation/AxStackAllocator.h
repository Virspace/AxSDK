#pragma once

#include "Foundation/AxTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
* Like the linear allocator, a stack allocator works by keeping a pointer to the
* current memory address and moving it forward every allocation. However, unlike
* the linear allocator it can also move the pointer backwards when memory is freed.
* Freeing follows the LIFO principle where the last block to be allocated is the
* first to be freed.
*
* [xxxx][xxxx][        ]
* ^           ^
* Start       Offset Pointer
*
* Complexity: O(N*H) where H is header size and N is the number of allocations
*/

struct AxStackAllocatorAPI;

#define AXON_LINEAR_ALLOCATOR_API_NAME "AxonStackAllocatorAPI"

struct AxStackAllocatorAPI
{
    struct AxStackAllocator *(*Create)(size_t MaxSize, void *BaseAddress);

    void (*Destroy)(struct AxStackAllocator *Allocator);

    void *(*Alloc)(struct AxStackAllocator *Allocator, size_t Size, size_t Alignment, const char *File, uint32_t Line);

    void (*Free)(struct AxStackAllocator *Allocator, const char *File, uint32_t Line);

    void (*Reset)(struct AxStackAllocator *Allocator);
};

#ifdef __cplusplus
}
#endif