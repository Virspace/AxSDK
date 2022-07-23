#pragma once

inline size_t RoundDownToPowerOfTwo(size_t Value, int Multiple)
{
    return (Value & ~(Multiple - 1));
}

inline size_t RoundUpToPowerOfTwo(size_t Value, int Multiple)
{
    return ((Value + Multiple - 1) & ~(Multiple - 1));
}