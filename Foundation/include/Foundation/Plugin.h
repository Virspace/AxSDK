#pragma once

#include "Foundation/Types.h"

struct AxAPIRegistry;

typedef void axon_load_plugin_f(struct AxAPIRegistry *APIRegistry, bool Load);

#define AXON_PLUGIN_API_NAME "AxonPluginAPI"

// Keeps track of all the shared libraries that have explicitly loaded at runtime
struct AxPluginAPI
{
    uint64_t (*Load)(const char *Path, bool HotReload);
    void (*Unload)(uint64_t Plugin);
};

#if defined(AXON_LINKS_FOUNDATION)
extern struct AxPluginAPI *AxPluginAPI;
#endif