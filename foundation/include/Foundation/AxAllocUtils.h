#pragma once

#include "Foundation/AxTypes.h"

/**
 * Checks if a value is a power of two.
 * @param Value The value to check.
 * @return True if the Value is a power of two, otherwise false.
 */
inline bool IsPowerOfTwo(size_t Value)
{
    return ((Value & (Value - 1)) == 0);
}

/**
 * Rounds a value up to the nearest power of two multiple.
 * @param Value The value to round up.
 * @param PowerOfTwo The power of two to round up to
 * @return The nearest power of two multiple, zero if Multiple is not a power of two.
 */
inline size_t RoundUpToPowerOfTwo(size_t Value, int Multiple)
{
    if (!IsPowerOfTwo(Multiple)) {
        return (0);
    }

    return ((Value + Multiple - 1) & ~(Multiple - 1));
}

/**
 * Rounds a value down to the nearest power of two multiple.
 * @param Value The value to round down.
 * @param Multiple The power of two to round down to
 * @return The nearest power of two multiple, zero if Multiple is not a power of two.
 */
inline size_t RoundDownToPowerOfTwo(size_t Value, int Multiple)
{
    if (!IsPowerOfTwo(Multiple)) {
        return (0);
    }

    return (Value & ~(Multiple - 1));
}

inline bool IsAligned(void *Address)
{
    // NOTE(mdeforge): The cast to uintptr_t is needed because the standard only
    // guarantees an invertible conversion to uintptr_t for void *.
    return (((uintptr_t)Address & (sizeof(void *) - 1)) == 0);
}

inline void *AlignAddress(void *Address)
{
    // NOTE(mdeforge): The cast to uintptr_t is needed because the standard only
    // guarantees an invertible conversion to uintptr_t for void *.
    return ((void *)RoundUpToPowerOfTwo((size_t)Address, sizeof(void *)));
}