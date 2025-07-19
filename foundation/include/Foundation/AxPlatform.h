#pragma once

#include "Foundation/AxTypes.h"

// Platform-agnostic error codes
typedef enum AxPlatformError
{
    AX_PLATFORM_SUCCESS = 0,                     // Operation completed successfully
    AX_PLATFORM_ERROR_INVALID_PARAMETER,         // Invalid parameter provided
    AX_PLATFORM_ERROR_PATH_NOT_FOUND,            // Path does not exist
    AX_PLATFORM_ERROR_FILE_NOT_FOUND,            // File does not exist
    AX_PLATFORM_ERROR_ALREADY_EXISTS,            // Path already exists
    AX_PLATFORM_ERROR_ACCESS_DENIED,             // Access denied
    AX_PLATFORM_ERROR_BUSY,                      // Resource is in use
    AX_PLATFORM_ERROR_READONLY,                  // Resource is read-only
    AX_PLATFORM_ERROR_DISK_FULL,                 // Disk is full
    AX_PLATFORM_ERROR_PATH_TOO_LONG,             // Path is too long
    AX_PLATFORM_ERROR_UNKNOWN                    // Unknown error occurred
} AxPlatformError;

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

// Represents a directory handle for iteration
typedef struct AxDirectoryHandle
{
    uint64_t Handle;
} AxDirectoryHandle;

// Represents a directory entry (file or folder)
typedef struct AxDirectoryEntry
{
    char Name[260];  // MAX_PATH on Windows
    bool IsDirectory;
    uint64_t Size;
} AxDirectoryEntry;

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

    // Deletes a file from the filesystem.
    bool (*DeleteFile)(const char *Path, AxPlatformError *ErrorCode);
};

// Interface for Directories
struct AxPlatformDirectoryAPI
{
    bool (*CreateDir)(const char *Path, AxPlatformError *ErrorCode);
    bool (*RemoveDir)(const char *Path, bool Recursive, AxPlatformError *ErrorCode);
    
    // Directory navigation functions
    bool (*ChangeDirectory)(const char *Path);
    char *(*GetCurrentDirectory)(void);
    
    // Directory iteration functions
    AxDirectoryHandle (*OpenDirectory)(const char *Path);
    bool (*ReadDirectoryEntry)(AxDirectoryHandle Handle, AxDirectoryEntry *Entry);
    bool (*CloseDirectory)(AxDirectoryHandle Handle);
    bool (*IsDirectoryHandleValid)(AxDirectoryHandle Handle);
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
