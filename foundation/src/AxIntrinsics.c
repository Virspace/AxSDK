#include "Foundation/AxIntrinsics.h"
#include <math.h>

float Cos(float Angle)
{
    return cosf(Angle);
}

float Sin(float Angle)
{
    return sinf(Angle);
}

float ATan2(float Y, float X)
{
    return atan2f(Y, X);
}

int32_t RoundFloatToInt32(float f)
{
    return (int32_t)(f + 0.5f);
}