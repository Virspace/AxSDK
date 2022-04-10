#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "Foundation/Types.h"

#define AXON_ALLOCATOR_INFO_REGISTRY_API_NAME "AxonLinearAllocatorAPI"

struct AxAllocatorInfo
{
    char Name[48];          // Allocator Name
    uint16_t PageSize;      // Page size
    size_t BytesCommitted;  // Number of bytes used
    size_t BytesReserved;   // Total bytes allocated
    size_t PagesCommitted;  // Number of pages used
    size_t PagesReserved;   // Total number of pages allocated
};

struct AxAllocatorInfoRegistryAPI
{
    void (*Register)(struct AxAllocatorInfo *AllocatorStats);
    size_t (*Count)(void);
    struct AxAllocatorInfo (*Stats)(size_t Index);
};

#if defined(AXON_LINKS_FOUNDATION)
extern struct AxAllocatorInfoRegistryAPI *AllocatorInfoRegistryAPI;
#endif

#ifdef __cplusplus
}
#endif