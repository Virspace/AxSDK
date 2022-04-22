#pragma once

inline size_t RoundSizeToNearestMultiple(size_t Size, size_t Multiple)
{
    return ((Size + Multiple - 1) & ~(Multiple - 1));
}

inline size_t CalcPadding(const size_t BaseAddress, const size_t Alignment)
{
    //const size_t Mult = (BaseAddress / Alignment) + 1;

}