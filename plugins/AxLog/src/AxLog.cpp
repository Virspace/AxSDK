#include "AxLog/AxLog.h"
#include "Foundation/AxPlatform.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <string_view>
#include <chrono>
#include <ctime>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

/* ------------------------------------------------------------------ */
/*  File-scope state                                                   */
/* ------------------------------------------------------------------ */

/* Runtime minimum log level -- defaults to TRACE (all messages pass) */
static int g_RuntimeLogLevel = AX_LOG_LEVEL_TRACE;

/* File output handle (invalid until AxLogOpenFile is called) */
static AxFile g_LogFile = { 0 };
static bool g_LogFileActive = false;

/* Uptime epoch recorded by AxLogOpenFile */
static AxWallClock g_UptimeEpoch = { 0 };
static bool g_UptimeEpochSet = false;

#ifdef _WIN32
/* Cached console handle and ANSI support flag */
static HANDLE g_StderrHandle = INVALID_HANDLE_VALUE;
static bool g_ConsoleInitialized = false;
static bool g_AnsiSupported = false;

static void InitConsole()
{
    if (g_ConsoleInitialized)
    {
        return;
    }

    g_StderrHandle = GetStdHandle(STD_ERROR_HANDLE);
    g_ConsoleInitialized = true;

    /* Try to enable ANSI escape code processing (Windows 10 version 1511+) */
    if (g_StderrHandle != INVALID_HANDLE_VALUE && g_StderrHandle != NULL)
    {
        DWORD Mode = 0;
        if (GetConsoleMode(g_StderrHandle, &Mode))
        {
            if (SetConsoleMode(g_StderrHandle, Mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
            {
                g_AnsiSupported = true;
            }
        }
    }
}
#endif

/* ------------------------------------------------------------------ */
/*  Severity label and color tables                                    */
/* ------------------------------------------------------------------ */

static constexpr std::string_view g_SeverityLabels[] =
{
    "TRACE",    /* AX_LOG_LEVEL_TRACE   = 0 */
    "DEBUG",    /* AX_LOG_LEVEL_DEBUG   = 1 */
    "INFO",     /* AX_LOG_LEVEL_INFO    = 2 */
    "WARNING",  /* AX_LOG_LEVEL_WARNING = 3 */
    "ERROR",    /* AX_LOG_LEVEL_ERROR   = 4 */
    "FATAL",    /* AX_LOG_LEVEL_FATAL   = 5 */
};

/* ANSI escape codes for console coloring */
static constexpr std::string_view g_AnsiColors[] =
{
    "\x1b[2m",     /* TRACE   = dim gray    */
    "\x1b[36m",    /* DEBUG   = cyan        */
    "\x1b[0m",     /* INFO    = default     */
    "\x1b[33m",    /* WARNING = yellow      */
    "\x1b[31m",    /* ERROR   = red         */
    "\x1b[1;31m",  /* FATAL   = bold red    */
};

static constexpr std::string_view g_AnsiReset = "\x1b[0m";

#ifdef _WIN32
/* Windows Console API color attributes (fallback for pre-Windows-10) */
static const WORD g_Win32Colors[] =
{
    FOREGROUND_INTENSITY,                                           /* TRACE   = dark gray */
    FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,      /* DEBUG   = cyan      */
    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,            /* INFO    = white     */
    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,       /* WARNING = yellow    */
    FOREGROUND_RED | FOREGROUND_INTENSITY,                          /* ERROR   = red       */
    FOREGROUND_RED | FOREGROUND_INTENSITY,                          /* FATAL   = bright red */
};

static const WORD g_Win32DefaultColor = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
#endif

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                   */
/* ------------------------------------------------------------------ */

/* Extracts just the filename from a full file path using string_view */
static std::string_view ExtractFilename(std::string_view FullPath)
{
    auto Pos = FullPath.find_last_of("/\\");
    if (Pos == std::string_view::npos)
    {
        return (FullPath);
    }
    return (FullPath.substr(Pos + 1));
}

/* Formats the current date/time as [YYYY-MM-DD HH:MM:SS.mmm] */
static int FormatDateTime(char *Buffer, size_t BufferSize)
{
    auto Now = std::chrono::system_clock::now();
    auto TimeT = std::chrono::system_clock::to_time_t(Now);
    auto Ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        Now.time_since_epoch()) % 1000;

    struct tm TimeBuf;
#ifdef _WIN32
    localtime_s(&TimeBuf, &TimeT);
#else
    localtime_r(&TimeT, &TimeBuf);
#endif

    int Written = snprintf(Buffer, BufferSize,
        "[%04d-%02d-%02d %02d:%02d:%02d.%03d]",
        TimeBuf.tm_year + 1900,
        TimeBuf.tm_mon + 1,
        TimeBuf.tm_mday,
        TimeBuf.tm_hour,
        TimeBuf.tm_min,
        TimeBuf.tm_sec,
        (int)Ms.count());

    return (Written);
}

/* Formats the uptime as [+NNN.NNNs] */
static int FormatUptime(char *Buffer, size_t BufferSize)
{
    if (!g_UptimeEpochSet || PlatformAPI == nullptr || PlatformAPI->TimeAPI == nullptr)
    {
        return (snprintf(Buffer, BufferSize, "[+0.000s]"));
    }

    AxWallClock Now = PlatformAPI->TimeAPI->WallTime();
    float Elapsed = PlatformAPI->TimeAPI->ElapsedWallTime(g_UptimeEpoch, Now);

    return (snprintf(Buffer, BufferSize, "[+%.3fs]", Elapsed));
}

/* Formats the thread ID as [tid:XXXX] */
static int FormatThreadId(char *Buffer, size_t BufferSize)
{
#ifdef _WIN32
    DWORD Tid = GetCurrentThreadId();
    return (snprintf(Buffer, BufferSize, "[tid:%04lx]", (unsigned long)Tid));
#else
    auto Tid = std::this_thread::get_id();
    unsigned long TidVal = 0;
    memcpy(&TidVal, &Tid, sizeof(TidVal) < sizeof(Tid) ? sizeof(TidVal) : sizeof(Tid));
    return (snprintf(Buffer, BufferSize, "[tid:%04lx]", TidVal));
#endif
}

/* ------------------------------------------------------------------ */
/*  Core log write implementation                                      */
/* ------------------------------------------------------------------ */

extern "C" void AxLogWrite(int Level, const char *File, int Line, const char *Fmt, ...)
{
    /* Validate severity level */
    if (Level < AX_LOG_LEVEL_TRACE || Level > AX_LOG_LEVEL_FATAL)
    {
        return;
    }

    /* Format the user message */
    char UserMsg[2048];
    va_list Args;
    va_start(Args, Fmt);
    vsnprintf(UserMsg, sizeof(UserMsg), Fmt, Args);
    va_end(Args);

    /* Extract filename from full path */
    std::string_view Filename = ExtractFilename(File ? File : "");
    std::string_view SeverityLabel = g_SeverityLabels[Level];

    /* Build the filename:line field */
    char FileLineField[64];
    snprintf(FileLineField, sizeof(FileLineField), "%.*s:%d",
        (int)Filename.size(), Filename.data(), Line);

    /* Format date/time, uptime, and thread ID */
    char DateTime[32];
    FormatDateTime(DateTime, sizeof(DateTime));

    char Uptime[32];
    FormatUptime(Uptime, sizeof(Uptime));

    char ThreadId[32];
    FormatThreadId(ThreadId, sizeof(ThreadId));

    /* Build the plain-text line (no ANSI codes) for file output */
    char PlainLine[4096];
    snprintf(PlainLine, sizeof(PlainLine), "%s %-12s %s %-30s %-8.*s %s\n",
        DateTime,
        Uptime,
        ThreadId,
        FileLineField,
        (int)SeverityLabel.size(), SeverityLabel.data(),
        UserMsg);

    /* ---- Console output (stderr, with color) ---- */
#ifdef _WIN32
    InitConsole();

    if (g_AnsiSupported)
    {
        /* ANSI escape codes supported -- use them */
        std::string_view Color = g_AnsiColors[Level];
        fprintf(stderr, "%.*s%s %-12s %s %-30s %-8.*s %s%.*s\n",
            (int)Color.size(), Color.data(),
            DateTime,
            Uptime,
            ThreadId,
            FileLineField,
            (int)SeverityLabel.size(), SeverityLabel.data(),
            UserMsg,
            (int)g_AnsiReset.size(), g_AnsiReset.data());
    }
    else if (g_StderrHandle != INVALID_HANDLE_VALUE && g_StderrHandle != NULL)
    {
        /* Fallback: Windows Console API for color */
        CONSOLE_SCREEN_BUFFER_INFO Info;
        WORD OriginalAttrs = g_Win32DefaultColor;
        if (GetConsoleScreenBufferInfo(g_StderrHandle, &Info))
        {
            OriginalAttrs = Info.wAttributes;
        }

        SetConsoleTextAttribute(g_StderrHandle, g_Win32Colors[Level]);
        fprintf(stderr, "%s %-12s %s %-30s %-8.*s %s\n",
            DateTime,
            Uptime,
            ThreadId,
            FileLineField,
            (int)SeverityLabel.size(), SeverityLabel.data(),
            UserMsg);
        fflush(stderr);
        SetConsoleTextAttribute(g_StderrHandle, OriginalAttrs);
    }
    else
    {
        /* No console handle -- just write plain text */
        fprintf(stderr, "%s", PlainLine);
    }
#else
    /* Unix: ANSI codes are always supported in terminals */
    std::string_view Color = g_AnsiColors[Level];
    fprintf(stderr, "%.*s%s %-12s %s %-30s %-8.*s %s%.*s\n",
        (int)Color.size(), Color.data(),
        DateTime,
        Uptime,
        ThreadId,
        FileLineField,
        (int)SeverityLabel.size(), SeverityLabel.data(),
        UserMsg,
        (int)g_AnsiReset.size(), g_AnsiReset.data());
#endif

    /* ---- File output (plain text, no ANSI codes) ---- */
    if (g_LogFileActive && PlatformAPI != nullptr && PlatformAPI->FileAPI != nullptr)
    {
        size_t Len = strlen(PlainLine);
        PlatformAPI->FileAPI->Write(g_LogFile, (void *)PlainLine, (uint32_t)Len);
        PlatformAPI->FileAPI->Flush(g_LogFile);
    }
}

/* ------------------------------------------------------------------ */
/*  Runtime level filtering                                            */
/* ------------------------------------------------------------------ */

extern "C" void AxLogSetLevel(int Level)
{
    g_RuntimeLogLevel = Level;
}

extern "C" int AxLogGetRuntimeLevel(void)
{
    return (g_RuntimeLogLevel);
}

/* ------------------------------------------------------------------ */
/*  File output via Platform API                                       */
/* ------------------------------------------------------------------ */

/* Writes the log header to the given FILE stream and/or the active log file */
static void WriteLogHeader(void)
{
    static constexpr const char *Header =
        "%-25s %-12s %-10s %-30s %-8s %s\n";
    static constexpr const char *Separator =
        "------------------------- ------------ ---------- ------------------------------ -------- ----------------------------------------\n";

    char HeaderLine[256];
    snprintf(HeaderLine, sizeof(HeaderLine), Header,
        "[Date/Time]", "[Uptime]", "[Thread]", "[Source]", "[Level]", "[Message]");

    /* Write to console */
    fprintf(stderr, "%s%s", HeaderLine, Separator);

    /* Write to file if active */
    if (g_LogFileActive && PlatformAPI != nullptr && PlatformAPI->FileAPI != nullptr)
    {
        PlatformAPI->FileAPI->Write(g_LogFile, (void *)HeaderLine, (uint32_t)strlen(HeaderLine));
        PlatformAPI->FileAPI->Write(g_LogFile, (void *)Separator, (uint32_t)strlen(Separator));
        PlatformAPI->FileAPI->Flush(g_LogFile);
    }
}

extern "C" void AxLogOpenFile(const char *Path)
{
    if (Path == nullptr || PlatformAPI == nullptr || PlatformAPI->FileAPI == nullptr)
    {
        return;
    }

    /* Close any previously open log file */
    if (g_LogFileActive)
    {
        AxLogCloseFile();
    }

    g_LogFile = PlatformAPI->FileAPI->OpenForWrite(Path);
    if (PlatformAPI->FileAPI->IsValid(g_LogFile))
    {
        g_LogFileActive = true;
    }

    /* Record the uptime epoch */
    if (PlatformAPI->TimeAPI != nullptr)
    {
        g_UptimeEpoch = PlatformAPI->TimeAPI->WallTime();
        g_UptimeEpochSet = true;
    }

    /* Write column header to console and file */
    WriteLogHeader();
}

extern "C" void AxLogCloseFile(void)
{
    if (g_LogFileActive && PlatformAPI != nullptr && PlatformAPI->FileAPI != nullptr)
    {
        PlatformAPI->FileAPI->Close(g_LogFile);
    }

    g_LogFile = { 0 };
    g_LogFileActive = false;
}
