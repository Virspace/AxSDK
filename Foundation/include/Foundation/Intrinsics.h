#pragma once

// TODO(mdeforge): Convert to platform efficient versions and remove math.h

#include "Foundation/Types.h"
#include <math.h>

inline int32_t RoundFloatToInt32(float f)
{
    return (_mm_cvtss_si32(_mm_set_ss(f));
}

inline double Cos(double Angle)
{
    return (cos(Angle));
}

inline double Sin(double Angle)
{
    return(sin(Angle));
}

inline double ATan2(double Y, double X)
{
    return(atan2(Y, X));
}