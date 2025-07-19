#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <ShObjIdl.h>
#include <shellapi.h>
#include "AxPlatform.h"

// Forward declarations
static AxPlatformError MapWin32ErrorToAxPlatformError(DWORD Win32Error);
static bool RemoveDirRecursive(const char *Path, AxPlatformError *ErrorCode);

// TODO(mdeforge): Need to read large files better

/* ========================================================================
   Axon Path
   ======================================================================== */

static bool FileExists(const char *Path)
{
    AXON_ASSERT(Path);

    uint32_t result = GetFileAttributes(Path);
    if (result != 0xFFFFFFFF && !(result & FILE_ATTRIBUTE_DIRECTORY)) {
        return (true);
    }

    return (false);
}

static bool DirectoryExists(const char *Path)
{
    AXON_ASSERT(Path);

    uint32_t result = GetFileAttributesA(Path);
    if (result != 0xFFFFFFFF && (result & FILE_ATTRIBUTE_DIRECTORY)) {
        return true;
    }

    // return exists;
    return false;
}

static const char *BasePath(const char *Path)
{
    AXON_ASSERT(Path);

    // Find the last occurrence of both '/' and '\\'
    const char *LastSlash = strrchr(Path, '/');
    const char *LastBackslash = strrchr(Path, '\\');

    // Use the latter of the two separators
    const char *lastSeparator = (LastSlash > LastBackslash) ? LastSlash : LastBackslash;

    if (lastSeparator == NULL) {
        // No directory separator found, return the full path
        return Path;
    }

    // Calculate the length of the base path
    size_t BaseLength = lastSeparator - Path + 1;

    // Create a static buffer to store the base path
    static char BasePath[MAX_PATH];

    // Copy the base path
    strncpy(BasePath, Path, BaseLength);
    BasePath[BaseLength] = '\0';

    return BasePath;
}

static char *CurrentWorkingDirectory(void)
{
    //return (GetCurrentDirectoryA(MAX_PATH, CurrentDirectory));
    return NULL;
}


/* ========================================================================
   Axon File
   ======================================================================== */

static bool FileIsValid(AxFile File)
{
    return(File.Handle != 0);
}

static AxFile FileOpenForRead(const char *Path)
{
    AxFile File = { 0 };

    uint32_t Access = GENERIC_READ;
    uint32_t WinFlags = FILE_SHARE_READ;
    uint32_t Create = OPEN_EXISTING;

    HANDLE Handle = CreateFileA(Path, Access, WinFlags, NULL, Create, FILE_ATTRIBUTE_NORMAL, NULL);
    if (Handle != INVALID_HANDLE_VALUE) {
        File.Handle = (uint64_t)Handle;
    }

    return(File);
}

static AxFile FileOpenForWrite(const char *Path)
{
    AXON_ASSERT(Path);

    AxFile File = { 0 };

    uint32_t Access = GENERIC_WRITE;
    uint32_t WinFlags = 0;
    uint32_t Create = CREATE_ALWAYS;

    HANDLE Handle = CreateFileA(Path, Access, WinFlags, NULL, Create, FILE_ATTRIBUTE_NORMAL, NULL);
    if (Handle != INVALID_HANDLE_VALUE) {
        File.Handle = (uint64_t)Handle;
    }

    return(File);
}

static int64_t FileSetPosition(AxFile File, int64_t Position)
{
    AXON_ASSERT(FileIsValid(File));
    AXON_ASSERT(Position >= 0);

    DWORD MoveMethod = FILE_BEGIN;

    LARGE_INTEGER LargeInt;
    LargeInt.QuadPart = Position;
    LargeInt.LowPart = SetFilePointer((HANDLE)File.Handle, LargeInt.LowPart, &LargeInt.HighPart, MoveMethod);

    if (LargeInt.LowPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
    {
        LargeInt.QuadPart = -1;
    }

    return(LargeInt.QuadPart);
}

static uint64_t FileSize(AxFile File)
{
    AXON_ASSERT(FileIsValid(File));

    LARGE_INTEGER LargeInt;
    GetFileSizeEx((HANDLE)File.Handle, &LargeInt);

    return(LargeInt.QuadPart);
}

static uint64_t FileRead(AxFile File, void *Buffer, uint32_t BytesToRead)
{
    AXON_ASSERT(FileIsValid(File));
    AXON_ASSERT(Buffer != NULL);

    //axon_platform_file_io &file_io = AxPlatformAPI->file_io;

    // Return if no bytes are to be read.
    if (BytesToRead <= 0)
    {
        return (0);
    }

    // If file size is smaller than requested read, read the number 
    // of bytes that are available and return the number of read bytes.
    uint64_t Size = FileSize(File);
    if (Size < BytesToRead)
    {
        BytesToRead = (uint32_t)Size;
    }

    // TODO(mdeforge): If EOF, return 0

    DWORD NumBytesRead = 0;
    if (!ReadFile((HANDLE)File.Handle, Buffer, (DWORD)BytesToRead, &NumBytesRead, NULL)) {
        return(0);
    }

    return((uint64_t)NumBytesRead);
}

static uint64_t FileWrite(AxFile File, void* Buffer, uint32_t BytesToWrite)
{
    AXON_ASSERT(FileIsValid(File));
    AXON_ASSERT(Buffer != NULL);

    DWORD NumBytesWritten = 0;
    if (!WriteFile((HANDLE)File.Handle, Buffer, BytesToWrite, &NumBytesWritten, NULL)) {
        return(0);
    }

    return((uint64_t)NumBytesWritten);
}

static void FileClose(AxFile File)
{
    AXON_ASSERT(FileIsValid(File));
    CloseHandle((HANDLE)File.Handle);
}

static bool FileDelete(const char *Path, AxPlatformError *ErrorCode)
{
    AXON_ASSERT(Path);
    if (ErrorCode) {
        *ErrorCode = AX_PLATFORM_SUCCESS; // Initialize to success
    }
    if (!Path || strlen(Path) == 0) {
        if (ErrorCode) {
            *ErrorCode = AX_PLATFORM_ERROR_INVALID_PARAMETER;
        }
        return false;
    }
    BOOL result = DeleteFileA(Path);
    if (!result && ErrorCode) {
        DWORD win32Error = GetLastError();
        *ErrorCode = MapWin32ErrorToAxPlatformError(win32Error);
    }
    return result != 0;
}

/* ========================================================================
   Axon Directory
   ======================================================================== */

// Helper function to map Win32 error codes to platform-agnostic error codes
static AxPlatformError MapWin32ErrorToAxPlatformError(DWORD win32Error)
{
    switch (win32Error) {
        case ERROR_SUCCESS:
            return AX_PLATFORM_SUCCESS;
        case ERROR_INVALID_PARAMETER:
            return AX_PLATFORM_ERROR_INVALID_PARAMETER;
        case ERROR_PATH_NOT_FOUND:
            return AX_PLATFORM_ERROR_PATH_NOT_FOUND;
        case ERROR_FILE_NOT_FOUND:
            return AX_PLATFORM_ERROR_FILE_NOT_FOUND;
        case ERROR_ALREADY_EXISTS:
            return AX_PLATFORM_ERROR_ALREADY_EXISTS;
        case ERROR_ACCESS_DENIED:
            return AX_PLATFORM_ERROR_ACCESS_DENIED;
        case ERROR_BUSY:
            return AX_PLATFORM_ERROR_BUSY;
        case ERROR_WRITE_PROTECT:
            return AX_PLATFORM_ERROR_READONLY;
        case ERROR_DISK_FULL:
            return AX_PLATFORM_ERROR_DISK_FULL;
        case ERROR_FILENAME_EXCED_RANGE:
            return AX_PLATFORM_ERROR_PATH_TOO_LONG;
        default:
            return AX_PLATFORM_ERROR_UNKNOWN;
    }
}

static bool CreateDir(const char *Path, AxPlatformError *ErrorCode)
{
    AXON_ASSERT(Path);
    if (ErrorCode) {
        *ErrorCode = AX_PLATFORM_SUCCESS; // Initialize to success
    }
    if (!Path || strlen(Path) == 0) {
        if (ErrorCode) {
            *ErrorCode = AX_PLATFORM_ERROR_INVALID_PARAMETER;
        }
        return (false);
    }
    // Try to create the directory directly first
    BOOL Result = CreateDirectoryA(Path, NULL);
    if (Result) {
        return (true);
    }
    DWORD Win32Error = GetLastError();
    // If it failed because parent doesn't exist, try to create parent directories
    if (Win32Error == ERROR_PATH_NOT_FOUND) {
        // Create a copy of the path to work with
        char PathCopy[MAX_PATH];
        strncpy(PathCopy, Path, MAX_PATH - 1);
        PathCopy[MAX_PATH - 1] = '\0';
            // Find the last backslash
        char *LastSlash = strrchr(PathCopy, '\\');
        if (LastSlash && LastSlash != PathCopy) {
            *LastSlash = '\0'; // Temporarily terminate at parent directory
                    // Only recurse if we have a valid parent path
            if (strlen(PathCopy) > 0) {
                // Recursively create parent directory
                if (CreateDir(PathCopy, ErrorCode)) {
                    // Now try to create the original directory
                    Result = CreateDirectoryA(Path, NULL);
                    if (Result) {
                        return (true);
                    }
                    Win32Error = GetLastError();
                }
            }
        }
    }
    if (ErrorCode) {
        *ErrorCode = MapWin32ErrorToAxPlatformError(Win32Error);
    }
    return false;
}

static bool RemoveDir(const char *Path, bool Recursive, AxPlatformError *ErrorCode)
{
    AXON_ASSERT(Path);
    if (ErrorCode) {
        *ErrorCode = AX_PLATFORM_SUCCESS; // Initialize to success
    }
    if (!Path || strlen(Path) == 0) {
        if (ErrorCode) {
            *ErrorCode = AX_PLATFORM_ERROR_INVALID_PARAMETER;
        }
        return false;
    }
    // First check if the directory exists
    DWORD Attributes = GetFileAttributesA(Path);
    if (Attributes == INVALID_FILE_ATTRIBUTES) {
        if (ErrorCode) {
            DWORD Win32Error = GetLastError();
            *ErrorCode = MapWin32ErrorToAxPlatformError(Win32Error);
        }
        return false;
    }
    // Check if it's actually a directory
    if (!(Attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        if (ErrorCode) {
            *ErrorCode = AX_PLATFORM_ERROR_ACCESS_DENIED; // Not a directory, treat as access denied
        }
        return false;
    }
    // Check if directory is empty
    char SearchPattern[MAX_PATH];
    int PathLen = strlen(Path);
    if (PathLen >= MAX_PATH - 2) {
        if (ErrorCode) {
            *ErrorCode = AX_PLATFORM_ERROR_PATH_TOO_LONG;
        }
        return false;
    }
    strcpy(SearchPattern, Path);
    // Add trailing slash if needed
    if (PathLen > 0 && SearchPattern[PathLen - 1] != '\\' && SearchPattern[PathLen - 1] != '/') {
        strcat(SearchPattern, "\\");
    }
    strcat(SearchPattern, "*");
    WIN32_FIND_DATAA FindData;
    HANDLE FindHandle = FindFirstFileA(SearchPattern, &FindData);
    if (FindHandle == INVALID_HANDLE_VALUE) {
        if (ErrorCode) {
            DWORD Win32Error = GetLastError();
            *ErrorCode = MapWin32ErrorToAxPlatformError(Win32Error);
        }
        return false;
    }
    bool HasEntries = false;
    do {
        // Skip "." and ".." entries
        if (strcmp(FindData.cFileName, ".") != 0 && strcmp(FindData.cFileName, "..") != 0) {
            HasEntries = true;
            break;
        }
    } while (FindNextFileA(FindHandle, &FindData));
    FindClose(FindHandle);
    if (HasEntries) {
        if (Recursive) {
            // Use recursive removal
            return RemoveDirRecursive(Path, ErrorCode);
        } else {
            if (ErrorCode) {
                *ErrorCode = AX_PLATFORM_ERROR_BUSY; // Directory not empty, treat as busy
            }
            return false;
        }
    }
    // Directory is empty, now try to remove it
    BOOL Result = RemoveDirectoryA(Path);
    if (!Result && ErrorCode) {
        DWORD Win32Error = GetLastError();
        *ErrorCode = MapWin32ErrorToAxPlatformError(Win32Error);
    }
    return Result != 0;
}

// Helper function to recursively remove a directory and all its contents
static bool RemoveDirRecursive(const char *Path, AxPlatformError *ErrorCode)
{
    // Create search pattern by appending "\*" to the path
    char SearchPattern[MAX_PATH];
    int PathLen = strlen(Path);
    if (PathLen >= MAX_PATH - 2) {
        if (ErrorCode) {
            *ErrorCode = AX_PLATFORM_ERROR_PATH_TOO_LONG;
        }
        return false;
    }
    strcpy(SearchPattern, Path);
    // Add trailing slash if needed
    if (PathLen > 0 && SearchPattern[PathLen - 1] != '\\' && SearchPattern[PathLen - 1] != '/') {
        strcat(SearchPattern, "\\");
    }
    strcat(SearchPattern, "*");
    WIN32_FIND_DATAA FindData;
    HANDLE FindHandle = FindFirstFileA(SearchPattern, &FindData);
    if (FindHandle == INVALID_HANDLE_VALUE) {
        if (ErrorCode) {
            DWORD win32Error = GetLastError();
            *ErrorCode = MapWin32ErrorToAxPlatformError(win32Error);
        }
        return false;
    }
    bool Success = true;
    do {
        // Skip "." and ".." entries
        if (strcmp(FindData.cFileName, ".") == 0 || strcmp(FindData.cFileName, "..") == 0) {
            continue;
        }
            // Build full path for this entry
        char FullPath[MAX_PATH];
        strcpy(FullPath, Path);
        if (PathLen > 0 && FullPath[PathLen - 1] != '\\' && FullPath[PathLen - 1] != '/') {
            strcat(FullPath, "\\");
        }
        strcat(FullPath, FindData.cFileName);
            if (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // Recursively remove subdirectory
            if (!RemoveDirRecursive(FullPath, ErrorCode)) {
                Success = false;
                break;
            }
        } else {
            // Delete file
            if (!DeleteFileA(FullPath)) {
                if (ErrorCode) {
                    DWORD win32Error = GetLastError();
                    *ErrorCode = MapWin32ErrorToAxPlatformError(win32Error);
                }
                Success = false;
                break;
            }
        }
    } while (FindNextFileA(FindHandle, &FindData));
    FindClose(FindHandle);
    if (!Success) {
        return false;
    }
    // Now remove the empty directory
    BOOL result = RemoveDirectoryA(Path);
    if (!result && ErrorCode) {
        DWORD win32Error = GetLastError();
        *ErrorCode = MapWin32ErrorToAxPlatformError(win32Error);
    }
    return result != 0;
}

static bool ChangeDirectory(const char *Path)
{
    AXON_ASSERT(Path);
    return SetCurrentDirectoryA(Path) != 0;
}

static char *GetCurrentDirectory_Internal(void)
{
    static char CurrentDirectory[MAX_PATH];
    DWORD Length = GetCurrentDirectoryA(MAX_PATH, CurrentDirectory);
    if (Length > 0 && Length < MAX_PATH) {
        return CurrentDirectory;
    }
    return NULL;
}

static bool IsDirectoryHandleValid(AxDirectoryHandle Handle)
{
    return (Handle.Handle != 0 && Handle.Handle != (uint64_t)INVALID_HANDLE_VALUE);
}

static AxDirectoryHandle OpenDirectory(const char *Path)
{
    AXON_ASSERT(Path);
    AxDirectoryHandle DirHandle = { 0 };
    // Create search pattern by appending "\*" to the path
    char SearchPattern[MAX_PATH];
    int PathLen = strlen(Path);
    if (PathLen >= MAX_PATH - 2) {
        return DirHandle; // Path too long
    }
    strcpy(SearchPattern, Path);
    // Add trailing slash if needed
    if (PathLen > 0 && SearchPattern[PathLen - 1] != '\\' && SearchPattern[PathLen - 1] != '/') {
        strcat(SearchPattern, "\\");
    }
    strcat(SearchPattern, "*");
    WIN32_FIND_DATAA FindData;
    HANDLE Handle = FindFirstFileA(SearchPattern, &FindData);
    if (Handle != INVALID_HANDLE_VALUE) {
        DirHandle.Handle = (uint64_t)Handle;
    }
    return DirHandle;
}

static bool ReadDirectoryEntry(AxDirectoryHandle Handle, AxDirectoryEntry *Entry)
{
    AXON_ASSERT(IsDirectoryHandleValid(Handle));
    AXON_ASSERT(Entry);
    WIN32_FIND_DATAA FindData;
    HANDLE WinHandle = (HANDLE)Handle.Handle;
    bool Found = false;
    // Keep looking for the next valid entry
    do {
        Found = FindNextFileA(WinHandle, &FindData);
            if (!Found) {
            return false;
        }
            // Skip "." and ".." entries
    } while (strcmp(FindData.cFileName, ".") == 0 || strcmp(FindData.cFileName, "..") == 0);
    // Fill in the directory entry
    strncpy(Entry->Name, FindData.cFileName, sizeof(Entry->Name) - 1);
    Entry->Name[sizeof(Entry->Name) - 1] = '\0';
    Entry->IsDirectory = (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    // Calculate file size
    LARGE_INTEGER FileSize;
    FileSize.LowPart = FindData.nFileSizeLow;
    FileSize.HighPart = FindData.nFileSizeHigh;
    Entry->Size = FileSize.QuadPart;
    return true;
}

static bool CloseDirectory(AxDirectoryHandle Handle)
{
    if (!IsDirectoryHandleValid(Handle)) {
        return false;
    }
    return (FindClose((HANDLE)Handle.Handle) != 0);
}

/* ========================================================================
   Axon DLL
   ======================================================================== */

static AxDLL DLLLoad(const char *Path)
{
    AxDLL DLL = { 0 };
    void *Handle = LoadLibrary(Path);
    if (Handle) {
        DLL.Opaque = (uint64_t)Handle;
    }

    return(DLL);
}

static void DLLUnload(AxDLL DLL)
{
    HMODULE *Handle = (HMODULE *)DLL.Opaque;
    if (Handle) {
        FreeLibrary((HMODULE)DLL.Opaque);
    }
}

static bool DLLIsValid(AxDLL DLL)
{
    return(DLL.Opaque ? true : false);
}

static void *DLLSymbol(AxDLL DLL, const char *SymbolName)
{
    void *Symbol = (void *)GetProcAddress((HMODULE)DLL.Opaque, SymbolName);

    return(Symbol ? Symbol : NULL);
}

/* ========================================================================
   Time
   ======================================================================== */

static AxWallClock WallTime(void)
{
    LARGE_INTEGER Result;
    QueryPerformanceCounter(&Result);

    return ((AxWallClock){ Result.QuadPart });
}

static float ElapsedWallTime(AxWallClock Start, AxWallClock End)
{
    // TODO(mdeforge): This can be cached.
    LARGE_INTEGER Frequency;
    QueryPerformanceFrequency(&Frequency);

    return ((float)(End.Ticks - Start.Ticks) / (float)Frequency.QuadPart);
}

/* ========================================================================
   System
   ======================================================================== */

void GetSysInfo(uint32_t *PageSize, uint32_t *AllocationGranularity)
{
    SYSTEM_INFO SystemInfo;
    GetSystemInfo(&SystemInfo);

    // Seems safe to assume a uint32_t is large enough
    *PageSize = (uint32_t)SystemInfo.dwPageSize;
    *AllocationGranularity = (uint32_t)SystemInfo.dwAllocationGranularity;
}

/* ========================================================================
   Setup
   ======================================================================== */

struct AxPlatformAPI *PlatformAPI = &(struct AxPlatformAPI) {
    .DirectoryAPI = &(struct AxPlatformDirectoryAPI) {
        .CreateDir = CreateDir,
        .RemoveDir = RemoveDir,
        .ChangeDirectory = ChangeDirectory,
        .GetCurrentDirectory = GetCurrentDirectory_Internal,
        .OpenDirectory = OpenDirectory,
        .ReadDirectoryEntry = ReadDirectoryEntry,
        .CloseDirectory = CloseDirectory,
        .IsDirectoryHandleValid = IsDirectoryHandleValid
    },
    .DLLAPI =  &(struct AxPlatformDLLAPI) {
        .Load = DLLLoad,
        .Unload = DLLUnload,
        .IsValid = DLLIsValid,
        .Symbol = DLLSymbol
    },
    .FileAPI = &(struct AxPlatformFileAPI) {
        .IsValid = FileIsValid,
        .OpenForRead = FileOpenForRead,
        .OpenForWrite = FileOpenForWrite,
        .SetPosition = FileSetPosition,
        .Size = FileSize,
        .Read = FileRead,
        .Write = FileWrite,
        .Close = FileClose,
        .DeleteFile = FileDelete
    },
    .PathAPI = &(struct AxPlatformPathAPI) {
        .FileExists = FileExists,
        .DirectoryExists = DirectoryExists,
        .BasePath = BasePath
    },
    .TimeAPI = &(struct AxTimeAPI) {
        .WallTime = WallTime,
        .ElapsedWallTime = ElapsedWallTime
    }
};

#if 0
void
BuildEXEPathFileName(win32_state *State, char *Filename, int DestCount, char *Dest)
{
    CatStrings(State->OnePastLastEXEFileNameSlash - State->EXEFileName, State->EXEFileName,
               StringLength(Filename), Filename,
               DestCount, Dest);
}

void
GetEXEFileName(win32_state *State)
{
    DWORD SizeOfFilename = GetModuleFileNameA(0, State->EXEFileName, sizeof(State->EXEFileName));
    State->OnePastLastEXEFileNameSlash = State->EXEFileName;
    for (char *Scan = State->EXEFileName; *Scan; ++Scan)
    {
        if (*Scan == '\\')
        {
            State->OnePastLastEXEFileNameSlash = Scan + 1;
        }
    }
}

void
GetInputFileLocation(win32_state *State, int SlotIndex, int DestCount, char *Dest)
{
    AXON_ASSERT(SlotIndex == 1);
    Win32BuildEXEPathFileName(State, "foo.ai", DestCount, Dest);
}

FILETIME
GetLastWriteTime(char *FileName)
{
    FILETIME LastWriteTime = {};
/*
  WIN32_FILE_ATTRIBUTE_DATA Data;
  if (GetFileAttributesEx(Filename, GetFileExInfoStandard, &Data))
  {
  LastWriteTime = Data.ftLastWriteTime;
  }
*/
#if 0
    WIN32_FIND_DATA FindData;
    HANDLE FindHandle = FindFirstFileA(Filename, &FindData);
    if(FindHandle != INVALID_HANDLE_VALUE)
    {
        LastWriteTime = FindData.ftLastWriteTime;
        FindClose(FindHandle);
    }
#else
    WIN32_FILE_ATTRIBUTE_DATA Data;
    if(GetFileAttributesEx(FileName, GetFileExInfoStandard, &Data))
    {
        LastWriteTime = Data.ftLastWriteTime;
    }
#endif    return(LastWriteTime);
}
#endif
