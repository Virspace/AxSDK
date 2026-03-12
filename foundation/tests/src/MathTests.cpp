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

TEST(Math, QuatEulerConversionRoundTrip)
{
    // Test that converting Euler -> Quat -> Euler gives back the original angles
    AxVec3 OriginalEuler = { -8.0f * (AX_PI / 180.0f), 45.0f * (AX_PI / 180.0f), 0.0f };

    AxQuat Quat = QuatFromEuler(OriginalEuler);
    AxVec3 ConvertedEuler = QuatToEuler(Quat);

    printf("Original Euler: Pitch=%.6f, Yaw=%.6f, Roll=%.6f\n",
           OriginalEuler.X * (180.0f / AX_PI), OriginalEuler.Y * (180.0f / AX_PI), OriginalEuler.Z * (180.0f / AX_PI));
    printf("Quaternion: X=%.6f, Y=%.6f, Z=%.6f, W=%.6f\n",
           Quat.X, Quat.Y, Quat.Z, Quat.W);
    printf("Converted Euler: Pitch=%.6f, Yaw=%.6f, Roll=%.6f\n",
           ConvertedEuler.X * (180.0f / AX_PI), ConvertedEuler.Y * (180.0f / AX_PI), ConvertedEuler.Z * (180.0f / AX_PI));

    // Allow small tolerance for floating point precision
    EXPECT_NEAR(OriginalEuler.X, ConvertedEuler.X, 0.001f);
    EXPECT_NEAR(OriginalEuler.Y, ConvertedEuler.Y, 0.001f);
    EXPECT_NEAR(OriginalEuler.Z, ConvertedEuler.Z, 0.001f);
}

TEST(Math, QuatZeroRollConversion)
{
    // Test specific case: quaternion with Z=0 should give Roll=0
    AxQuat TestQuat = { .X = -0.054863f, .Y = 0.382683f, .Z = 0.000000f, .W = 0.922297f };
    AxVec3 EulerResult = QuatToEuler(TestQuat);

    printf("Test Quat: X=%.6f, Y=%.6f, Z=%.6f, W=%.6f\n",
           TestQuat.X, TestQuat.Y, TestQuat.Z, TestQuat.W);
    printf("Result Euler: Pitch=%.6f, Yaw=%.6f, Roll=%.6f\n",
           EulerResult.X * (180.0f / AX_PI), EulerResult.Y * (180.0f / AX_PI), EulerResult.Z * (180.0f / AX_PI));

    // Roll should be very close to zero when quaternion Z component is zero
    EXPECT_NEAR(EulerResult.Z, 0.0f, 0.001f);
}

TEST(Math, QuatFromEulerZeroRoll)
{
    // Test identity case first
    AxVec3 IdentityEuler = { .X = 0.0f, .Y = 0.0f, .Z = 0.0f };
    AxQuat IdentityQuat = QuatFromEuler(IdentityEuler);
    printf("Identity Euler: Pitch=%.6f, Yaw=%.6f, Roll=%.6f\n", 0.0f, 0.0f, 0.0f);
    printf("Identity Quat: X=%.6f, Y=%.6f, Z=%.6f, W=%.6f\n",
           IdentityQuat.X, IdentityQuat.Y, IdentityQuat.Z, IdentityQuat.W);
    EXPECT_NEAR(IdentityQuat.X, 0.0f, 0.001f);
    EXPECT_NEAR(IdentityQuat.Y, 0.0f, 0.001f);
    EXPECT_NEAR(IdentityQuat.Z, 0.0f, 0.001f);
    EXPECT_NEAR(IdentityQuat.W, 1.0f, 0.001f);

    // Test that QuatFromEuler with Roll=0 produces Z component close to 0
    AxVec3 EulerInput = { .X = -8.0f * (AX_PI / 180.0f), .Y = 45.0f * (AX_PI / 180.0f), .Z = 0.0f };
    AxQuat QuatResult = QuatFromEuler(EulerInput);

    printf("Input Euler: Pitch=%.6f, Yaw=%.6f, Roll=%.6f\n",
           EulerInput.X * (180.0f / AX_PI), EulerInput.Y * (180.0f / AX_PI), EulerInput.Z * (180.0f / AX_PI));
    printf("Result Quat: X=%.6f, Y=%.6f, Z=%.6f, W=%.6f\n",
           QuatResult.X, QuatResult.Y, QuatResult.Z, QuatResult.W);

    // Z component should be very close to zero when Roll=0
    EXPECT_NEAR(QuatResult.Z, 0.0f, 0.001f);
}
