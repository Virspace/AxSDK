#pragma once

#include "Foundation/Types.h"

struct AxAPIRegistry;

typedef void axon_load_plugin_f(struct AxAPIRegistry *APIRegistry, bool Load);

#define AXON_PLUGIN_API_NAME "AxonPluginAPI"

// Keeps track of all the shared libraries that have been explicitly loaded at runtime
struct AxPluginAPI
{
    /**
     * Loads a shared library and adds it to the plugin map.
     * @param Path The full path to the shared library.
     * @param HotReload If true, will hot reload when changes are detected (TODO)
     */
    uint64_t (*Load)(const char *Path, bool HotReload);

    /**
     * Unloads a shared library.
     * @param Plugin 
     */
    void (*Unload)(uint64_t Plugin);
};

#if defined(AXON_LINKS_FOUNDATION)
extern struct AxPluginAPI *AxPluginAPI;
#endif