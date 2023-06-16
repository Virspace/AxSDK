#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "Foundation/AxTypes.h"

#define AXON_ALLOCATOR_REGISTRY_API_NAME "AxonAllocatorInfoAPI"

    struct AxAllocatorData;
    struct AxAllocatorInfoAPI
    {
        const char *(*Name)(struct AxAllocatorData *Info);
        void *(*BaseAddress)(struct AxAllocatorData *Info);
        uint32_t (*PageSize)(struct AxAllocatorData *Info);
        uint32_t (*AllocationGranularity)(struct AxAllocatorData *Info);
        size_t (*BytesReserved)(struct AxAllocatorData *Info);
        size_t (*BytesCommitted)(struct AxAllocatorData *Info);
        size_t (*BytesAllocated)(struct AxAllocatorData *Info);
        size_t (*PagesReserved)(struct AxAllocatorData *Info);
        size_t (*PagesCommitted)(struct AxAllocatorData *Info);
        size_t (*NumAllocs)(struct AxAllocatorData *Info);
    };

#if defined(AXON_LINKS_FOUNDATION)
    extern struct AxAllocatorInfoAPI *AllocatorInfoAPI;
#endif

#ifdef __cplusplus
}
#endif