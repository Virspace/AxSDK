#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include "AxHashTable.h"
#include "AxPlatform.h"

// TODO(mdeforge): size_t vs. uint_X_t vs int_X_t

/* ========================================================================
   Axon Path
   ======================================================================== */

static bool FileExists(const char *Path)
{
    struct stat FileInfo;
    if (stat(Path, &FileInfo) == 0)
    {
        if (!(S_ISDIR(FileInfo.st_mode)))
        {
            return (true);
        }
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

    int Handle = open(Path, O_RDONLY);
    if (Handle != -1)
    {
        File.Handle = (uint64_t)Handle;
    }

    return(File);
}

static int64_t FileSetPosition(AxFile File, int64_t Position)
{
    Assert(FileIsValid(File));
    Assert(Position >= 0);

    off_t Offset = lseek(File.Handle, Position, SEEK_SET);

    if (Offset == -1)
    {
        return -1;
    }

    return Offset;
}

static uint64_t FileSize(AxFile File)
{
    Assert(FileIsValid(File));

    struct stat FileInfo;
    if (fstat(File.Handle, &FileInfo) != -1)
    {
        return FileInfo.st_size;
    }

    return 0;
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

    // NOTE(mdeforge): I'm not sure what this was about...
    //fseek(File.Handle, 0, SEEK_END);
    //long Size = ftell(File.Handle);
    //rewind(File.Handle);

    // If file size is smaller than requested read, read the number 
    // of bytes that are available and return the number of read bytes.
    uint64_t Size = FileSize(File);
    if (Size < BytesToRead)
    {
        BytesToRead = Size;
    }

    size_t NumBytesRead = fread(Buffer, 1, Size, (FILE *)File.Handle);
    if (NumBytesRead != Size)
    {
        fclose((FILE *)File.Handle);
        return -1;
    }

    fclose((FILE *)File.Handle);

    return((int64_t)NumBytesRead);
}

/* ========================================================================
   Axon Directory
   ======================================================================== */

static void FileClose(AxFile File)
{
    Assert(FileIsValid(File));
    close(File.Handle);
}

/* ========================================================================
   Axon Directory
   ======================================================================== */
/*
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
/*
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
/*
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

struct AxPlatformAPI *PlatformAPI = &(struct AxPlatformAPI) {
    .File = &(struct AxPlatformFileAPI) {
        .IsValid = FileIsValid,
        .OpenForRead = FileOpenForRead,
        .SetPosition = FileSetPosition,
        .Size = FileSize,
        .Read = FileRead,
        .Close = FileClose
    },
    // .Directory = &(struct AxPlatformDirectoryAPI) {
    //     .CreateDir = CreateDir,
    //     .RemoveDir = RemoveDir
    // },
    // .DLL =  &(struct AxPlatformDLLAPI) {
    //     .Load = DLLLoad,
    //     .Unload = DLLUnload,
    //     .IsValid = DLLIsValid,
    //     .Symbol = DLLSymbol
    // },
    // .Time = &(struct AxTimeAPI) {
    //     .WallTime = WallTime,
    //     .ElapsedWallTime = ElapsedWallTime
    // }
};