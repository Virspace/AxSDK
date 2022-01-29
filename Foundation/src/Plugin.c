#include "Plugin.h"
#include "APIRegistry.h"
#include "Platform.h"
#include "AxHashTable.h"
#include <string.h>

static const uint8_t InitialSize = 2;
static AxHashTable *PluginTable; // <ID, Path>

static bool PluginMapCreated = false;
static int PluginIndex = 0;

static uint64_t Load(const char *Path, bool HotReload)
{
    if (!PluginMapCreated)
    {
        PluginTable = CreateTable(InitialSize);
        PluginMapCreated = true;
    }

    struct AxPlatformDLLAPI *DLLAPI = AxPlatformAPI->DLL;

    AxDLL DLL = DLLAPI->Load(Path);
    if (DLLAPI->IsValid(DLL))
    {
        axon_load_plugin_f *LoadPlugin = (axon_load_plugin_f *)DLLAPI->Symbol(DLL, "LoadPlugin");
        if (LoadPlugin)
        {
            LoadPlugin(AxonGlobalAPIRegistry, false);
            HashInsert(PluginTable, Path, (void *)DLL.Opaque);

            return(DLL.Opaque);
        }
    }

    return (0);
}

static void Unload(uint64_t Plugin)
{
    AxPlatformAPI->DLL->Unload((AxDLL){.Opaque = Plugin});
}

struct AxPluginAPI *AxPluginAPI = &(struct AxPluginAPI) {
    .Load = Load,
    .Unload = Unload
};
