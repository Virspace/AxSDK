#pragma once

#include "AxAllocationData.h"
#ifdef __cplusplus
extern "C"
{
#endif

#include "Foundation/AxTypes.h"

#define AXON_ALLOCATOR_DATA_API_NAME "AxonAllocatorDataAPI"

struct AxAllocationData;
struct AxAllocatorData;
struct AxAllocatorDataAPI
{
    const char *(*Name)(struct AxAllocatorData *Data);
    void *(*BaseAddress)(struct AxAllocatorData *Data);
    uint32_t (*PageSize)(struct AxAllocatorData *Data);
    uint32_t (*AllocationGranularity)(struct AxAllocatorData *Data);
    size_t (*BytesReserved)(struct AxAllocatorData *Data);
    size_t (*BytesCommitted)(struct AxAllocatorData *Data);
    size_t (*BytesAllocated)(struct AxAllocatorData *Data);
    size_t (*PagesReserved)(struct AxAllocatorData *Data);
    size_t (*PagesCommitted)(struct AxAllocatorData *Data);
    size_t (*NumAllocs)(struct AxAllocatorData *Data);
    bool (*GetAllocationDataByIndex)(struct AxAllocatorData *Data, size_t Index, struct AxAllocationData *AllocData);
};

#if defined(AXON_LINKS_FOUNDATION)
    extern struct AxAllocatorDataAPI *AllocatorDataAPI;
#endif

#ifdef __cplusplus
}
#endif