#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <ShObjIdl.h>
#include <shellapi.h>
#include "AxHashTable.h"
#include "AxPlatform.h"

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
    // bool exists = !dir.Len();
    // if (!exists)
    // {
    //     Paths::NormalizeDirectory(dir);
    //     uint32_t result = GetFileAttributesA(dir);
    //     exists = (result != 0xFFFFFFFF && (result & FILE_ATTRIBUTE_DIRECTORY));
    // }

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

    LPDWORD NumBytesRead = 0;
    if (!ReadFile((HANDLE)File.Handle, Buffer, (DWORD)BytesToRead, NumBytesRead, NULL)) {
        return(0);
    }

    return((uint64_t)NumBytesRead);
}

static uint64_t FileWrite(AxFile File, void* Buffer, uint32_t BytesToWrite)
{
    AXON_ASSERT(FileIsValid(File));
    AXON_ASSERT(Buffer != NULL);

    LPDWORD NumBytesWritten = 0;
    if (!WriteFile((HANDLE)File.Handle, Buffer, BytesToWrite, NumBytesWritten, NULL)) {
        return(0);
    }

    return((uint64_t)NumBytesWritten);
}

static void FileClose(AxFile File)
{
    AXON_ASSERT(FileIsValid(File));
    CloseHandle((HANDLE)File.Handle);
}

/* ========================================================================
   Axon Directory
   ======================================================================== */

static bool CreateDir(const char *Path)
{
    return (CreateDirectory(Path, NULL));
}

static bool RemoveDir(const char *Path)
{
    SHFILEOPSTRUCT FileOp = { 0 };
    FileOp.hwnd = NULL;
    FileOp.wFunc = FO_DELETE;
    FileOp.pFrom = TEXT(Path);
    FileOp.fFlags = FOF_SILENT | FOF_NOCONFIRMATION;

    if (SHFileOperation(&FileOp) == 0) {
        return (true);
    }

    return (false);
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
        .RemoveDir = RemoveDir
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
        .Close = FileClose
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
#endif    
    return(LastWriteTime);
}
#endif
