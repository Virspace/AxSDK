#pragma once

/*
 * AxLog - Lightweight logging library for AxSDK
 *
 * Usage:
 *   #include "AxLog/AxLog.h"
 *   AX_LOG(INFO, "Loaded shader: %s", path);
 *   AX_LOG_IF(WARNING, health < 20, "Player health low: %d", health);
 */

/* Severity levels */
#define AX_LOG_LEVEL_TRACE   0
#define AX_LOG_LEVEL_DEBUG   1
#define AX_LOG_LEVEL_INFO    2
#define AX_LOG_LEVEL_WARNING 3
#define AX_LOG_LEVEL_ERROR   4
#define AX_LOG_LEVEL_FATAL   5
#define AX_LOG_LEVEL_OFF     6

/* Compile-time level threshold -- define before including this header to override */
#ifndef AX_LOG_COMPILE_LEVEL
#define AX_LOG_COMPILE_LEVEL AX_LOG_LEVEL_TRACE
#endif

/* DLL export/import -- in shipping or static-link mode, no decorator needed */
#if defined(AX_SHIPPING) || defined(AXLOG_STATIC)
  #define AXLOG_API
#elif defined(AXLOG_EXPORTS)
  #ifdef _WIN32
    #define AXLOG_API __declspec(dllexport)
  #else
    #define AXLOG_API __attribute__((visibility("default")))
  #endif
#else
  #ifdef _WIN32
    #define AXLOG_API __declspec(dllimport)
  #else
    #define AXLOG_API
  #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Writes a formatted log message. Called by the AX_LOG and AX_LOG_IF macros.
 * @param Level    One of AX_LOG_LEVEL_TRACE .. AX_LOG_LEVEL_FATAL.
 * @param File     Source file path (typically __FILE__).
 * @param Line     Source line number (typically __LINE__).
 * @param Fmt      printf-style format string.
 */
AXLOG_API void AxLogWrite(int Level, const char *File, int Line, const char *Fmt, ...);

/**
 * Sets the runtime minimum log level. Messages below this level are suppressed.
 * @param Level    One of AX_LOG_LEVEL_TRACE .. AX_LOG_LEVEL_OFF.
 */
AXLOG_API void AxLogSetLevel(int Level);

/**
 * Returns the current runtime minimum log level.
 */
AXLOG_API int AxLogGetRuntimeLevel(void);

/**
 * Opens a log file for output using the Platform FileAPI.
 * Also records the current wall clock as the uptime epoch.
 * @param Path     File path to open for writing.
 */
AXLOG_API void AxLogOpenFile(const char *Path);

/**
 * Closes the currently open log file.
 */
AXLOG_API void AxLogCloseFile(void);

#ifdef __cplusplus
}
#endif

/*
 * Logging macros -- these live outside the extern "C" block because
 * they are preprocessor constructs, not declarations.
 *
 * AX_LOG(LEVEL, fmt, ...)
 *   Logs a message at the given severity level.
 *   Compile-time stripping: expands to ((void)0) when
 *   AX_LOG_LEVEL_##LEVEL < AX_LOG_COMPILE_LEVEL.
 *   Runtime filtering: the if-guard checks the runtime level so
 *   arguments are never evaluated when the message would be suppressed.
 *
 * AX_LOG_IF(LEVEL, cond, fmt, ...)
 *   Like AX_LOG but only logs when 'cond' evaluates to true.
 */

/*
 * NOTE: The ## token-paste operator is placed directly adjacent to LEVEL
 * in these macros (AX_LOG_LEVEL_##LEVEL) rather than through an intermediate
 * macro. This is critical because Windows <Windows.h> defines ERROR as 0.
 * When ## is directly adjacent to a macro parameter, the C standard
 * guarantees the parameter is NOT expanded before pasting. Without this,
 * AX_LOG(ERROR, ...) would expand ERROR to 0, producing AX_LOG_LEVEL_0.
 */

#if defined(_MSC_VER)
/* MSVC does not support zero-length __VA_ARGS__ without the /Zc:preprocessor flag,
   so we use a different expansion trick. */
#define AX_LOG(LEVEL, FMT, ...) \
    do { \
        if (AX_LOG_LEVEL_##LEVEL >= AX_LOG_COMPILE_LEVEL && \
            AX_LOG_LEVEL_##LEVEL >= AxLogGetRuntimeLevel()) \
        { \
            AxLogWrite(AX_LOG_LEVEL_##LEVEL, __FILE__, __LINE__, (FMT), __VA_ARGS__); \
        } \
    } while (0)

#define AX_LOG_IF(LEVEL, COND, FMT, ...) \
    do { \
        if (AX_LOG_LEVEL_##LEVEL >= AX_LOG_COMPILE_LEVEL && \
            AX_LOG_LEVEL_##LEVEL >= AxLogGetRuntimeLevel() && \
            (COND)) \
        { \
            AxLogWrite(AX_LOG_LEVEL_##LEVEL, __FILE__, __LINE__, (FMT), __VA_ARGS__); \
        } \
    } while (0)
#else
/* GCC / Clang: use ##__VA_ARGS__ to allow zero variadic arguments */
#define AX_LOG(LEVEL, FMT, ...) \
    do { \
        if (AX_LOG_LEVEL_##LEVEL >= AX_LOG_COMPILE_LEVEL && \
            AX_LOG_LEVEL_##LEVEL >= AxLogGetRuntimeLevel()) \
        { \
            AxLogWrite(AX_LOG_LEVEL_##LEVEL, __FILE__, __LINE__, (FMT), ##__VA_ARGS__); \
        } \
    } while (0)

#define AX_LOG_IF(LEVEL, COND, FMT, ...) \
    do { \
        if (AX_LOG_LEVEL_##LEVEL >= AX_LOG_COMPILE_LEVEL && \
            AX_LOG_LEVEL_##LEVEL >= AxLogGetRuntimeLevel() && \
            (COND)) \
        { \
            AxLogWrite(AX_LOG_LEVEL_##LEVEL, __FILE__, __LINE__, (FMT), ##__VA_ARGS__); \
        } \
    } while (0)
#endif
