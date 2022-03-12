#pragma once

#include "Foundation/Types.h"

struct AxPlugin;
struct AxAPIRegistry;

typedef void AxLoadPluginF(struct AxAPIRegistry *APIRegistry, bool Load);

#define AXON_PLUGIN_API_NAME "AxonPluginAPI"

// Keeps track of all the shared libraries that have been explicitly loaded at runtime
struct AxPluginAPI
{
    /**
     * Loads a shared library and adds it to the plugin map.
     * @param Path The full path to the shared library.
     * @param HotReload If true, will hot reload when changes are detected (TODO).
     * @return Returns an AxPlugin object if successful, otherwise NULL.
     */
    struct AxPlugin *(*Load)(const char *Path, bool HotReload);

    /**
     * Unloads a shared library.
     * @param Plugin
     */
    void (*Unload)(struct AxPlugin *Plugin);

    /**
     * Returns pointer to an AxArray (pointer) of Plugins.
     */
    struct AxPlugin *(*GetPlugins)(void);

    /**
     * Returns number of plugins.
     * @return The number of plugins.
     */
    size_t (*GetNumPlugins)(void);

    /**
     * Returns a pointer to an AxPlugin given an index
     * @return Returns an AxPlugin if given a valid index, otherwise NULL.
     */
    struct AxPlugin *(*GetPlugin)(size_t Index);

    /**
     * Returns a full path to the target plugin.
     * @param Plugin The target plugin.
     * @return A pointer to the path, otherwise NULL.
     */
    char *(*GetPath)(struct AxPlugin *Plugin);
};

#if defined(AXON_LINKS_FOUNDATION)
extern struct AxPluginAPI *AxPluginAPI;
#endif