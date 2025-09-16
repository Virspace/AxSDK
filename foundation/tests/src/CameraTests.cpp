#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxMath.h"

TEST(CameraTest, OrthographicProjection)
{
    AxMat4x4 Result = CalcOrthographicProjection(0.0f, 800.0f, 0.0f, 600.0f, 0.1f, 100.0f);

    // Column-major layout:
    // Column 0 (X scale and transformation)
    ASSERT_FLOAT_EQ(Result.E[0][0], 0.0024999999441206455);
    ASSERT_FLOAT_EQ(Result.E[0][1], 0.0);
    ASSERT_FLOAT_EQ(Result.E[0][2], 0.0);
    ASSERT_FLOAT_EQ(Result.E[0][3], 0.0);

    // Column 1 (Y scale and transformation)
    ASSERT_FLOAT_EQ(Result.E[1][0], 0.0);
    ASSERT_FLOAT_EQ(Result.E[1][1], 0.0033333334140479565);
    ASSERT_FLOAT_EQ(Result.E[1][2], 0.0);
    ASSERT_FLOAT_EQ(Result.E[1][3], 0.0);

    // Column 2 (Z scale and transformation)
    ASSERT_FLOAT_EQ(Result.E[2][0], 0.0);
    ASSERT_FLOAT_EQ(Result.E[2][1], 0.0);
    ASSERT_FLOAT_EQ(Result.E[2][2], 0.010009999275207519);
    ASSERT_FLOAT_EQ(Result.E[2][3], 0.0);

    // Column 3 (Translation and projection)
    ASSERT_FLOAT_EQ(Result.E[3][0], -1.0);
    ASSERT_FLOAT_EQ(Result.E[3][1], -1.0);
    ASSERT_FLOAT_EQ(Result.E[3][2], -0.001000999985076487);
    ASSERT_FLOAT_EQ(Result.E[3][3], 1.0);
}