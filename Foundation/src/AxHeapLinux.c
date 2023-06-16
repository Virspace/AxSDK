#include <unistd.h>

#define __USE_MISC // for MAP_ANONYMOUS
#include <sys/mman.h>
#include <string.h>
#include "AxHeap.h"
#include "AxHashTable.h"

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
    AxHeapHeader Header;
    void *Arena;
};

static struct AxHashTable *HeapTable;

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
    int PageSize = sysconf(_SC_PAGESIZE);

    return ((uint32_t)PageSize);
}

static struct AxHeap *Create(const char *Name, size_t InitialSize, size_t MaxSize)
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
    //void *BaseAddress = VirtualAlloc(0, MaxSize, MEM_RESERVE, PAGE_NOACCESS);
    void* BaseAddress = mmap(0, MaxSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);


    // The initial size determines the number of committed pages that are allocated initially for the heap.
    //VirtualAlloc(BaseAddress, InitialSize, MEM_COMMIT, PAGE_READWRITE);
    mprotect(BaseAddress, InitialSize, PROT_READ | PROT_WRITE);


    // Construct the heap
    struct AxHeap *Heap = BaseAddress;
    if (Heap)
    {
        Heap->Header.Name = strdup(Name);
        Heap->Header.BytesUsed = 0; // TODO(mdeforge): Consider ByteOffset instead
        Heap->Header.PageSize = PageSize;
        Heap->Header.PagesUsed = InitialSize / PageSize;
        Heap->Header.MaxPages = MaxSize / PageSize;
        Heap->Arena = (uint8_t *)BaseAddress + sizeof(struct AxHeapHeader);
    }

    // Lazy-initialize heap table
    if (!HeapTable) {
        HeapTable = HashTableAPI->CreateTable();
    }

    // Add heap to heap table
    HashTableAPI->Set(HeapTable, Name, Heap);

    return (Heap);
}

static void Destroy(struct AxHeap *Heap)
{

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

    uint64_t HeapSize = Heap->Header.PagesUsed * Heap->Header.PageSize;
    size_t RequestedSize = Heap->Header.BytesUsed + Size;
    if (HeapSize >= RequestedSize)
    {
        Heap->Header.BytesUsed += Size; // Needs atomic
        Result = (uint8_t *)Heap->Arena + Heap->Header.BytesUsed;
    }
    else
    {
        uint64_t PagesNeeded = RoundValueToNearestMultiple(HeapSize + Size, Heap->Header.PageSize);
        uint64_t NewPages = PagesNeeded - Heap->Header.PagesUsed;

        // Check if can commit another N page from the pages we already reserved
        // If we can, commit them, if we can't try and reserve them and grow if needed, if we can't then return null

        //VirtualAlloc((uint8_t *)Heap->Arena + HeapSize, NewPages * Heap->Header.PageSize, MEM_COMMIT, PAGE_READWRITE);
        //mmap(0, MaxSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        mmap((uint8_t *)Heap->Arena + HeapSize, NewPages * Heap->Header.PageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Heap->Header.PagesUsed += NewPages; // Needs atomic

        Heap->Header.BytesUsed += Size; // Needs atomic
        Result = (uint8_t *)Heap->Arena + Heap->Header.BytesUsed;
    }

    return (Result);
}

// TODO(mdeforge): Shit, Linux actually needs to know the size of the block we're freeing
static void Free(void *Block, size_t Size)
{
    //VirtualFree(Block, 0, MEM_DECOMMIT);
    mprotect(Block, Size, PROT_NONE);
}

static size_t GetNumHeaps(void)
{
    Assert(HeapTable);
    if (!HeapTable) {
        return (0);
    }

    return (HashTableAPI->Size(HeapTable));
}

static struct AxHeap *GetHeap(size_t Index)
{
    Assert(HeapTable);
    if (!HeapTable) {
        return (NULL);
    }

    return (HashTableAPI->GetValueAtIndex(HeapTable, Index));
}

static const struct AxHeapHeader *GetHeapHeader(struct AxHeap *Heap)
{
    Assert(Heap);

    return ((Heap) ? &Heap->Header : NULL);
}

struct AxHeapAPI *HeapAPI = &(struct AxHeapAPI) {
    .Create = Create,
    .Destroy = Destroy,
    .Alloc = Alloc,
    .Free = Free,
    .GetNumHeaps = GetNumHeaps,
    .GetHeap = GetHeap,
    .GetHeapHeader = GetHeapHeader
};