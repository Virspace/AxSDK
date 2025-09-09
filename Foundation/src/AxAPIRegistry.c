#include "AxAPIRegistry.h"
#include "AxPlugin.h"
#include "AxPlatform.h"
#include "AxHashTable.h"
#include "AxAllocatorRegistry.h"
#include "AxLinearAllocator.h"
#include "AxAllocatorInfo.h"
#include "AxCamera.h"
#include <stdlib.h>
#include <string.h>

struct AxAPIRegistry *AxonGlobalAPIRegistry;

static const uint32_t INITIAL_SIZE = 16;
static struct AxHashTable *APIMap;

static void *APIMemory;
static int32_t APIMemoryOffset;

// TODO(mdeforge): Keep track of memory allocations via a block allocator

static void Set(const char *Name, void *API, size_t Size)
{
    void* Result = HashTableAPI->Find(APIMap, Name);
    if (Result) {
        memcpy(Result, API, Size);
    } else {
        HashTableAPI->Set(APIMap, Name, API);
    }
}

static void *Get(const char *Name)
{
    // Try to find it
    void *API = (void *)HashTableAPI->Find(APIMap, Name);
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
        HashTableAPI->Set(APIMap, Name, Temp);
        void *Result = HashTableAPI->Find(APIMap, Name);
        APIMemoryOffset += Kilobytes(8);

        return(Result);
    }
}

#if defined(AXON_LINKS_FOUNDATION)

void AxonInitGlobalAPIRegistry()
{
    APIMap = HashTableAPI->CreateTable();
    APIMemory = calloc(INITIAL_SIZE, Kilobytes(8) * 16);
    APIMemoryOffset = 0;
}

void AxonTermGlobalAPIRegistry()
{
    HashTableAPI->DestroyTable(APIMap);
    free(APIMemory);
}

void AxonRegisterAllFoundationAPIs(struct AxAPIRegistry *APIRegistry)
{
    if (APIRegistry)
    {
        APIRegistry->Set(AXON_PLUGIN_API_NAME, PluginAPI, sizeof(struct AxPluginAPI));
        APIRegistry->Set(AXON_PLATFORM_API_NAME, PlatformAPI, sizeof(struct AxPlatformAPI));
        APIRegistry->Set(AXON_ALLOCATOR_REGISTRY_API_NAME, AllocatorRegistryAPI, sizeof(struct AxAllocatorRegistryAPI));
        APIRegistry->Set(AXON_ALLOCATOR_DATA_API_NAME, AllocatorDataAPI, sizeof(struct AxAllocatorDataAPI));
        APIRegistry->Set(AXON_LINEAR_ALLOCATOR_API_NAME, LinearAllocatorAPI, sizeof(struct AxLinearAllocatorAPI));
        APIRegistry->Set(AXON_HASH_TABLE_API_NAME, HashTableAPI, sizeof(struct AxHashTableAPI));
        APIRegistry->Set(AXON_CAMERA_API_NAME, CameraAPI, sizeof(struct AxCameraAPI));

    }
}

struct AxAPIRegistry *AxonGlobalAPIRegistry = &(struct AxAPIRegistry) {
    .Set = Set,
    .Get = Get
};

#endif
