#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <ShObjIdl.h>
#include <shellapi.h>
#include "AxHashTable.h"
#include "platform.h"

static AxHashTable *DLLMap; // <Path, Pointer>

/* ========================================================================
   Axon Path
   ======================================================================== */

static bool FileExists(const char *Path)
{
    uint32_t result = GetFileAttributes(Path);
    if (result != 0xFFFFFFFF && !(result & FILE_ATTRIBUTE_DIRECTORY)) {
        return (true);
    }

    return (false); 
}

static bool DirectoryExists(const char *Path)
{
    // bool exists = !dir.Len();
    // if (!exists)
    // {
    //     Paths::NormalizeDirectory(dir);
    //     uint32_t result = GetFileAttributesA(dir);
    //     exists = (result != 0xFFFFFFFF && (result & FILE_ATTRIBUTE_DIRECTORY));
    // }

    // return exists;
}

static char *CurrentWorkingDirectory(void)
{

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
    if (Handle != INVALID_HANDLE_VALUE)
    {
        File.Handle = (uint64_t)Handle;
    }

    return(File);
}

static int64_t FileSetPosition(AxFile File, int64_t Position)
{
    Assert(FileIsValid(File));
    Assert(Position >= 0);

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
    Assert(FileIsValid(File));
    
    LARGE_INTEGER LargeInt;
    GetFileSizeEx((HANDLE)File.Handle, &LargeInt);
    
    return(LargeInt.QuadPart);
}

static int64_t FileRead(AxFile File, void *Buffer, uint64_t BytesToRead)
{
    Assert(FileIsValid(File));
    Assert(Buffer != NULL);

    //axon_platform_file_io &file_io = AxPlatformAPI->file_io;

    // Return if no bytes are to be read.
    if (BytesToRead <= 0)
    {
        return (0);
    }

    // If file size is smaller than requested read, read the number 
    // of bytes that are available and return the number of read bytes.
    uint64_t size = FileSize(File);
    if (size < BytesToRead)
    {
        BytesToRead = size;
    }

    // TODO(mdeforge): If EOF, return 0

    uint32_t NumBytesRead;
    if (!ReadFile((HANDLE)File.Handle, Buffer, (DWORD)BytesToRead, (DWORD*)&NumBytesRead, NULL))
    {
        return(-1);
    }

    return(NumBytesRead);
}

static void FileClose(AxFile File)
{
    Assert(FileIsValid(File));
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
    if (!DLLMap) {
        DLLMap = CreateTable(10);
    }

    uint64_t Handle = (uint64_t)LoadLibrary(Path);
    if (Handle) {
        return((AxDLL){ .Opaque = Handle });
    }

    return((AxDLL){ .Opaque = 0 });
}

static void DLLUnload(AxDLL DLL)
{
    FreeLibrary((HMODULE)DLL.Opaque);
}

static AxDLL DLLGet(const char *Path)
{
    if (DLLMap)
    {
        uint64_t Handle = (uint64_t)HashTableSearch(DLLMap, Path);
        return ((AxDLL){ .Opaque = Handle });
    }

    return ((AxDLL){ .Opaque = 0 });
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
   Setup
   ======================================================================== */

struct AxPlatformAPI *AxPlatformAPI = &(struct AxPlatformAPI) {
    .File = &(struct AxPlatformFileAPI) {
        .IsValid = FileIsValid,
        .OpenForRead = FileOpenForRead,
        .SetPosition = FileSetPosition,
        .Size = FileSize,
        .Read = FileRead,
        .Close = FileClose
    },
    .Directory = &(struct AxPlatformDirectoryAPI) {
        .CreateDir = CreateDir,
        .RemoveDir = RemoveDir
    },
    .DLL =  &(struct AxPlatformDLLAPI) {
        .Load = DLLLoad,
        .Unload = DLLUnload,
        .Get = DLLGet,
        .IsValid = DLLIsValid,
        .Symbol = DLLSymbol
    },
    .Time = &(struct AxTimeAPI) {
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
    Assert(SlotIndex == 1);
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
