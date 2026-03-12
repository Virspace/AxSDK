#pragma once

#include "Foundation/AxTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AXON_API_REGISTRY_NAME "AxonAPIRegistry"

struct AxAPIRegistry
{
    void (*Set)(const char *Name, void *API, size_t Size);
    void *(*Get)(const char *Name);
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
