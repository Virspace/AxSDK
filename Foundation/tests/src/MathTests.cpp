#include "gtest/gtest.h"
#include "Foundation/Types.h"

extern "C"
{
    #include "Foundation/Math.h"
}

TEST(Math, RandomFloatFunction)
{
    float RandomNum = RandomFloat(0.0f, 1.0f);
    EXPECT_GE(RandomNum, 0.0f);
    EXPECT_LE(RandomNum, 1.0f);
}