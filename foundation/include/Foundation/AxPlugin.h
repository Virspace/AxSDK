#pragma once

#include "Foundation/AxTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

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
     * @return Returns a non-zero handle to the plugin if successful, otherwise 0.
     */
    uint64_t (*Load)(const char *Path, bool HotReload);

    /**
     * Unloads a shared library.
     * @param Handle 
     */
    void (*Unload)(uint64_t Handle);

    /**
     * Returns a full path to the target plugin.
     * @param Plugin The target plugin.
     * @return A pointer to the path, otherwise NULL.
     */
    char *(*GetPath)(uint64_t Handle);

    /**
     * Checks to see if a plugin is valid or not.
     * @param Plugin The target plugin.
     * @return A handle to the plugin, otherwise 0.
     */
    bool (*IsValid)(uint64_t Handle);
};

#if defined(AXON_LINKS_FOUNDATION)
extern struct AxPluginAPI *PluginAPI;
#endif

#ifdef __cplusplus
}
#endif