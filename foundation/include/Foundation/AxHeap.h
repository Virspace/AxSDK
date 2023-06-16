#pragma once

#include "Foundation/AxTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

// An implementation of heap alloc described at https://docs.microsoft.com/en-us/windows/win32/memory/heap-functions

// TODO(mdeforge): It might be a good idea to remove the ability of the AxHeapAPI to track Heaps into something else
// TODO(mdeforge): Consider moving the AxHeapHeader into the implementation. Don't want anything messing with it.

struct AxHeap;

typedef struct AxHeapHeader
{
    char *Name;

    size_t BytesUsed;
    size_t PageSize;
    size_t PagesUsed;
    size_t MaxPages;
} AxHeapHeader;

#define AXON_HEAP_API_NAME "AxonHeapAPI"

struct AxHeapAPI
{
    /**
     * Creates a memory heap. Reserves space in the virtual address space of the process and
     * allocates physical storage for a specified initial size of the maximum size. To create
     * a growable heap, pass a MaxSize of zero. A non-zero MaxSize will result in a non-growable
     * heap. The system uses memory from the private heap to store heap support structures, so
     * not all of the specified heap size is available to the process.
     * @param Name The name of the heap
     * @param InitialSize The initial size of the heap.
     * @param MaxSize The maximum size of the heap.
     * @param BaseAddress The starting address of the region to allocate. If the memory is
     * being reserved, the specified address is rounded down to the nearest multiple of the
     * allocation granularity. If the memory is already reserved and is being committed, the
     * address is rounded down to the next page boundary.
     * @return A pointer to the heap, otherwise NULL.
     */
    struct AxHeap *(*Create)(const char *Name, size_t InitialSize, size_t MaxSize);

    /**
     * Decommits and releases all pages in the heap.
     * @param Heap The target heap to destroy.
     */
    void (*Destroy)(struct AxHeap *Heap);

    /**
     * Allocates a block of memory from a heap. If the requested size exceeds the current size
     * of committed pages, additional pages are automatically committed from this reserved
     * space, if the physical storage is available.
     * @param Heap The target heap to allocate memory from.
     * @param Size The number of bytes to be allocated.
     * @return A pointer to the allocated block, otherwise NULL.
     */
    void *(*Alloc)(struct AxHeap *Heap, size_t Size);

    /**
     * Frees a memory block allocated by Alloc.
     * @param Block The target memory block to free.
     * @param Size The size of the block to be freed.
     */
    void (*Free)(void *Block, size_t Size);

    /**
     * Returns number of AxHeaps managed by the API.
     * @return The number of plugins.
     */
    size_t (*GetNumHeaps)(void);

    /**
     * Returns a pointer to an AxPlugin given an index
     * @return Returns an AxPlugin if given a valid index, otherwise NULL.
     */
    struct AxHeap *(*GetHeap)(size_t Index);

    /**
     * Returns the heap header
     * @param Heap The target heap.
     * @return A pointer to the AxHeapHeader.
     */
    const struct AxHeapHeader *(*GetHeapHeader)(struct AxHeap *Heap);
};

#if defined(AXON_LINKS_FOUNDATION)
extern struct AxHeapAPI *HeapAPI;
#endif

#ifdef __cplusplus
}
#endif