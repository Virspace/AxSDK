#pragma once

inline size_t Align(size_t N)
{
    return (N + sizeof(intptr_t) - 1) & ~(sizeof(intptr_t) - 1);
}

inline size_t RoundSizeToNearestPageMultiple(size_t Value, size_t Multiple)
{
    size_t Remainder = Value % Multiple;
    if (Remainder == 0) {
        return Value;
    }

    return (Value + Multiple - Remainder);
}