#pragma once

// TODO(mdeforge): Convert to platform efficient versions and remove math.h

#include "Foundation/AxTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

float Cos(float Angle);
float Sin(float Angle);
float ATan2(float Y, float X);
int32_t RoundFloatToInt32(float f);

#ifdef __cplusplus
}
#endif