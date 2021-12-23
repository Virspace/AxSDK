#pragma once

#include <math.h>

inline bool
StringAreEqual(char *A, char *B)
{
    bool Result = (A == B);
    if (A && B)
    {
        while (*A && *B && (*A == *B))
        {
            ++A;
            ++B;
        }

        Result = ((*A == 0) && (*B == 0));
    }

    return(Result);
}

inline bool
IsEndOfLine(char C)
{
    bool Result = ((C == '\n') ||
                     (C == '\r'));

    return(Result);
}

inline bool
IsWhitespace(char C)
{
    bool Result = ((C == ' ') ||
                     (C == '\t') ||
                     (C == '\v') ||
                     (C == '\f') ||
                     IsEndOfLine(C));

    return(Result);
}

inline bool
StringsAreEqual(uintptr_t ALength, char *A, char *B)
{
    char *At = B;
    for (uintptr_t Index = 0; Index < ALength; ++Index, ++At)
    {
        if((*At == 0) || (A[Index] != *At))
        {
            return(false);
        }
    }

    bool Result = (*At == 0);
    return(Result);
}