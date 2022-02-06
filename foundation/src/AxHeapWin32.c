#include "AxHeap.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define PAGE_LIMIT 1
#define PAGE_SIZE 4096

// TODO(mdeforge): Make thread safe

// Describes the status of each page in a segment
typedef struct PageRangeDesc
{
    short UnusedBytes;
    unsigned char Align;
    unsigned char Offset; // Non-first: Offset from the first page range descriptor
    unsigned char Size;   // First: Number of pages of the backend block
} PageRangeDesc;

struct PageSegment
{
    PageRangeDesc PageRanges[256];
};

struct Bucket
{
    uint8_t TotalBlockCount; // Total number of blocks in all subsegments related to the bucket
};

struct AxHeap
{
    uint8_t *Heap;
    AxHeapStats Stats;
};

static size_t RoundValueToNearestMultiple(size_t Value, size_t Multiple)
{
    size_t Remainder = Value % Multiple;
    if (Remainder == 0) {
        return Value;
    }

    return (Value + Multiple - Remainder);
}

static uint32_t GetSystemPageSize()
{
    SYSTEM_INFO SystemInfo;
    GetSystemInfo(&SystemInfo);

    return ((uint32_t)SystemInfo.dwPageSize);
}

static struct AxHeap *Create(size_t InitialSize, size_t MaxSize)
{
    // Get system page size on this computer.
    uint32_t PageSize = GetSystemPageSize(); 

    // Round InitialSize up to the nearest multiple of the system page size.
    InitialSize = RoundValueToNearestMultiple(InitialSize, PageSize);

    // If MaxSize is not zero, the heap size is fixed and cannot grow beyond the maximum size
    // and should be rounded up to the nearest multiple of the system page size as well.
    if (MaxSize != 0) {
        MaxSize = RoundValueToNearestMultiple(MaxSize, PageSize);
    }

    // The InitialSize must be smaller than MaxSize.
    if (InitialSize > MaxSize) {
        return (NULL);
    }

    // Reserve a block of MaxSize in the process's virtual address space for this heap.
    // The maximum size determines the total number of reserved pages.
    void *BaseAddress = VirtualAlloc(0, MaxSize, MEM_RESERVE, PAGE_NOACCESS);

    // The initial size determines the number of committed pages that are allocated initially for the heap.
    VirtualAlloc(BaseAddress, InitialSize, MEM_COMMIT, PAGE_READWRITE);

    // TODO(mdeforge): So here we're taking up some of the initial memory, we need to factor this into the sizes above
    struct AxHeap *Heap = BaseAddress;
    if (Heap)
    {
        Heap->Heap = (uint8_t *)BaseAddress + sizeof(struct AxHeap);
        Heap->Stats.BytesUsed = 0; // TODO(mdeforge): Consider ByteOffset instead
        Heap->Stats.PageSize = PageSize;
        Heap->Stats.PageCount = InitialSize / PageSize;
        Heap->Stats.MaxPages = MaxSize / PageSize;
    }

    return (Heap);
}

// TODO(mdeforge): I don't think Alloc is an appropriate function to add to the heap allocator.
// There is too much info needed on how alloc's and free's should happen, which should be
// implemented by the higher level. All this should do is manage the pages.
static void *Alloc(struct AxHeap *Heap, size_t Size)
{
    // TODO(mdeforge): If MaxSize is zero, grow beyond maximum size. Requests to allocate memory
    // blocks larger than the limit for a fixed-size heap do not automatically fail; instead, 
    // the system calls the VirtualAlloc function to obtain the memory that is needed for large blocks

    void *Result = NULL;

    uint64_t HeapSize = Heap->Stats.PageCount * Heap->Stats.PageSize;
    size_t RequestedSize = Heap->Stats.BytesUsed + Size;
    if (HeapSize >= RequestedSize)
    {
        Heap->Stats.BytesUsed += Size; // Needs atomic
        Result = Heap->Heap + Heap->Stats.BytesUsed;
    }
    else
    {
        uint64_t PagesNeeded = RoundValueToNearestMultiple(HeapSize + Size, Heap->Stats.PageSize);
        uint64_t NewPages = PagesNeeded - Heap->Stats.PageCount;

        // Check if can commit another N page from the pages we already reserved
        // If we can, commit them, if we can't try and reserve them and grow if needed, if we can't then return null

        VirtualAlloc(Heap->Heap + HeapSize, NewPages * Heap->Stats.PageSize, MEM_COMMIT, PAGE_READWRITE);
        Heap->Stats.PageCount += NewPages; // Needs atomic

        Heap->Stats.BytesUsed += Size; // Needs atomic
        Result = Heap->Heap + Heap->Stats.BytesUsed;
    }

    return (Result);
}

static void Free(struct AxHeap *Heap, void *Ptr, size_t Size)
{

}

struct AxHeapAPI *AxHeapAPI = &(struct AxHeapAPI) {
    .Create = Create,
    .Alloc = Alloc,
    .Free = Free
};