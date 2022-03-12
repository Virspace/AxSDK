#include "Plugin.h"
#include "APIRegistry.h"
#include "Platform.h"
#include "AxHashTable.h"
#include "Hash.h"
#include <string.h> // _strdup

#define AXARRAY_IMPLEMENTATION
#include "AxArray.h"

// NOTE(mdeforge): I'm still deciding if I want to get a hash by reading or
// use Win32's FindFirstChangeNotification or ReadDirectoryChanges. One
// advantange of at least FindFirst is that it can watch a whole directory,
// presumably the one where all the plugins would be...
#define DO_HASH 0

struct AxPlugin
{
    char *Path;
    uint64_t Hash;
    AxDLL Handle;
    bool IsHotReloadable;
};

static struct AxPlugin *PluginArray;
static AxHashTable *PluginTable; // <Path, PluginInfo>
static uint64_t HashVal = FNV1_64_INIT;

static struct AxPlugin *Load(const char *Path, bool HotReload)
{
    if (!PluginTable) {
        PluginTable = CreateTable(10);
    }

    struct AxPlatformDLLAPI *DLLAPI = AxPlatformAPI->DLL;
    struct AxPlatformFileAPI *FileAPI = AxPlatformAPI->File;

    AxDLL DLL = DLLAPI->Load(Path);
    if (DLLAPI->IsValid(DLL))
    {
        AxLoadPluginF *LoadPlugin = (AxLoadPluginF *)DLLAPI->Symbol(DLL, "LoadPlugin");
        if (LoadPlugin)
        {
            // Call the plugins LoadPlugin function
            LoadPlugin(AxonGlobalAPIRegistry, false);

            // Read DLL into buffer for hashing
            uint64_t Hash = 0;
            #if (DO_HASH)
            AxFile DLLFile = FileAPI->OpenForRead(Path);
            if (FileAPI->IsValid)
            {
                // Read DLL
                size_t DLLFileSize = FileAPI->Size(DLLFile);
                void *DLLFileBuffer = malloc(DLLFileSize);
                FileAPI->Read(DLLFile, DLLFileBuffer, DLLFileSize);
                FileAPI->Close(DLLFile);

                // Hash the plugin
                Hash = HashBufferFNV1a(DLLFileBuffer, DLLFileSize, HashVal);
                free(DLLFileBuffer);
            }
            #endif

            // Create info
            struct AxPlugin Plugin = {
                .Path = _strdup(Path),
                .Handle = DLL,
                .Hash = Hash,
                .IsHotReloadable = HotReload
            };

            // Add info to array and table
            ArrayPush(PluginArray, Plugin);
            HashInsert(PluginTable, Path, ArrayBack(PluginArray));

            // Update HashVal for next use
            HashVal = Hash;

            return(ArrayBack(PluginArray));
        }
    }

    return (NULL);
}

static struct AxPlugin *GetPlugins(void)
{
    return (PluginArray);
}

static size_t GetNumPlugins(void)
{
    return (ArraySize(PluginArray));
}

static struct AxPlugin *GetPlugin(size_t Index)
{
    struct AxPlugin *Plugin = &PluginArray[Index];

    return ((Plugin) ? Plugin : NULL);
}

static char *GetPath(struct AxPlugin *Plugin)
{
    if (!Plugin) {
        return (NULL);
    }

    return(Plugin->Path);
}

static void Unload(struct AxPlugin *Plugin)
{
    if (Plugin)
    {
        AxPlatformAPI->DLL->Unload(Plugin->Handle);
        free(Plugin->Path);
    }

}

struct AxPluginAPI *AxPluginAPI = &(struct AxPluginAPI) {
    .Load = Load,
    .Unload = Unload,
    .GetPlugins = GetPlugins,
    .GetNumPlugins = GetNumPlugins,
    .GetPlugin = GetPlugin,
    .GetPath = GetPath
};
