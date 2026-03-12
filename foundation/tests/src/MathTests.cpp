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

TEST(Math, QuatIdentityRoundTrip)
{
    // Identity quaternion should produce zero Euler angles
    AxVec3 IdentityEuler = { .X = 0.0f, .Y = 0.0f, .Z = 0.0f };
    AxQuat IdentityQuat = QuatFromEuler(IdentityEuler);
    EXPECT_NEAR(IdentityQuat.X, 0.0f, 0.001f);
    EXPECT_NEAR(IdentityQuat.Y, 0.0f, 0.001f);
    EXPECT_NEAR(IdentityQuat.Z, 0.0f, 0.001f);
    EXPECT_NEAR(IdentityQuat.W, 1.0f, 0.001f);

    AxVec3 BackToEuler = QuatToEuler(IdentityQuat);
    EXPECT_NEAR(BackToEuler.X, 0.0f, 0.001f);
    EXPECT_NEAR(BackToEuler.Y, 0.0f, 0.001f);
    EXPECT_NEAR(BackToEuler.Z, 0.0f, 0.001f);
}

TEST(Math, QuatSingleAxisRoundTrips)
{
    // Pitch only (X-axis rotation)
    AxVec3 PitchOnly = { .X = 30.0f * (AX_PI / 180.0f), .Y = 0.0f, .Z = 0.0f };
    AxVec3 PitchResult = QuatToEuler(QuatFromEuler(PitchOnly));
    EXPECT_NEAR(PitchOnly.X, PitchResult.X, 0.001f);
    EXPECT_NEAR(PitchOnly.Y, PitchResult.Y, 0.001f);
    EXPECT_NEAR(PitchOnly.Z, PitchResult.Z, 0.001f);

    // Yaw only (Y-axis rotation)
    AxVec3 YawOnly = { .X = 0.0f, .Y = 45.0f * (AX_PI / 180.0f), .Z = 0.0f };
    AxVec3 YawResult = QuatToEuler(QuatFromEuler(YawOnly));
    EXPECT_NEAR(YawOnly.X, YawResult.X, 0.001f);
    EXPECT_NEAR(YawOnly.Y, YawResult.Y, 0.001f);
    EXPECT_NEAR(YawOnly.Z, YawResult.Z, 0.001f);

    // Roll only (Z-axis rotation)
    AxVec3 RollOnly = { .X = 0.0f, .Y = 0.0f, .Z = 60.0f * (AX_PI / 180.0f) };
    AxVec3 RollResult = QuatToEuler(QuatFromEuler(RollOnly));
    EXPECT_NEAR(RollOnly.X, RollResult.X, 0.001f);
    EXPECT_NEAR(RollOnly.Y, RollResult.Y, 0.001f);
    EXPECT_NEAR(RollOnly.Z, RollResult.Z, 0.001f);
}
