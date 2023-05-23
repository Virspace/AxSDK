#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxCamera.h"

TEST(CameraTest, OrthographicProjection)
{
    AxMat4x4f Result = CameraAPI->CalcOrthographicProjection(0.0f, 800.0f, 0.0f, 600.0f, 0.1f, 100.0f);

    ASSERT_FLOAT_EQ(Result.E[0][0], 0.0024999999441206455);
    ASSERT_FLOAT_EQ(Result.E[0][1], 0.0);
    ASSERT_FLOAT_EQ(Result.E[0][2], 0.0);
    ASSERT_FLOAT_EQ(Result.E[0][3], 0.0);

    ASSERT_FLOAT_EQ(Result.E[1][0], 0.0);
    ASSERT_FLOAT_EQ(Result.E[1][1], 0.0033333334140479565);
    ASSERT_FLOAT_EQ(Result.E[1][2], 0.0);
    ASSERT_FLOAT_EQ(Result.E[1][3], 0.0);

    ASSERT_FLOAT_EQ(Result.E[2][0], 0.0);
    ASSERT_FLOAT_EQ(Result.E[2][1], 0.0);
    ASSERT_FLOAT_EQ(Result.E[2][2], -0.020020019263029099);
    ASSERT_FLOAT_EQ(Result.E[2][3], 0.0);

    ASSERT_FLOAT_EQ(Result.E[3][0], -1.0);
    ASSERT_FLOAT_EQ(Result.E[3][1], -1.0);
    ASSERT_FLOAT_EQ(Result.E[3][2], -1.0020020008087158);
    ASSERT_FLOAT_EQ(Result.E[3][3], 1.0);
}