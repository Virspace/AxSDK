#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "Foundation/Types.h"

#define AXON_ALLOCATOR_REGISTRY_API_NAME "AxonAllocatorRegistryAPI"

struct AxAllocatorInfo;
struct AxAllocatorRegistryAPI
{
    void (*Register)(struct AxAllocatorInfo *Info);
    size_t (*Length)(void);

    struct AxAllocatorInfo *(*GetAllocatorInfoByName)(const char *Name);
    struct AxAllocatorInfo *(*GetAllocatorInfoByIndex)(size_t Index);

    const char *(*Name)(struct AxAllocatorInfo *Info);
    void *(*BaseAddress)(struct AxAllocatorInfo *Info);
    uint32_t (*PageSize)(struct AxAllocatorInfo *Info);
    uint32_t (*AllocationGranularity)(struct AxAllocatorInfo *Info);
    size_t (*BytesReserved)(struct AxAllocatorInfo *Info);
    size_t (*BytesCommitted)(struct AxAllocatorInfo *Info);
    size_t (*BytesAllocated)(struct AxAllocatorInfo *Info);
    size_t (*PagesReserved)(struct AxAllocatorInfo *Info);
    size_t (*PagesCommitted)(struct AxAllocatorInfo *Info);
};

#if defined(AXON_LINKS_FOUNDATION)
extern struct AxAllocatorRegistryAPI *AllocatorRegistryAPI;
#endif

#ifdef __cplusplus
}
#endif