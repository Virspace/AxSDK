#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "Foundation/AxTypes.h"

struct AxAllocationData;

struct AxAllocatorData
{
    char Name[64];                            // Allocator Name
    void *BaseAddress;                        // Base Address
    uint32_t PageSize;                        // Page size
    uint32_t AllocationGranularity;           // Granularity
    size_t BytesReserved;                     // Number of bytes reserved in the address space
    size_t BytesCommitted;                    // Number of bytes mapped to physical memory
    size_t BytesAllocated;                    // Number of bytes actually allocated
    size_t PagesReserved;                     // Total number of pages allocated
    size_t PagesCommitted;                    // Number of pages used
    size_t NumAllocs;                         // Number of allocations
    struct AxAllocationData *AllocationData;  // Info on each of the allocations
};

#ifdef __cplusplus
}
#endif