#include "Plugin.h"
#include "APIRegistry.h"
#include "Platform.h"
#include "AxHashTable.h"
#include "AxArray.h"
#include "Hash.h"
#include <string.h>
#include <stdio.h>

#define AXARRAY_IMPLEMENTATION

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

static struct AxPlugin **GetPlugins(void)
{
    return (&PluginArray);
}

static bool GetPath(struct AxPlugin *Plugin, char *Buffer, size_t BufferSize)
{
    if (!Plugin) {
        return (false);
    }

    // TODO(mdeforge): I kind of hate how this is done, really need a temp allocator
    // or AxString. Reminder, remove stdio.h and string.h later
    if (strlen(Plugin->Path) > BufferSize) {
        return (false);
    }

    sprintf(Buffer, "%s", Plugin->Path);

    return (true);
}

static void Unload(struct AxPlugin *Plugin)
{
    if (Plugin) {
        AxPlatformAPI->DLL->Unload(Plugin->Handle);
    }
}

struct AxPluginAPI *AxPluginAPI = &(struct AxPluginAPI) {
    .Load = Load,
    .Unload = Unload,
    .GetPlugins = GetPlugins,
    .GetPath = GetPath
};
