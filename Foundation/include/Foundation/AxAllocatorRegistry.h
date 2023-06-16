#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "Foundation/AxTypes.h"

#define AXON_ALLOCATOR_REGISTRY_API_NAME "AxonAllocatorRegistryAPI"

    struct AxAllocatorData;
    struct AxAllocatorRegistryAPI
    {
        void (*Register)(struct AxAllocatorData *Info);
        size_t (*Length)(void);
        struct AxAllocatorData *(*GetAllocatorDataByName)(const char *Name);
        struct AxAllocatorData *(*GetAllocatorDataByIndex)(size_t Index);

    };

#if defined(AXON_LINKS_FOUNDATION)
    extern struct AxAllocatorRegistryAPI *AllocatorRegistryAPI;
#endif

#ifdef __cplusplus
}
#endif