#include "Plugin.h"
#include "APIRegistry.h"
#include "Platform.h"
#include "AxHashTable.h"
#include "Hash.h"
#include <string.h> // _strdup
#include <stdio.h>

#define AXARRAY_IMPLEMENTATION
#include "AxArray.h"

struct AxPlugin
{
    char *Path;
    AxDLL DLLHandle;
    uint64_t Hash;
    bool IsHotReloadable;
};

static struct AxPlugin *PluginArray;
static AxHashTable *PluginTable; // <Path, PluginInfo>
static uint64_t HashVal = FNV1_64_INIT;

static bool IsValid(uint64_t Handle)
{
    return ((Handle) ? true : false);
}

static struct AxPlugin *FindPlugin(uint64_t Handle)
{
    if (!IsValid(Handle)) {
        return (NULL);
    }

    char HashBuffer[sizeof(uint64_t) + 1];
    memcpy(&HashBuffer, &Handle, sizeof(uint64_t));
    HashBuffer[sizeof(uint64_t)] = '\0';

    return ((struct AxPlugin *)HashTableSearch(PluginTable, HashBuffer));
}

static uint64_t Load(const char *Path, bool HotReload)
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

            // Create info
            struct AxPlugin Plugin = {
                .Path = _strdup(Path),
                .DLLHandle = DLL,
                .Hash = Hash,
                .IsHotReloadable = HotReload
            };

            // NOTE(mdeforge): A uint64_t has a particular bit pattern across 8 bytes
            // Copy the uint64_t hash into a char array
            char HashBuffer[sizeof(uint64_t) + 1];
            memcpy(&HashBuffer, &Hash, sizeof(uint64_t));
            HashBuffer[sizeof(uint64_t)] = '\0';

            // Add info to array and table
            ArrayPush(PluginArray, Plugin);
            HashInsert(PluginTable, HashBuffer, ArrayBack(PluginArray));

            // Update HashVal for next use
            HashVal = Hash;

            return(Hash);
        }
    }

    return (0);
}

static void Unload(uint64_t Handle)
{
    struct AxPlugin *Plugin = FindPlugin(Handle);
    if (Plugin)
    {
        AxPlatformAPI->DLL->Unload(Plugin->DLLHandle);
        free(Plugin->Path);
    }
}

static char *GetPath(uint64_t Handle)
{
    struct AxPlugin *Plugin = FindPlugin(Handle);
    if (Plugin) {
        return (Plugin->Path);
    }

    return(NULL);
}

struct AxPluginAPI *AxPluginAPI = &(struct AxPluginAPI) {
    .Load = Load,
    .Unload = Unload,
    .GetPath = GetPath,
    .IsValid = IsValid
};
