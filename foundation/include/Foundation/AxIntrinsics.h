#pragma once

// TODO(mdeforge): Convert to platform efficient versions and remove math.h

#include "Foundation/AxTypes.h"
#include <math.h>
#include <immintrin.h>

#ifdef __cplusplus
extern "C" {
#endif

inline int32_t RoundFloatToInt32(float f)
{
    return (_mm_cvtss_si32(_mm_set_ss(f)));
}

inline float Cos(float Angle)
{
    return (cosf(Angle));
}

inline float Sin(float Angle)
{
    return (sinf(Angle));
}

inline float ATan2(float Y, float X)
{
    return (atan2f(Y, X));
}

#ifdef __cplusplus
}
#endif