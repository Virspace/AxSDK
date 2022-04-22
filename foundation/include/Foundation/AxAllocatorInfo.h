#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "Foundation/Types.h"

#define AXON_ALLOCATOR_INFO_REGISTRY_API_NAME "AxonAllocatorInfoRegistryAPI"

struct AxAllocatorInfo
{
    char Name[32];                   // Allocator Name
    uint32_t PageSize;               // Page size
    uint32_t AllocationGranularity;  // Granularity
    size_t BytesReserved;            // Number of bytes reserved
    size_t BytesCommitted;           // Number of bytes used
    size_t BytesAllocated;           // Number of bytes allocated
    size_t PagesReserved;            // Total number of pages allocated
    size_t PagesCommitted;           // Number of pages used
};

struct AxAllocatorInfoRegistryAPI
{
    void (*Register)(struct AxAllocatorInfo *AllocatorStats);
    size_t (*Size)(void);
    struct AxAllocatorInfo (*GetInfoByIndex)(size_t Index);
};

#if defined(AXON_LINKS_FOUNDATION)
extern struct AxAllocatorInfoRegistryAPI *AllocatorInfoRegistryAPI;
#endif

#ifdef __cplusplus
}
#endif