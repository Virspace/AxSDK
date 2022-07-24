#include "APIRegistry.h"
#include "Plugin.h"
#include "Platform.h"
#include "ImageLoader.h"
#include "AxHashTable.h"
#include "AxAllocatorRegistry.h"
#include "AxLinearAllocator.h"
#include <stdlib.h>
#include <string.h>

struct AxAPIRegistry *AxonGlobalAPIRegistry;

static const uint32_t InitialSize = 16;
static struct AxHashTable *APIMap;

static void *APIMemory;
static int32_t APIMemoryOffset;

// TODO(mdeforge): Keep track of memory allocations via a block allocator

static void Set(const char *Name, void *API, size_t Size)
{
    void* Result = HashTableSearch(APIMap, Name);
    if (Result) {
        memcpy(Result, API, Size);
    } else {
        HashInsert(APIMap, Name, API);
    }
}

static void *Get(const char *Name)
{
    // Try to find it
    void *API = (void *)HashTableSearch(APIMap, Name);
    if (API)
    {
        // If found, return API
        return(API);
    }
    else
    {
        // If not found, create a placeholder that will be filled out
        // once the API gets loaded.
        void* Temp = (char *)APIMemory + APIMemoryOffset;
        HashInsert(APIMap, Name, Temp);
        void *Result = HashTableSearch(APIMap, Name);
        APIMemoryOffset += Kilobytes(8);

        return(Result);
    }
}

#if defined(AXON_LINKS_FOUNDATION)

void AxonInitGlobalAPIRegistry()
{
    APIMap = CreateTable(InitialSize);
    APIMemory = calloc(InitialSize, Kilobytes(8) * 16);
    APIMemoryOffset = 0;
}

void AxonTermGlobalAPIRegistry()
{
    DestroyTable(APIMap);
    free(APIMemory);
}

void AxonRegisterAllFoundationAPIs(struct AxAPIRegistry *APIRegistry)
{
    if (APIRegistry)
    {
        APIRegistry->Set(AXON_PLUGIN_API_NAME, PluginAPI, sizeof(struct AxPluginAPI));
        APIRegistry->Set(AXON_PLATFORM_API_NAME, PlatformAPI, sizeof(struct AxPlatformAPI));
        APIRegistry->Set(AXON_ALLOCATOR_REGISTRY_API_NAME, AllocatorRegistryAPI, sizeof(struct AxAllocatorRegistryAPI));
        APIRegistry->Set(AXON_LINEAR_ALLOCATOR_API_NAME, LinearAllocatorAPI, sizeof(struct AxLinearAllocatorAPI));
    }
}

struct AxAPIRegistry *AxonGlobalAPIRegistry = &(struct AxAPIRegistry) {
    .Set = Set,
    .Get = Get
};

#endif
