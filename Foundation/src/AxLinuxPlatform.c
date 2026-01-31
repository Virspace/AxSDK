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
    struct stat FileInfo;
    if (stat(Path, &FileInfo) == 0)
    {
        if (S_ISDIR(FileInfo.st_mode))
        {
            return (true);
        }
    }

    return (false);
}

static char *CurrentWorkingDirectory(void)
{
    return (NULL);
}

/* ========================================================================
   Axon File
   ======================================================================== */

static bool FileIsValid(AxFile File)
{
    return (File.Handle != 0);
}

static AxFile FileOpenForRead(const char *Path)
{
    AxFile File = { 0 };

    int Handle = open(Path, O_RDONLY);
    if (Handle != -1)
    {
        File.Handle = (uint64_t)Handle;
    }

    return (File);
}

static AxFile FileOpenForWrite(const char *Path)
{
    AxFile File = { 0 };

    int Handle = open(Path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (Handle != -1)
    {
        File.Handle = (uint64_t)Handle;
    }

    return (File);
}

static int64_t FileSetPosition(AxFile File, int64_t Position)
{
    AXON_ASSERT(FileIsValid(File));
    AXON_ASSERT(Position >= 0);

    off_t Offset = lseek(File.Handle, Position, SEEK_SET);

    if (Offset == -1)
    {
        return (-1);
    }

    return (Offset);
}

static uint64_t FileSize(AxFile File)
{
    AXON_ASSERT(FileIsValid(File));

    struct stat FileInfo;
    if (fstat(File.Handle, &FileInfo) != -1)
    {
        return (FileInfo.st_size);
    }

    return (0);
}

static uint64_t FileRead(AxFile File, void *Buffer, uint32_t BytesToRead)
{
    AXON_ASSERT(FileIsValid(File));
    AXON_ASSERT(Buffer != NULL);

    // Return if no bytes are to be read.
    if (BytesToRead <= 0)
    {
        return (0);
    }

    ssize_t NumBytesRead = read(File.Handle, Buffer, BytesToRead);
    if (NumBytesRead == -1)
    {
        return (0);
    }

    return ((uint64_t)NumBytesRead);
}

static uint64_t FileWrite(AxFile File, void *Buffer, uint32_t BytesToWrite)
{
    AXON_ASSERT(FileIsValid(File));
    AXON_ASSERT(Buffer != NULL);

    if (BytesToWrite <= 0)
    {
        return (0);
    }

    ssize_t NumBytesWritten = write(File.Handle, Buffer, BytesToWrite);
    if (NumBytesWritten == -1)
    {
        return (0);
    }

    return ((uint64_t)NumBytesWritten);
}

static void FileClose(AxFile File)
{
    AXON_ASSERT(FileIsValid(File));
    close(File.Handle);
}

/* ========================================================================
   System Info
   ======================================================================== */

void GetSysInfo(uint32_t *PageSize, uint32_t *AllocationGranularity)
{
    // On Linux, page size is typically 4096
    long pageSize = sysconf(_SC_PAGESIZE);
    *PageSize = (uint32_t)pageSize;
    // Linux doesn't have a separate allocation granularity concept like Windows,
    // so we use page size for both
    *AllocationGranularity = (uint32_t)pageSize;
}

/* ========================================================================
   Setup
   ======================================================================== */

struct AxPlatformAPI *PlatformAPI = &(struct AxPlatformAPI) {
    .FileAPI = &(struct AxPlatformFileAPI) {
        .OpenForRead = FileOpenForRead,
        .OpenForWrite = FileOpenForWrite,
        .SetPosition = FileSetPosition,
        .Size = FileSize,
        .Read = FileRead,
        .Write = FileWrite,
        .IsValid = FileIsValid,
        .Close = FileClose
    },
    .DirectoryAPI = NULL,
    .DLLAPI = NULL,
    .PathAPI = NULL,
    .TimeAPI = NULL
};
