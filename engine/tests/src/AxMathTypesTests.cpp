/**
 * AxMathTypesTests.cpp - Tests for C++ math wrapper types (Vec2, Vec3, Vec4)
 *
 * Comprehensive tests for Vec3, abbreviated coverage for Vec2 and Vec4.
 * Verifies construction, conversion, operators, methods, and layout compatibility.
 */

#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "Foundation/AxMath.h"
#include "AxEngine/AxMathTypes.h"

#include <cmath>

//=============================================================================
// Vec3 Tests - Construction and Conversion
//=============================================================================

TEST(Vec3Test, DefaultConstructor_AllZero)
{
  Vec3 V;
  EXPECT_FLOAT_EQ(V.X, 0.0f);
  EXPECT_FLOAT_EQ(V.Y, 0.0f);
  EXPECT_FLOAT_EQ(V.Z, 0.0f);
}

TEST(Vec3Test, ComponentConstructor_SetsXYZ)
{
  Vec3 V(1.0f, 2.0f, 3.0f);
  EXPECT_FLOAT_EQ(V.X, 1.0f);
  EXPECT_FLOAT_EQ(V.Y, 2.0f);
  EXPECT_FLOAT_EQ(V.Z, 3.0f);
}

TEST(Vec3Test, ScalarConstructor_BroadcastsValue)
{
  Vec3 V(5.0f);
  EXPECT_FLOAT_EQ(V.X, 5.0f);
  EXPECT_FLOAT_EQ(V.Y, 5.0f);
  EXPECT_FLOAT_EQ(V.Z, 5.0f);
}

TEST(Vec3Test, FromAxVec3_ConvertsCorrectly)
{
  AxVec3 Ax = {1.0f, 2.0f, 3.0f};
  Vec3 V(Ax);
  EXPECT_FLOAT_EQ(V.X, 1.0f);
  EXPECT_FLOAT_EQ(V.Y, 2.0f);
  EXPECT_FLOAT_EQ(V.Z, 3.0f);
}

TEST(Vec3Test, ToAxVec3_ConvertsCorrectly)
{
  Vec3 V(4.0f, 5.0f, 6.0f);
  AxVec3 Ax = V;
  EXPECT_FLOAT_EQ(Ax.X, 4.0f);
  EXPECT_FLOAT_EQ(Ax.Y, 5.0f);
  EXPECT_FLOAT_EQ(Ax.Z, 6.0f);
}

TEST(Vec3Test, RoundTrip_PreservesValues)
{
  AxVec3 Original = {7.5f, -3.2f, 1.1f};
  Vec3 V(Original);
  AxVec3 Result = V;
  EXPECT_FLOAT_EQ(Result.X, Original.X);
  EXPECT_FLOAT_EQ(Result.Y, Original.Y);
  EXPECT_FLOAT_EQ(Result.Z, Original.Z);
}

TEST(Vec3Test, LayoutCompatible_SameSizeAndAlignment)
{
  // These are also checked via static_assert in the header,
  // but we verify at runtime too
  EXPECT_EQ(sizeof(Vec3), sizeof(AxVec3));
  EXPECT_EQ(sizeof(Vec3), 3 * sizeof(float));
}

//=============================================================================
// Vec3 Tests - Arithmetic Operators
//=============================================================================

TEST(Vec3Test, Add_ComponentWise)
{
  Vec3 A(1, 2, 3);
  Vec3 B(4, 5, 6);
  Vec3 R = A + B;
  EXPECT_FLOAT_EQ(R.X, 5.0f);
  EXPECT_FLOAT_EQ(R.Y, 7.0f);
  EXPECT_FLOAT_EQ(R.Z, 9.0f);
}

TEST(Vec3Test, Sub_ComponentWise)
{
  Vec3 A(5, 7, 9);
  Vec3 B(1, 2, 3);
  Vec3 R = A - B;
  EXPECT_FLOAT_EQ(R.X, 4.0f);
  EXPECT_FLOAT_EQ(R.Y, 5.0f);
  EXPECT_FLOAT_EQ(R.Z, 6.0f);
}

TEST(Vec3Test, MulScalar_ScalesAll)
{
  Vec3 V(1, 2, 3);
  Vec3 R = V * 2.0f;
  EXPECT_FLOAT_EQ(R.X, 2.0f);
  EXPECT_FLOAT_EQ(R.Y, 4.0f);
  EXPECT_FLOAT_EQ(R.Z, 6.0f);
}

TEST(Vec3Test, ScalarMul_Commutative)
{
  Vec3 V(1, 2, 3);
  Vec3 R = 2.0f * V;
  EXPECT_FLOAT_EQ(R.X, 2.0f);
  EXPECT_FLOAT_EQ(R.Y, 4.0f);
  EXPECT_FLOAT_EQ(R.Z, 6.0f);
}

TEST(Vec3Test, DivScalar_ScalesAll)
{
  Vec3 V(2, 4, 6);
  Vec3 R = V / 2.0f;
  EXPECT_FLOAT_EQ(R.X, 1.0f);
  EXPECT_FLOAT_EQ(R.Y, 2.0f);
  EXPECT_FLOAT_EQ(R.Z, 3.0f);
}

TEST(Vec3Test, Negate_FlipsSign)
{
  Vec3 V(1, -2, 3);
  Vec3 R = -V;
  EXPECT_FLOAT_EQ(R.X, -1.0f);
  EXPECT_FLOAT_EQ(R.Y, 2.0f);
  EXPECT_FLOAT_EQ(R.Z, -3.0f);
}

TEST(Vec3Test, AddAssign_MutatesInPlace)
{
  Vec3 V(1, 2, 3);
  V += Vec3(4, 5, 6);
  EXPECT_FLOAT_EQ(V.X, 5.0f);
  EXPECT_FLOAT_EQ(V.Y, 7.0f);
  EXPECT_FLOAT_EQ(V.Z, 9.0f);
}

TEST(Vec3Test, SubAssign_MutatesInPlace)
{
  Vec3 V(5, 7, 9);
  V -= Vec3(1, 2, 3);
  EXPECT_FLOAT_EQ(V.X, 4.0f);
  EXPECT_FLOAT_EQ(V.Y, 5.0f);
  EXPECT_FLOAT_EQ(V.Z, 6.0f);
}

TEST(Vec3Test, MulAssign_MutatesInPlace)
{
  Vec3 V(1, 2, 3);
  V *= 3.0f;
  EXPECT_FLOAT_EQ(V.X, 3.0f);
  EXPECT_FLOAT_EQ(V.Y, 6.0f);
  EXPECT_FLOAT_EQ(V.Z, 9.0f);
}

//=============================================================================
// Vec3 Tests - Equality
//=============================================================================

TEST(Vec3Test, Equal_SameValues_True)
{
  Vec3 A(1, 2, 3);
  Vec3 B(1, 2, 3);
  EXPECT_TRUE(A == B);
}

TEST(Vec3Test, Equal_DifferentValues_False)
{
  Vec3 A(1, 2, 3);
  Vec3 B(1, 2, 4);
  EXPECT_FALSE(A == B);
}

TEST(Vec3Test, NotEqual_Opposite)
{
  Vec3 A(1, 2, 3);
  Vec3 B(1, 2, 4);
  EXPECT_TRUE(A != B);
  EXPECT_FALSE(A != A);
}

//=============================================================================
// Vec3 Tests - Methods
//=============================================================================

TEST(Vec3Test, Length_UnitVector_ReturnsOne)
{
  Vec3 V(1, 0, 0);
  EXPECT_FLOAT_EQ(V.Length(), 1.0f);
}

TEST(Vec3Test, Length_KnownVector_CorrectResult)
{
  Vec3 V(3, 4, 0);
  EXPECT_FLOAT_EQ(V.Length(), 5.0f);
}

TEST(Vec3Test, LengthSquared_AvoidsSqrt)
{
  Vec3 V(3, 4, 0);
  EXPECT_FLOAT_EQ(V.LengthSquared(), 25.0f);
}

TEST(Vec3Test, Normalized_UnitLength)
{
  Vec3 V(3, 4, 0);
  Vec3 N = V.Normalized();
  EXPECT_NEAR(N.Length(), 1.0f, 1e-6f);
  EXPECT_NEAR(N.X, 0.6f, 1e-6f);
  EXPECT_NEAR(N.Y, 0.8f, 1e-6f);
  EXPECT_NEAR(N.Z, 0.0f, 1e-6f);
}

TEST(Vec3Test, Normalized_ZeroVector_HandlesGracefully)
{
  // Foundation's Vec3Normalize divides by zero length, producing inf/nan.
  // Just verify it doesn't crash.
  Vec3 V(0, 0, 0);
  Vec3 N = V.Normalized();
  (void)N; // no crash is the test
}

TEST(Vec3Test, Dot_Perpendicular_Zero)
{
  Vec3 A(1, 0, 0);
  Vec3 B(0, 1, 0);
  EXPECT_FLOAT_EQ(A.Dot(B), 0.0f);
}

TEST(Vec3Test, Dot_Parallel_Product)
{
  Vec3 A(2, 0, 0);
  Vec3 B(3, 0, 0);
  EXPECT_FLOAT_EQ(A.Dot(B), 6.0f);
}

TEST(Vec3Test, Cross_AxesProduceThird)
{
  Vec3 X(1, 0, 0);
  Vec3 Y(0, 1, 0);
  Vec3 Z = X.Cross(Y);
  EXPECT_FLOAT_EQ(Z.X, 0.0f);
  EXPECT_FLOAT_EQ(Z.Y, 0.0f);
  EXPECT_FLOAT_EQ(Z.Z, 1.0f);
}

TEST(Vec3Test, Cross_Anticommutative)
{
  Vec3 A(1, 2, 3);
  Vec3 B(4, 5, 6);
  Vec3 AB = A.Cross(B);
  Vec3 BA = B.Cross(A);
  EXPECT_FLOAT_EQ(AB.X, -BA.X);
  EXPECT_FLOAT_EQ(AB.Y, -BA.Y);
  EXPECT_FLOAT_EQ(AB.Z, -BA.Z);
}

TEST(Vec3Test, Lerp_ZeroT_ReturnsA)
{
  Vec3 A(1, 2, 3);
  Vec3 B(4, 5, 6);
  Vec3 R = A.Lerp(B, 0.0f);
  EXPECT_FLOAT_EQ(R.X, 1.0f);
  EXPECT_FLOAT_EQ(R.Y, 2.0f);
  EXPECT_FLOAT_EQ(R.Z, 3.0f);
}

TEST(Vec3Test, Lerp_OneT_ReturnsB)
{
  Vec3 A(1, 2, 3);
  Vec3 B(4, 5, 6);
  Vec3 R = A.Lerp(B, 1.0f);
  EXPECT_FLOAT_EQ(R.X, 4.0f);
  EXPECT_FLOAT_EQ(R.Y, 5.0f);
  EXPECT_FLOAT_EQ(R.Z, 6.0f);
}

TEST(Vec3Test, Lerp_HalfT_Midpoint)
{
  Vec3 A(0, 0, 0);
  Vec3 B(10, 20, 30);
  Vec3 R = A.Lerp(B, 0.5f);
  EXPECT_FLOAT_EQ(R.X, 5.0f);
  EXPECT_FLOAT_EQ(R.Y, 10.0f);
  EXPECT_FLOAT_EQ(R.Z, 15.0f);
}

//=============================================================================
// Vec3 Tests - Static Constructors
//=============================================================================

TEST(Vec3Test, Zero_AllZero)
{
  Vec3 V = Vec3::Zero();
  EXPECT_FLOAT_EQ(V.X, 0.0f);
  EXPECT_FLOAT_EQ(V.Y, 0.0f);
  EXPECT_FLOAT_EQ(V.Z, 0.0f);
}

TEST(Vec3Test, One_AllOne)
{
  Vec3 V = Vec3::One();
  EXPECT_FLOAT_EQ(V.X, 1.0f);
  EXPECT_FLOAT_EQ(V.Y, 1.0f);
  EXPECT_FLOAT_EQ(V.Z, 1.0f);
}

TEST(Vec3Test, Up_Y1)
{
  Vec3 V = Vec3::Up();
  EXPECT_FLOAT_EQ(V.X, 0.0f);
  EXPECT_FLOAT_EQ(V.Y, 1.0f);
  EXPECT_FLOAT_EQ(V.Z, 0.0f);
}

TEST(Vec3Test, Forward_NegZ)
{
  Vec3 V = Vec3::Forward();
  EXPECT_FLOAT_EQ(V.X, 0.0f);
  EXPECT_FLOAT_EQ(V.Y, 0.0f);
  EXPECT_FLOAT_EQ(V.Z, -1.0f);
}

TEST(Vec3Test, Right_X1)
{
  Vec3 V = Vec3::Right();
  EXPECT_FLOAT_EQ(V.X, 1.0f);
  EXPECT_FLOAT_EQ(V.Y, 0.0f);
  EXPECT_FLOAT_EQ(V.Z, 0.0f);
}

//=============================================================================
// Vec2 Tests (abbreviated)
//=============================================================================

TEST(Vec2Test, Construction_And_Conversion)
{
  // Default
  Vec2 A;
  EXPECT_FLOAT_EQ(A.X, 0.0f);
  EXPECT_FLOAT_EQ(A.Y, 0.0f);

  // Component
  Vec2 B(3.0f, 4.0f);
  EXPECT_FLOAT_EQ(B.X, 3.0f);
  EXPECT_FLOAT_EQ(B.Y, 4.0f);

  // From AxVec2
  AxVec2 Ax = {1.0f, 2.0f};
  Vec2 C(Ax);
  EXPECT_FLOAT_EQ(C.X, 1.0f);
  EXPECT_FLOAT_EQ(C.Y, 2.0f);

  // Round-trip
  AxVec2 Back = C;
  EXPECT_FLOAT_EQ(Back.X, 1.0f);
  EXPECT_FLOAT_EQ(Back.Y, 2.0f);

  // Layout
  EXPECT_EQ(sizeof(Vec2), sizeof(AxVec2));
}

TEST(Vec2Test, Arithmetic_Operators)
{
  Vec2 A(1, 2);
  Vec2 B(3, 4);

  Vec2 Sum = A + B;
  EXPECT_FLOAT_EQ(Sum.X, 4.0f);
  EXPECT_FLOAT_EQ(Sum.Y, 6.0f);

  Vec2 Diff = B - A;
  EXPECT_FLOAT_EQ(Diff.X, 2.0f);
  EXPECT_FLOAT_EQ(Diff.Y, 2.0f);

  Vec2 Scaled = A * 3.0f;
  EXPECT_FLOAT_EQ(Scaled.X, 3.0f);
  EXPECT_FLOAT_EQ(Scaled.Y, 6.0f);

  Vec2 ScaledCommutative = 3.0f * A;
  EXPECT_FLOAT_EQ(ScaledCommutative.X, 3.0f);
  EXPECT_FLOAT_EQ(ScaledCommutative.Y, 6.0f);

  Vec2 Neg = -A;
  EXPECT_FLOAT_EQ(Neg.X, -1.0f);
  EXPECT_FLOAT_EQ(Neg.Y, -2.0f);

  EXPECT_TRUE(A == Vec2(1, 2));
  EXPECT_TRUE(A != B);
}

TEST(Vec2Test, Length_And_Normalized)
{
  Vec2 V(3, 4);
  EXPECT_FLOAT_EQ(V.Length(), 5.0f);
  EXPECT_FLOAT_EQ(V.LengthSquared(), 25.0f);

  Vec2 N = V.Normalized();
  EXPECT_NEAR(N.Length(), 1.0f, 1e-6f);
  EXPECT_NEAR(N.X, 0.6f, 1e-6f);
  EXPECT_NEAR(N.Y, 0.8f, 1e-6f);
}

TEST(Vec2Test, Dot)
{
  Vec2 A(1, 0);
  Vec2 B(0, 1);
  EXPECT_FLOAT_EQ(A.Dot(B), 0.0f);

  Vec2 C(2, 3);
  Vec2 D(4, 5);
  EXPECT_FLOAT_EQ(C.Dot(D), 23.0f);
}

//=============================================================================
// Vec4 Tests (abbreviated)
//=============================================================================

TEST(Vec4Test, Construction_From_Vec3_And_W)
{
  Vec3 V3(1, 2, 3);
  Vec4 V4(V3, 1.0f);
  EXPECT_FLOAT_EQ(V4.X, 1.0f);
  EXPECT_FLOAT_EQ(V4.Y, 2.0f);
  EXPECT_FLOAT_EQ(V4.Z, 3.0f);
  EXPECT_FLOAT_EQ(V4.W, 1.0f);

  // Default
  Vec4 D;
  EXPECT_FLOAT_EQ(D.X, 0.0f);
  EXPECT_FLOAT_EQ(D.W, 0.0f);

  // Layout: Vec4 is 16 bytes, AxVec4 is 20 bytes (RGBA union/array)
  EXPECT_EQ(sizeof(Vec4), 4 * sizeof(float));
}

TEST(Vec4Test, Arithmetic_Operators)
{
  Vec4 A(1, 2, 3, 4);
  Vec4 B(5, 6, 7, 8);

  Vec4 Sum = A + B;
  EXPECT_FLOAT_EQ(Sum.X, 6.0f);
  EXPECT_FLOAT_EQ(Sum.W, 12.0f);

  Vec4 Scaled = A * 2.0f;
  EXPECT_FLOAT_EQ(Scaled.X, 2.0f);
  EXPECT_FLOAT_EQ(Scaled.W, 8.0f);

  Vec4 ScaledCommutative = 2.0f * A;
  EXPECT_FLOAT_EQ(ScaledCommutative.X, 2.0f);

  Vec4 Neg = -A;
  EXPECT_FLOAT_EQ(Neg.X, -1.0f);
  EXPECT_FLOAT_EQ(Neg.W, -4.0f);

  EXPECT_TRUE(A == Vec4(1, 2, 3, 4));
  EXPECT_TRUE(A != B);
}

TEST(Vec4Test, Conversion_AxVec4)
{
  Vec4 V(1, 2, 3, 4);
  AxVec4 Ax = V;
  EXPECT_FLOAT_EQ(Ax.X, 1.0f);
  EXPECT_FLOAT_EQ(Ax.Y, 2.0f);
  EXPECT_FLOAT_EQ(Ax.Z, 3.0f);
  EXPECT_FLOAT_EQ(Ax.W, 4.0f);

  // Round-trip
  Vec4 Back(Ax);
  EXPECT_TRUE(Back == V);
}

//=============================================================================
// Quat Tests - Construction and Conversion
//=============================================================================

TEST(QuatTest, DefaultConstructor_Identity)
{
  Quat Q;
  EXPECT_FLOAT_EQ(Q.X, 0.0f);
  EXPECT_FLOAT_EQ(Q.Y, 0.0f);
  EXPECT_FLOAT_EQ(Q.Z, 0.0f);
  EXPECT_FLOAT_EQ(Q.W, 1.0f);
}

TEST(QuatTest, FromAxQuat_ConvertsCorrectly)
{
  AxQuat Ax = {0.1f, 0.2f, 0.3f, 0.9f};
  Quat Q(Ax);
  EXPECT_FLOAT_EQ(Q.X, 0.1f);
  EXPECT_FLOAT_EQ(Q.Y, 0.2f);
  EXPECT_FLOAT_EQ(Q.Z, 0.3f);
  EXPECT_FLOAT_EQ(Q.W, 0.9f);
}

TEST(QuatTest, ToAxQuat_ConvertsCorrectly)
{
  Quat Q(0.1f, 0.2f, 0.3f, 0.9f);
  AxQuat Ax = Q;
  EXPECT_FLOAT_EQ(Ax.X, 0.1f);
  EXPECT_FLOAT_EQ(Ax.Y, 0.2f);
  EXPECT_FLOAT_EQ(Ax.Z, 0.3f);
  EXPECT_FLOAT_EQ(Ax.W, 0.9f);
}

TEST(QuatTest, LayoutCompatible_SameSizeAndAlignment)
{
  EXPECT_EQ(sizeof(Quat), sizeof(AxQuat));
  EXPECT_EQ(sizeof(Quat), 4 * sizeof(float));
}

//=============================================================================
// Quat Tests - Operations
//=============================================================================

TEST(QuatTest, Multiply_IdentityIsNoOp)
{
  Quat Q = Quat::FromAxisAngle(Vec3::Up(), 0.5f);
  Quat I = Quat::Identity();
  Quat R = Q * I;
  EXPECT_NEAR(R.X, Q.X, 1e-6f);
  EXPECT_NEAR(R.Y, Q.Y, 1e-6f);
  EXPECT_NEAR(R.Z, Q.Z, 1e-6f);
  EXPECT_NEAR(R.W, Q.W, 1e-6f);
}

TEST(QuatTest, Multiply_TwoRotations_Compose)
{
  // Two 90-degree rotations around Y should produce 180 degrees
  float HalfPi = 3.14159265f / 2.0f;
  Quat A = Quat::FromAxisAngle(Vec3::Up(), HalfPi);
  Quat B = Quat::FromAxisAngle(Vec3::Up(), HalfPi);
  Quat C = A * B;

  // 180 degrees around Y: expect W near 0, Y near +-1
  EXPECT_NEAR(fabsf(C.Y), sinf(HalfPi / 2.0f * 2.0f), 0.01f);
}

TEST(QuatTest, Normalized_UnitLength)
{
  Quat Q(1, 2, 3, 4);
  Quat N = Q.Normalized();
  EXPECT_NEAR(N.Length(), 1.0f, 1e-6f);
}

TEST(QuatTest, Conjugate_NegatesXYZ)
{
  Quat Q(0.1f, 0.2f, 0.3f, 0.9f);
  Quat C = Q.Conjugate();
  EXPECT_FLOAT_EQ(C.X, -0.1f);
  EXPECT_FLOAT_EQ(C.Y, -0.2f);
  EXPECT_FLOAT_EQ(C.Z, -0.3f);
  EXPECT_FLOAT_EQ(C.W, 0.9f);
}

TEST(QuatTest, Slerp_ZeroT_ReturnsA)
{
  Quat A = Quat::Identity();
  Quat B = Quat::FromAxisAngle(Vec3::Up(), 1.0f);
  Quat R = A.Slerp(B, 0.0f);
  EXPECT_NEAR(R.X, A.X, 1e-6f);
  EXPECT_NEAR(R.Y, A.Y, 1e-6f);
  EXPECT_NEAR(R.Z, A.Z, 1e-6f);
  EXPECT_NEAR(R.W, A.W, 1e-6f);
}

TEST(QuatTest, Slerp_OneT_ReturnsB)
{
  Quat A = Quat::Identity();
  Quat B = Quat::FromAxisAngle(Vec3::Up(), 1.0f);
  Quat R = A.Slerp(B, 1.0f);
  EXPECT_NEAR(R.X, B.X, 1e-6f);
  EXPECT_NEAR(R.Y, B.Y, 1e-6f);
  EXPECT_NEAR(R.Z, B.Z, 1e-6f);
  EXPECT_NEAR(R.W, B.W, 1e-6f);
}

TEST(QuatTest, Slerp_Midpoint_InterpolatesCorrectly)
{
  Quat A = Quat::Identity();
  Quat B = Quat::FromAxisAngle(Vec3::Up(), 1.0f);
  Quat Mid = A.Slerp(B, 0.5f);

  // Midpoint should be a rotation of 0.5 radians around Y
  Quat Expected = Quat::FromAxisAngle(Vec3::Up(), 0.5f);
  EXPECT_NEAR(Mid.X, Expected.X, 1e-4f);
  EXPECT_NEAR(Mid.Y, Expected.Y, 1e-4f);
  EXPECT_NEAR(Mid.Z, Expected.Z, 1e-4f);
  EXPECT_NEAR(Mid.W, Expected.W, 1e-4f);
}

//=============================================================================
// Quat Tests - Static Constructors
//=============================================================================

TEST(QuatTest, Identity_WIsOne)
{
  Quat Q = Quat::Identity();
  EXPECT_FLOAT_EQ(Q.X, 0.0f);
  EXPECT_FLOAT_EQ(Q.Y, 0.0f);
  EXPECT_FLOAT_EQ(Q.Z, 0.0f);
  EXPECT_FLOAT_EQ(Q.W, 1.0f);
}

TEST(QuatTest, FromAxisAngle_90DegY_CorrectRotation)
{
  float HalfPi = 3.14159265f / 2.0f;
  Quat Q = Quat::FromAxisAngle(Vec3::Up(), HalfPi);

  // For 90 deg around Y: sin(45deg) ~= 0.7071
  EXPECT_NEAR(Q.X, 0.0f, 1e-6f);
  EXPECT_NEAR(Q.Y, sinf(HalfPi / 2.0f), 1e-4f);
  EXPECT_NEAR(Q.Z, 0.0f, 1e-6f);
  EXPECT_NEAR(Q.W, cosf(HalfPi / 2.0f), 1e-4f);
}

TEST(QuatTest, FromEuler_KnownValues)
{
  // 90 degrees around Y axis only
  float HalfPi = 3.14159265f / 2.0f;
  Quat Q = Quat::FromEuler(Vec3(0, HalfPi, 0));

  // Same result as FromAxisAngle around Y
  Quat Expected = Quat::FromAxisAngle(Vec3::Up(), HalfPi);
  EXPECT_NEAR(Q.X, Expected.X, 1e-4f);
  EXPECT_NEAR(Q.Y, Expected.Y, 1e-4f);
  EXPECT_NEAR(Q.Z, Expected.Z, 1e-4f);
  EXPECT_NEAR(Q.W, Expected.W, 1e-4f);
}

TEST(QuatTest, ToEuler_RoundTrip)
{
  Vec3 Original(0.3f, 0.5f, 0.1f);
  Quat Q = Quat::FromEuler(Original);
  Vec3 Recovered = Q.ToEuler();
  EXPECT_NEAR(Recovered.X, Original.X, 1e-4f);
  EXPECT_NEAR(Recovered.Y, Original.Y, 1e-4f);
  EXPECT_NEAR(Recovered.Z, Original.Z, 1e-4f);
}

//=============================================================================
// Quat Tests - Vector Rotation
//=============================================================================

TEST(QuatTest, Rotate_90DegY_RotatesForwardToRight)
{
  float HalfPi = 3.14159265f / 2.0f;
  Quat Q = Quat::FromAxisAngle(Vec3::Up(), HalfPi);
  Vec3 Forward(0, 0, -1);
  Vec3 Result = Q.Rotate(Forward);

  // 90 deg Y rotation should turn -Z into -X
  EXPECT_NEAR(Result.X, -1.0f, 1e-4f);
  EXPECT_NEAR(Result.Y, 0.0f, 1e-4f);
  EXPECT_NEAR(Result.Z, 0.0f, 1e-4f);
}

//=============================================================================
// Mat4 Tests
//=============================================================================

TEST(Mat4Test, Identity_DiagonalOnes)
{
  Mat4 I = Mat4::Identity();
  for (int C = 0; C < 4; ++C)
  {
    for (int R = 0; R < 4; ++R)
    {
      EXPECT_FLOAT_EQ(I.E[C][R], (C == R) ? 1.0f : 0.0f);
    }
  }
}

TEST(Mat4Test, Multiply_IdentityIsNoOp)
{
  Mat4 A = Mat4::Translation(Vec3(1, 2, 3));
  Mat4 I = Mat4::Identity();
  Mat4 R = A * I;
  for (int C = 0; C < 4; ++C)
  {
    for (int Row = 0; Row < 4; ++Row)
    {
      EXPECT_NEAR(R.E[C][Row], A.E[C][Row], 1e-6f);
    }
  }
}

TEST(Mat4Test, Multiply_TwoMatrices_CorrectResult)
{
  // Translate by (1,0,0) then (0,2,0) should give (1,2,0)
  Mat4 A = Mat4::Translation(Vec3(1, 0, 0));
  Mat4 B = Mat4::Translation(Vec3(0, 2, 0));
  Mat4 R = B * A;
  EXPECT_NEAR(R.E[3][0], 1.0f, 1e-6f);
  EXPECT_NEAR(R.E[3][1], 2.0f, 1e-6f);
  EXPECT_NEAR(R.E[3][2], 0.0f, 1e-6f);
}

TEST(Mat4Test, MulVec4_TransformsCorrectly)
{
  Mat4 T = Mat4::Translation(Vec3(10, 20, 30));
  Vec4 P(0, 0, 0, 1);
  Vec4 R = T * P;
  EXPECT_NEAR(R.X, 10.0f, 1e-6f);
  EXPECT_NEAR(R.Y, 20.0f, 1e-6f);
  EXPECT_NEAR(R.Z, 30.0f, 1e-6f);
  EXPECT_NEAR(R.W, 1.0f, 1e-6f);
}

TEST(Mat4Test, Translation_SetsColumn3)
{
  Mat4 T = Mat4::Translation(Vec3(5, 6, 7));
  EXPECT_FLOAT_EQ(T.E[3][0], 5.0f);
  EXPECT_FLOAT_EQ(T.E[3][1], 6.0f);
  EXPECT_FLOAT_EQ(T.E[3][2], 7.0f);
  EXPECT_FLOAT_EQ(T.E[3][3], 1.0f);
}

TEST(Mat4Test, Scale_SetsDiagonal)
{
  Mat4 S = Mat4::Scale(Vec3(2, 3, 4));
  EXPECT_FLOAT_EQ(S.E[0][0], 2.0f);
  EXPECT_FLOAT_EQ(S.E[1][1], 3.0f);
  EXPECT_FLOAT_EQ(S.E[2][2], 4.0f);
  EXPECT_FLOAT_EQ(S.E[3][3], 1.0f);
  // Off-diagonals should be zero
  EXPECT_FLOAT_EQ(S.E[0][1], 0.0f);
  EXPECT_FLOAT_EQ(S.E[1][0], 0.0f);
}

TEST(Mat4Test, Transposed_SwapsRowsAndColumns)
{
  Mat4 M = Mat4::Translation(Vec3(1, 2, 3));
  Mat4 T = M.Transposed();
  for (int C = 0; C < 4; ++C)
  {
    for (int R = 0; R < 4; ++R)
    {
      EXPECT_FLOAT_EQ(T.E[C][R], M.E[R][C]);
    }
  }
}

TEST(Mat4Test, FromAxMat4x4_Conversion)
{
  AxMat4x4 Ax = ::Identity();
  Ax.E[3][0] = 42.0f;
  Mat4 M(Ax);
  EXPECT_FLOAT_EQ(M.E[3][0], 42.0f);
  EXPECT_FLOAT_EQ(M.E[0][0], 1.0f);

  // Round-trip
  AxMat4x4 Back = M;
  EXPECT_FLOAT_EQ(Back.E[3][0], 42.0f);
}

TEST(Mat4Test, LayoutCompatible_SameSizeAndAlignment)
{
  EXPECT_EQ(sizeof(Mat4), sizeof(AxMat4x4));
  EXPECT_EQ(sizeof(Mat4), 16 * sizeof(float));
}

//=============================================================================
// Transform Tests - Construction
//=============================================================================

#include "AxEngine/AxTransformType.h"

TEST(TransformTest, Default_IsIdentity)
{
  Transform T;
  EXPECT_FLOAT_EQ(T.Translation.X, 0.0f);
  EXPECT_FLOAT_EQ(T.Translation.Y, 0.0f);
  EXPECT_FLOAT_EQ(T.Translation.Z, 0.0f);
  EXPECT_FLOAT_EQ(T.Rotation.W, 1.0f);
  EXPECT_FLOAT_EQ(T.Scale.X, 1.0f);
  EXPECT_FLOAT_EQ(T.Scale.Y, 1.0f);
  EXPECT_FLOAT_EQ(T.Scale.Z, 1.0f);
}

TEST(TransformTest, FromAxTransform_ConvertsCorrectly)
{
  AxTransform Ax = TransformIdentity();
  Ax.Translation = {1.0f, 2.0f, 3.0f};
  Ax.Scale = {2.0f, 2.0f, 2.0f};

  Transform T(Ax);
  EXPECT_FLOAT_EQ(T.Translation.X, 1.0f);
  EXPECT_FLOAT_EQ(T.Translation.Y, 2.0f);
  EXPECT_FLOAT_EQ(T.Translation.Z, 3.0f);
  EXPECT_FLOAT_EQ(T.Scale.X, 2.0f);
}

//=============================================================================
// Transform Tests - Setters
//=============================================================================

TEST(TransformTest, SetTranslation_UpdatesPosition)
{
  Transform T;
  T.SetTranslation(Vec3(5, 6, 7));
  EXPECT_FLOAT_EQ(T.Translation.X, 5.0f);
  EXPECT_FLOAT_EQ(T.Translation.Y, 6.0f);
  EXPECT_FLOAT_EQ(T.Translation.Z, 7.0f);
}

TEST(TransformTest, SetTranslation_MarksDirty)
{
  Transform T;
  // Force cache clean
  T.GetForwardMatrix();
  EXPECT_FALSE(T.IsDirty());

  T.SetTranslation(Vec3(1, 0, 0));
  EXPECT_TRUE(T.IsDirty());
}

TEST(TransformTest, SetRotation_UpdatesRotation)
{
  Transform T;
  Quat R = Quat::FromAxisAngle(Vec3::Up(), 1.0f);
  T.SetRotation(R);
  EXPECT_NEAR(T.Rotation.Y, R.Y, 1e-6f);
  EXPECT_NEAR(T.Rotation.W, R.W, 1e-6f);
}

TEST(TransformTest, SetScale_UpdatesScale)
{
  Transform T;
  T.SetScale(Vec3(2, 3, 4));
  EXPECT_FLOAT_EQ(T.Scale.X, 2.0f);
  EXPECT_FLOAT_EQ(T.Scale.Y, 3.0f);
  EXPECT_FLOAT_EQ(T.Scale.Z, 4.0f);
}

TEST(TransformTest, FluentSetters_ReturnRef)
{
  Transform T;
  Transform& Ref = T.SetTranslation(1, 2, 3).SetScale(2, 2, 2);
  EXPECT_EQ(&Ref, &T);
  EXPECT_FLOAT_EQ(T.Translation.X, 1.0f);
  EXPECT_FLOAT_EQ(T.Scale.X, 2.0f);
}

//=============================================================================
// Transform Tests - Operations
//=============================================================================

TEST(TransformTest, Translate_LocalSpace_RotationAffectsDirection)
{
  Transform T;
  // Rotate 90 degrees around Y
  float HalfPi = 3.14159265f / 2.0f;
  T.SetRotation(Quat::FromAxisAngle(Vec3::Up(), HalfPi));

  // Translate along local Z (forward)
  T.Translate(Vec3(0, 0, -1), false);

  // After 90deg Y rotation, local -Z should map to world -X
  EXPECT_NEAR(T.Translation.X, -1.0f, 0.01f);
  EXPECT_NEAR(T.Translation.Y, 0.0f, 0.01f);
  EXPECT_NEAR(T.Translation.Z, 0.0f, 0.01f);
}

TEST(TransformTest, Translate_WorldSpace_IgnoresRotation)
{
  Transform T;
  // Rotate 90 degrees around Y
  float HalfPi = 3.14159265f / 2.0f;
  T.SetRotation(Quat::FromAxisAngle(Vec3::Up(), HalfPi));

  // Translate in world space
  T.Translate(Vec3(0, 0, -1), true);

  // World-space translation should go straight along -Z
  EXPECT_NEAR(T.Translation.X, 0.0f, 0.01f);
  EXPECT_NEAR(T.Translation.Z, -1.0f, 0.01f);
}

TEST(TransformTest, Rotate_LocalSpace_ComposesWithCurrent)
{
  Transform T;
  float HalfPi = 3.14159265f / 2.0f;

  // Rotate 90 degrees, then 90 more = 180 total
  T.Rotate(Vec3(0, HalfPi, 0), false);
  T.Rotate(Vec3(0, HalfPi, 0), false);

  Vec3 Fwd = T.Forward();
  // 180 deg Y rotation: forward (0,0,-1) becomes (0,0,1)
  EXPECT_NEAR(Fwd.X, 0.0f, 0.01f);
  EXPECT_NEAR(Fwd.Z, 1.0f, 0.01f);
}

TEST(TransformTest, LookAt_PointsForwardAtTarget)
{
  Transform T;
  T.SetTranslation(Vec3(0, 0, 0));
  T.LookAt(Vec3(1, 0, 0));

  Vec3 Fwd = T.Forward();
  EXPECT_NEAR(Fwd.X, 1.0f, 0.01f);
  EXPECT_NEAR(Fwd.Y, 0.0f, 0.01f);
  EXPECT_NEAR(Fwd.Z, 0.0f, 0.01f);
}

//=============================================================================
// Transform Tests - Direction Queries
//=============================================================================

TEST(TransformTest, Forward_DefaultIsNegZ)
{
  Transform T;
  Vec3 Fwd = T.Forward();
  EXPECT_NEAR(Fwd.X, 0.0f, 1e-6f);
  EXPECT_NEAR(Fwd.Y, 0.0f, 1e-6f);
  EXPECT_NEAR(Fwd.Z, -1.0f, 1e-6f);
}

TEST(TransformTest, Right_DefaultIsX)
{
  Transform T;
  Vec3 R = T.Right();
  EXPECT_NEAR(R.X, 1.0f, 1e-6f);
  EXPECT_NEAR(R.Y, 0.0f, 1e-6f);
  EXPECT_NEAR(R.Z, 0.0f, 1e-6f);
}

TEST(TransformTest, Up_DefaultIsY)
{
  Transform T;
  Vec3 U = T.Up();
  EXPECT_NEAR(U.X, 0.0f, 1e-6f);
  EXPECT_NEAR(U.Y, 1.0f, 1e-6f);
  EXPECT_NEAR(U.Z, 0.0f, 1e-6f);
}

TEST(TransformTest, Forward_After90DegYRot_IsX)
{
  Transform T;
  float HalfPi = 3.14159265f / 2.0f;
  T.SetRotation(Quat::FromAxisAngle(Vec3::Up(), HalfPi));

  Vec3 Fwd = T.Forward();
  // 90 deg Y rotation: forward (0,0,-1) becomes (-1,0,0)
  EXPECT_NEAR(Fwd.X, -1.0f, 0.01f);
  EXPECT_NEAR(Fwd.Y, 0.0f, 0.01f);
  EXPECT_NEAR(Fwd.Z, 0.0f, 0.01f);
}

//=============================================================================
// Transform Tests - Dirty Tracking
//=============================================================================

TEST(TransformTest, SetTranslation_SetsDirtyFlag)
{
  Transform T;
  T.GetForwardMatrix();  // clean cache
  EXPECT_FALSE(T.IsDirty());

  T.SetTranslation(Vec3(1, 0, 0));
  EXPECT_TRUE(T.IsDirty());

  // Recompute should clear dirty
  T.GetForwardMatrix();
  EXPECT_FALSE(T.IsDirty());
}

TEST(TransformTest, MarkDirty_SetsFlags)
{
  Transform T;
  T.GetForwardMatrix();
  T.GetInverseMatrix();
  EXPECT_FALSE(T.IsDirty());

  T.MarkDirty();
  EXPECT_TRUE(T.IsDirty());
}

//=============================================================================
// Transform Tests - Matrix Computation
//=============================================================================

TEST(TransformTest, GetForwardMatrix_IdentityIsIdentityMatrix)
{
  Transform T;
  const Mat4& M = T.GetForwardMatrix();
  Mat4 I = Mat4::Identity();
  for (int C = 0; C < 4; ++C)
  {
    for (int R = 0; R < 4; ++R)
    {
      EXPECT_NEAR(M.E[C][R], I.E[C][R], 1e-6f);
    }
  }
}

TEST(TransformTest, GetForwardMatrix_TranslationInColumn3)
{
  Transform T;
  T.SetTranslation(Vec3(10, 20, 30));
  const Mat4& M = T.GetForwardMatrix();
  EXPECT_NEAR(M.E[3][0], 10.0f, 1e-6f);
  EXPECT_NEAR(M.E[3][1], 20.0f, 1e-6f);
  EXPECT_NEAR(M.E[3][2], 30.0f, 1e-6f);
}

TEST(TransformTest, GetInverseMatrix_UndoesForward)
{
  Transform T;
  T.SetTranslation(Vec3(5, 10, 15));
  T.SetScale(Vec3(2, 2, 2));

  const Mat4& Fwd = T.GetForwardMatrix();
  const Mat4& Inv = T.GetInverseMatrix();

  // Fwd * Inv should be approximately Identity
  Mat4 Product = Fwd * Inv;
  for (int C = 0; C < 4; ++C)
  {
    for (int R = 0; R < 4; ++R)
    {
      float Expected = (C == R) ? 1.0f : 0.0f;
      EXPECT_NEAR(Product.E[C][R], Expected, 1e-4f);
    }
  }
}

TEST(TransformTest, ToAxTransform_Conversion)
{
  Transform T;
  T.SetTranslation(Vec3(1, 2, 3));
  T.SetScale(Vec3(4, 5, 6));

  AxTransform Ax = T;
  EXPECT_FLOAT_EQ(Ax.Translation.X, 1.0f);
  EXPECT_FLOAT_EQ(Ax.Translation.Y, 2.0f);
  EXPECT_FLOAT_EQ(Ax.Translation.Z, 3.0f);
  EXPECT_FLOAT_EQ(Ax.Scale.X, 4.0f);
}
