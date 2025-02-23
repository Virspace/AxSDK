#pragma once

#include "Foundation/AxTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

// Represents a file handle
typedef struct AxFile
{
    uint64_t Handle;
} AxFile;

// Represents a loaded DLL
typedef struct AxDLL
{
    uint64_t Opaque;
} AxDLL;

#define AXON_PLATFORM_API_NAME "AxonPlatformAPI"

// Interface for paths
struct AxPlatformPathAPI
{
    bool (*FileExists)(const char *Path);
    bool (*DirectoryExists)(const char *Path);
    const char *(*BasePath)(const char *Path);

    //char *(*CurrentWorkingDirectory)(void);
};

// Interface for File I/O
struct AxPlatformFileAPI
{
    // Opens the specified file for read. If opening fails, returns an invalid file handle.
    AxFile (*OpenForRead)(const char *Path);

    // Opens the specifiefd file for write. If opening fails, returns an invalid file handle.
    AxFile (*OpenForWrite)(const char *Path);

    // Sets the read/write offset from the start of the file.
    int64_t (*SetPosition)(AxFile File, int64_t Position);

    // Returns the current size of the file.
    uint64_t (*Size)(AxFile File);

    // Reads size bytes into the buffer from the current file position.
    uint64_t (*Read)(AxFile File, void *Buffer, uint32_t BytesToRead);

    // Writes size bytes to file
    uint64_t (*Write)(AxFile File, void *Buffer, uint32_t BytesToWrite);

    // Checks to see if a file handle is valid.
    bool (*IsValid)(AxFile File);

    // Closes the file.
    void (*Close)(AxFile File);
};

// Interface for Directories
struct AxPlatformDirectoryAPI
{
    bool (*CreateDir)(const char *Path);
    bool (*RemoveDir)(const char *Path);
};

// Interface for loading libraries
struct AxPlatformDLLAPI
{
    // Loads a DLL at the given path.
    AxDLL (*Load)(const char *Path);

    // Unloads a given DLL
    void (*Unload)(AxDLL Handle);

    // Checks to see if DLL is valid
    bool (*IsValid)(AxDLL Handle);

    // Returns a symbol address for the DLL
    void *(*Symbol)(AxDLL Handle, const char *SymbolName);
};

// Interface for time
struct AxTimeAPI
{
    AxWallClock (*WallTime)(void);

    /**
     * Gets the elapsed wall time in seconds.
     * @param Start The starting wall clock time.
     * @param End The ending wall clock time.
     * @return The elapsed wall time in seconds.
     */
    float (*ElapsedWallTime)(AxWallClock Start, AxWallClock End);
};

// TODO(mdeforge): Create a system interface for these types of functions
void GetSysInfo(uint32_t *PageSize, uint32_t *AllocationGranularity);

struct AxPlatformAPI
{
    struct AxPlatformDirectoryAPI *DirectoryAPI;
    struct AxPlatformDLLAPI *DLLAPI;
    struct AxPlatformFileAPI *FileAPI;
    struct AxPlatformPathAPI *PathAPI;
    struct AxTimeAPI *TimeAPI;
};

#if defined(AXON_LINKS_FOUNDATION)
extern struct AxPlatformAPI *PlatformAPI;
#endif

#ifdef __cplusplus
}
#endif
