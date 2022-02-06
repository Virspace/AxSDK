#pragma once

#include "Foundation/Types.h"

// An implementation of heap alloc described at https://docs.microsoft.com/en-us/windows/win32/memory/heap-functions

// TODO(mdeforge): It might be a good idea to remove the ability of the AxHeapAPI to track Heaps into something else
// TODO(mdeforge): Consider moving the AxHeapHeader into the implementation. Don't want anything messing with it.

struct AxHeap;

typedef struct AxHeapHeader
{
    char *Name;

    uint64_t BytesUsed;
    uint64_t PageCount;
    uint32_t PageSize;
    uint64_t MaxPages;

    uint8_t TotalReservedPages;  // Number of reserved pages
    uint8_t TotalCommittedPages; // Number of commited pages
    uint8_t SegmentCount;        // Number of segments owned by the heap
    uint8_t SegmentHead;         // Linked-list of segments owned by the heap.
} AxHeapHeader;

#define AXON_HEAP_API_NAME "AxonHeapAPI"

struct AxHeapAPI
{
    /**
     * Creates a memory heap. Reserves space in the virtual address space of the process and
     * allocates physical storage for a specified initial size of the maximum size. The system
     * uses memory from the private heap to store heap support structures, so not all of the
     * specified heap size is available to the process.
     * @param Name The name of the heap
     * @param InitialSize The initial size of the heap.
     * @param MaxSize The maximum size of the heap.
     * @return A pointer to the heap, otherwise NULL.
     */
    struct AxHeap *(*Create)(const char *Name, size_t InitialSize, size_t MaxSize);

    /**
     * Allocates a block of memory from a heap. If the requested size exceeds the current size
     * of committed pages, additional pages are automatically committed from this reserved
     * space, if the physical storage is available.
     * @param Heap The target heap to allocate memory from.
     * @param Size The amount of memory to allocate.
     * @return A pointer to the allocated block, otherwise NULL.
     */
    void *(*Alloc)(struct AxHeap *Heap, size_t Size);

    /**
     * Frees the memory heap.
     * @param Heap The target heap to free. This handle is returned from Create().
     */
    void (*Free)(struct AxHeap *Heap);

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
extern struct AxHeapAPI *AxHeapAPI;
#endif