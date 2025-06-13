#pragma once

#ifdef __cplusplus
extern "C" {
#endif

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

#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define AXON_DLL_EXPORT __declspec(dllexport)

#define AX_PI 3.14159265359f

//#if AXON_SLOW
#if defined(__clang__)
#define AXON_ASSERT(Expression) if(!(Expression)) { __builtin_trap(); }
#else
#define AXON_ASSERT(Expression) if(!(Expression)) { *(volatile int *)0 = 0; }
#endif

#define AXON_UNUSED(Variable) ((void)(Variable)) // Used to silence "unused variable warnings"

#define SAFE_FREE(a) if( (a) != NULL ) free(a); (a) = NULL;

#define Kilobytes(Value) ((Value) * 1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

// TODO(mdeforge): Move to utilities
#define FOURCC(String) (((u32)(String[0]) << 0) | ((u32)(String[1]) << 8) | ((u32)(String[2]) << 16) | ((u32)(String[3]) << 24))

typedef struct AxRect
{
    float Left;
    float Top;
    float Right;
    float Bottom;
} AxRect;

typedef union AxVec2
{
    struct
    {
        float X, Y;
    };

    struct
    {
        float U, V;
    };

    struct
    {
        float Width, Height;
    };

    struct
    {
        float Min, Max;
    };

    float XY[2];
} AxVec2;

typedef union AxVec3
{
    struct
    {
        float X, Y, Z;
    };

    struct
    {
        float U, V, W;
    };

    struct
    {
        float R, G, B;
    };

    float XYZ[3];
} AxVec3;

typedef union AxVec4
{
  struct
  {
    union
    {
        AxVec3 XYZ;
        struct
        {
            float X, Y, Z;
        };
    };

    float W;
  };

  struct
  {
    union
    {
        float R, G, B, A;
    };

    float RGBA[4];
  };

  float E[4];
} AxVec4;

// A column major 4x4 matrix
typedef struct AxMat4x4
{
    float E[4][4]; // E[column][row]
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

typedef struct AxTransform
{
    AxVec3 Position;
    AxVec3 Rotation;
    AxVec3 Scale;
} AxTransform;

typedef enum AxTupleType { AX_TUPLE_INT, AX_TUPLE_FLOAT, AX_TUPLE_STRING } AxTupleType;

typedef struct AxTupleItem
{
    AxTupleType Type;
    union
    {
        int ValInt;
        float ValFloat;
        char *ValString;
    };
} AxTupleItem;

// TODO(mdeforge): Consider moving this outside of Types.h so that we can use an AxArray to back this
typedef struct AxTuple
{
    size_t NumItems;
    AxTupleItem *Items;
} AxTuple;

#ifdef __cplusplus
}
#endif
