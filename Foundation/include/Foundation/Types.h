#pragma once

#if !defined(COMPILER_MSVC)
#define COMPILER_MSVC 0
#endif

#if !defined(COMPILER_LLVM)
#define COMPILER_LLVM 0
#endif

#if !COMPILER_MVC && !COMPILER_LLVM
#if _MSC_VER
#undef COMPILER_MSVC
#define COMPILER_MSVC 1
#else
// TODO(mdeforge): Add more compilers here
#undef COMPILER_LLVM
#define COMPILER_LLVM 1
#endif
#endif

#if COMPILER_MSVC
#include <intrin.h>
#else
#error SSE optimizations are not available for this compiler yet!
#endif

#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define AXON_DLL_EXPORT __declspec(dllexport)

#define Pi32 3.14159265359f

//#if AXON_SLOW
#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
//#else
//#define Assert(Expression)
//#endif

#define AXON_ASSERT(Expression) Assert(Expression)

#define AXON_UNUSED(Variable) ((void)(Variable)) // Used to silence "unused variable warnings"

#define SAFE_DELETE(a) if( (a) != NULL ) delete (a); (a) = NULL;

#define Kilobytes(Value) ((Value) * 1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

// TODO(mdeforge): Move to utilities
#define FOURCC(String) (((u32)(String[0]) << 0) | ((u32)(String[1]) << 8) | ((u32)(String[2]) << 16) | ((u32)(String[3]) << 24))

typedef struct AxRect
{
    int32_t Left;
    int32_t Top;
    int32_t Right;
    int32_t Bottom;
} AxRect;

typedef union AxVec2
{
    struct
    {
        double X, Y;
    };

    struct
    {
        double U, V;
    };

    double XY[2];
} AxVec2;

typedef union AxVec3
{
    struct
    {
        double X, Y, Z;
    };

    struct
    {
        double U, V, W;
    };

    struct
    {
        double R, G, B;
    };

    double XYZ[3];
} AxVec3;

typedef struct AxTransform
{
    AxVec3 Position;
    AxVec3 Rotation;
    AxVec3 Scale;
} AxTransform;

typedef union AxVec4
{
  struct
  {
    union
    {
        AxVec3 XYZ;
        struct
        {
            double X, Y, Z;
        };
    };

    double W;
  };

  double E[4];
} AxVec4;

typedef struct AxMat4x4
{
    double E[4][4]; // ROW MAJOR!
} AxMat4x4;

typedef struct AxMat4x4Inv
{
    AxMat4x4 Forward;
    AxMat4x4 Inverse;
} AxMat4x4Inv;

// Total number of ticks that have occurred since the OS started
typedef struct AxWallClock
{
    int64_t Ticks;
} AxWallClock;

// Represents a time from the system clock.
typedef struct AxClock
{
    int64_t TimeStamp;
} AxClock;

// Banned functions

/*
 * This lists functions that have been banned from our code base,
 * because they're too easy to misuse (and even if used correctly,
 * complicate audits). Including this turns them into compile-time
 * errors.
 */

// #define BANNED(func) sorry_##func##_is_a_banned_function

// #undef strcpy
// #define strcpy(x,y) BANNED(strcpy)
// #undef strcat
// #define strcat(x,y) BANNED(strcat)
// #undef strncpy
// #define strncpy(x,y,n) BANNED(strncpy)
// #undef strncat
// #define strncat(x,y,n) BANNED(strncat)

// #undef sprintf
// #undef vsprintf
// #ifdef HAVE_VARIADIC_MACROS
// #define sprintf(...) BANNED(sprintf)
// #define vsprintf(...) BANNED(vsprintf)
// #else
// #define sprintf(buf,fmt,arg) BANNED(sprintf)
// #define vsprintf(buf,fmt,arg) BANNED(vsprintf)
// #endif

// #undef gmtime
// #define gmtime(t) BANNED(gmtime)
// #undef localtime
// #define localtime(t) BANNED(localtime)
// #undef ctime
// #define ctime(t) BANNED(ctime)
// #undef ctime_r
// #define ctime_r(t, buf) BANNED(ctime_r)
// #undef asctime
// #define asctime(t) BANNED(asctime)
// #undef asctime_r
// #define asctime_r(t, buf) BANNED(asctime_r)