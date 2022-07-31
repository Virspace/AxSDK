#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "Foundation/AxTypes.h"

struct AxAllocatorInfo
{
    char Name[64];                   // Allocator Name
    void *BaseAddress;               // Base Address
    uint32_t PageSize;               // Page size
    uint32_t AllocationGranularity;  // Granularity
    size_t BytesReserved;            // Number of bytes reserved
    size_t BytesCommitted;           // Number of bytes used
    size_t BytesAllocated;           // Number of bytes allocated
    size_t PagesReserved;            // Total number of pages allocated
    size_t PagesCommitted;           // Number of pages used
};

#ifdef __cplusplus
}
#endif