#include "Plugin.h"
#include "APIRegistry.h"
#include "Platform.h"
#include "HashTable.h"
#include <cassert>

static const uint8_t InitialSize = 2;
static HashTable *PluginMap;

static bool PluginMapCreated = false;
static uint64_t PluginHandleIndex = 0;

static uint64_t Load(const char *Path, bool HotReload)
{
    assert(AxPlatformAPI && "AxonPlatformAPI is NULL");

    if (!PluginMapCreated)
    {
        PluginMap = CreateTable(InitialSize);
        PluginMapCreated = true;
    }

    struct AxPlatformDLLAPI *DLLAPI = AxPlatformAPI->DLL;
    assert(DLLAPI && "DLLAPI is NULL");

    AxDLL DLL = DLLAPI->Load(Path);
    if (DLLAPI->IsValid(DLL))
    {
        axon_load_plugin_f *LoadPlugin = (axon_load_plugin_f *)DLLAPI->Symbol(DLL, "LoadPlugin");
        if (LoadPlugin)
        {
            LoadPlugin(AxonGlobalAPIRegistry, false);
            PluginHandleIndex++;

            return(PluginHandleIndex);
        }
    }

    return (0);
}

struct AxPluginAPI *AxPluginAPI = &(struct AxPluginAPI) {
    .Load = Load
};
