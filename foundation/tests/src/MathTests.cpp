#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxMath.h"

TEST(Math, RandomFloatFunction)
{
    float RandomNum = RandomFloat(0.0f, 1.0f);
    EXPECT_GE(RandomNum, 0.0f);
    EXPECT_LE(RandomNum, 1.0f);
}