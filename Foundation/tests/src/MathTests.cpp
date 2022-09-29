#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxMath.h"

TEST(Math, RandomFloatFunction)
{
    float RandomNum = RandomFloat(0.0f, 1.0f);
    EXPECT_GE(RandomNum, 0.0f);
    EXPECT_LE(RandomNum, 1.0f);
}

TEST(Math, IsPowerOfTwo)
{
    EXPECT_EQ(IsPowerOfTwo(3), false);
    EXPECT_EQ(IsPowerOfTwo(4), true);
}

TEST(Math, RoundUpToPowerOfTwo)
{
    // Value is zero case
    EXPECT_EQ(RoundUpToPowerOfTwo(0, 16), 0);

    // Multiple is zero case
    EXPECT_EQ(RoundUpToPowerOfTwo(7, 0), 0);

    // Value is less than multiple case
    EXPECT_EQ(RoundUpToPowerOfTwo(7, 16), 16);

    // Value is more than twice as small as multiple case
    EXPECT_EQ(RoundUpToPowerOfTwo(7, 32), 32);

    // Value is greater than the multiple case
    EXPECT_EQ(RoundUpToPowerOfTwo(17, 8), 24);

    // Multiple is less than not a power of two case
    EXPECT_EQ(RoundUpToPowerOfTwo(7, 9), 0);

    // Multiple is greater than not a power of two case
    EXPECT_EQ(RoundUpToPowerOfTwo(13, 9), 0);
}

TEST(Math, RoundDownToPowerOfTwo)
{
    // Value is zero case
    EXPECT_EQ(RoundDownToPowerOfTwo(0, 16), 0);

    // Multiple is zero case
    EXPECT_EQ(RoundDownToPowerOfTwo(18, 0), 0);

    // Value is greater than multiple case
    EXPECT_EQ(RoundDownToPowerOfTwo(18, 16), 16);

    // Value is more than twice as small as multiple case
    EXPECT_EQ(RoundDownToPowerOfTwo(31, 16), 16);

    // Value is smaller than the multiple case
    EXPECT_EQ(RoundDownToPowerOfTwo(3, 16), 0);

    // Multiple is less than not a power of two case
    EXPECT_EQ(RoundDownToPowerOfTwo(5, 13), 0);

    // Multiple is greater than not a power of two case
    EXPECT_EQ(RoundDownToPowerOfTwo(18, 13), 0);
}