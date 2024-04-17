#pragma once

#include "Foundation/AxTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AXON_API_REGISTRY_NAME "AxonAPIRegistry"

struct AxAPIRegistry
{
    /**
     * Creates a new API entry and returns a handle to it.
     * @param Name The name of the API entry to crate.
     * @return A pointer to a new API entry to be filled out.
    */
    void *(*Create)(const char *Name);

    /**
     * Gets a new API entry or returns NULL.
     * @param Name The name of the API to get.
     * @return A pointer to the API with the specified name, otherwise NULL.
    */
    void *(*Get)(const char *Name);

    /**
     * Overwrites the target API with the specified API.
     * @param Name The name of the API to set.
     * @param API A pointer to the API to be set.
     * @param Size The size of the API to be set.
    */
    void (*Set)(const char *Name, void *API, size_t Size);
};

#if defined(AXON_LINKS_FOUNDATION)

// Extern variable holding the global plugin registry
extern struct AxAPIRegistry *AxonGlobalAPIRegistry;

void AxonInitGlobalAPIRegistry();
void AxonTermGlobalAPIRegistry();
void AxonRegisterAllFoundationAPIs(struct AxAPIRegistry *APIRegistry);

#endif

#ifdef __cplusplus
}
#endif
