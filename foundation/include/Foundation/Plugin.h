#pragma once

#include "Foundation/Types.h"

struct AxAPIRegistry;

typedef struct AxPlugin AxPlugin;
typedef void AxLoadPluginF(struct AxAPIRegistry *APIRegistry, bool Load);

#define AXON_PLUGIN_API_NAME "AxonPluginAPI"

// Keeps track of all the shared libraries that have been explicitly loaded at runtime
struct AxPluginAPI
{
    /**
     * Loads a shared library and adds it to the plugin map.
     * @param Path The full path to the shared library.
     * @param HotReload If true, will hot reload when changes are detected (TODO)
     */
    AxPlugin *(*Load)(const char *Path, bool HotReload);

    /**
     * Unloads a shared library.
     * @param Plugin
     */
    void (*Unload)(AxPlugin *Plugin);

    /**
     * Returns AxArray of Plugins
     */
    AxPlugin *(*GetPlugins)(void);
};

#if defined(AXON_LINKS_FOUNDATION)
extern struct AxPluginAPI *AxPluginAPI;
#endif