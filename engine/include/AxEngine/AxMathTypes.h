#pragma once

#include "Foundation/AxTypes.h"
#include "Foundation/AxMath.h"

#include <cstddef>
#include <cstring>

// Forward declarations
struct Vec3;
struct Vec4;

//=============================================================================
// Vec2
//=============================================================================

struct Vec2
{
  float X, Y;

  // Constructors
  Vec2() : X(0), Y(0) {}
  Vec2(float X, float Y) : X(X), Y(Y) {}
  Vec2(const AxVec2& V) : X(V.X), Y(V.Y) {}
  operator AxVec2() const { return (AxVec2){X, Y}; }

  // Arithmetic operators
  Vec2 operator+(const Vec2& B) const { return (Vec2(X + B.X, Y + B.Y)); }
  Vec2 operator-(const Vec2& B) const { return (Vec2(X - B.X, Y - B.Y)); }
  Vec2 operator*(float S) const { return (Vec2(X * S, Y * S)); }
  Vec2 operator/(float S) const { return (Vec2(X / S, Y / S)); }
  Vec2 operator-() const { return (Vec2(-X, -Y)); }

  Vec2& operator+=(const Vec2& B) { X += B.X; Y += B.Y; return (*this); }
  Vec2& operator-=(const Vec2& B) { X -= B.X; Y -= B.Y; return (*this); }
  Vec2& operator*=(float S) { X *= S; Y *= S; return (*this); }

  bool operator==(const Vec2& B) const { return (X == B.X && Y == B.Y); }
  bool operator!=(const Vec2& B) const { return (!(*this == B)); }

  // Methods
  float Length() const { return (Vec2Len((AxVec2){X, Y})); }
  float LengthSquared() const { return (X * X + Y * Y); }
  Vec2 Normalized() const { AxVec2 R = Vec2Norm((AxVec2){X, Y}); return (Vec2(R.X, R.Y)); }
  float Dot(const Vec2& B) const { return (X * B.X + Y * B.Y); }

  // Static constructors
  static Vec2 Zero() { return (Vec2(0, 0)); }
  static Vec2 One() { return (Vec2(1, 1)); }
  static Vec2 Up() { return (Vec2(0, 1)); }
};

inline Vec2 operator*(float S, const Vec2& V) { return (V * S); }

static_assert(sizeof(Vec2) == sizeof(AxVec2), "Vec2 must be layout-compatible with AxVec2");
static_assert(offsetof(Vec2, X) == 0, "Vec2::X must be at offset 0");
static_assert(offsetof(Vec2, Y) == sizeof(float), "Vec2::Y must be at offset 4");

//=============================================================================
// Vec3
//=============================================================================

struct Vec3
{
  float X, Y, Z;

  // Constructors
  Vec3() : X(0), Y(0), Z(0) {}
  Vec3(float X, float Y, float Z) : X(X), Y(Y), Z(Z) {}
  Vec3(float S) : X(S), Y(S), Z(S) {}
  Vec3(const AxVec3& V) : X(V.X), Y(V.Y), Z(V.Z) {}
  operator AxVec3() const { return (AxVec3){X, Y, Z}; }

  // Arithmetic operators
  Vec3 operator+(const Vec3& B) const { return (Vec3(X + B.X, Y + B.Y, Z + B.Z)); }
  Vec3 operator-(const Vec3& B) const { return (Vec3(X - B.X, Y - B.Y, Z - B.Z)); }
  Vec3 operator*(float S) const { return (Vec3(X * S, Y * S, Z * S)); }
  Vec3 operator/(float S) const { return (Vec3(X / S, Y / S, Z / S)); }
  Vec3 operator-() const { return (Vec3(-X, -Y, -Z)); }

  Vec3& operator+=(const Vec3& B) { X += B.X; Y += B.Y; Z += B.Z; return (*this); }
  Vec3& operator-=(const Vec3& B) { X -= B.X; Y -= B.Y; Z -= B.Z; return (*this); }
  Vec3& operator*=(float S) { X *= S; Y *= S; Z *= S; return (*this); }

  bool operator==(const Vec3& B) const { return (X == B.X && Y == B.Y && Z == B.Z); }
  bool operator!=(const Vec3& B) const { return (!(*this == B)); }

  // Methods
  float Length() const { return (Vec3Length((AxVec3){X, Y, Z})); }
  float LengthSquared() const { return (X * X + Y * Y + Z * Z); }
  Vec3 Normalized() const { AxVec3 R = Vec3Normalize((AxVec3){X, Y, Z}); return (Vec3(R.X, R.Y, R.Z)); }
  float Dot(const Vec3& B) const { return (Vec3Dot((AxVec3){X, Y, Z}, (AxVec3){B.X, B.Y, B.Z})); }
  Vec3 Cross(const Vec3& B) const { AxVec3 R = Vec3Cross((AxVec3){X, Y, Z}, (AxVec3){B.X, B.Y, B.Z}); return (Vec3(R.X, R.Y, R.Z)); }
  Vec3 Lerp(const Vec3& B, float T) const { AxVec3 R = Vec3Lerp((AxVec3){X, Y, Z}, (AxVec3){B.X, B.Y, B.Z}, T); return (Vec3(R.X, R.Y, R.Z)); }

  // Static constructors
  static Vec3 Zero() { return (Vec3(0, 0, 0)); }
  static Vec3 One() { return (Vec3(1, 1, 1)); }
  static Vec3 Up() { return (Vec3(0, 1, 0)); }
  static Vec3 Forward() { return (Vec3(0, 0, -1)); }
  static Vec3 Right() { return (Vec3(1, 0, 0)); }
};

inline Vec3 operator*(float S, const Vec3& V) { return (V * S); }

static_assert(sizeof(Vec3) == sizeof(AxVec3), "Vec3 must be layout-compatible with AxVec3");
static_assert(offsetof(Vec3, X) == 0, "Vec3::X must be at offset 0");
static_assert(offsetof(Vec3, Y) == sizeof(float), "Vec3::Y must be at offset 4");
static_assert(offsetof(Vec3, Z) == 2 * sizeof(float), "Vec3::Z must be at offset 8");

//=============================================================================
// Vec4
//=============================================================================

struct Vec4
{
  float X, Y, Z, W;

  // Constructors
  Vec4() : X(0), Y(0), Z(0), W(0) {}
  Vec4(float X, float Y, float Z, float W) : X(X), Y(Y), Z(Z), W(W) {}
  Vec4(const Vec3& V, float W) : X(V.X), Y(V.Y), Z(V.Z), W(W) {}
  Vec4(const AxVec4& V) : X(V.X), Y(V.Y), Z(V.Z), W(V.W) {}
  operator AxVec4() const { return (AxVec4){.E = {X, Y, Z, W}}; }

  // Arithmetic operators
  Vec4 operator+(const Vec4& B) const { return (Vec4(X + B.X, Y + B.Y, Z + B.Z, W + B.W)); }
  Vec4 operator-(const Vec4& B) const { return (Vec4(X - B.X, Y - B.Y, Z - B.Z, W - B.W)); }
  Vec4 operator*(float S) const { return (Vec4(X * S, Y * S, Z * S, W * S)); }
  Vec4 operator/(float S) const { return (Vec4(X / S, Y / S, Z / S, W / S)); }
  Vec4 operator-() const { return (Vec4(-X, -Y, -Z, -W)); }

  Vec4& operator+=(const Vec4& B) { X += B.X; Y += B.Y; Z += B.Z; W += B.W; return (*this); }
  Vec4& operator-=(const Vec4& B) { X -= B.X; Y -= B.Y; Z -= B.Z; W -= B.W; return (*this); }
  Vec4& operator*=(float S) { X *= S; Y *= S; Z *= S; W *= S; return (*this); }

  bool operator==(const Vec4& B) const { return (X == B.X && Y == B.Y && Z == B.Z && W == B.W); }
  bool operator!=(const Vec4& B) const { return (!(*this == B)); }

  // Methods
  float Length() const { return (sqrtf(X * X + Y * Y + Z * Z + W * W)); }
  float LengthSquared() const { return (X * X + Y * Y + Z * Z + W * W); }
  float Dot(const Vec4& B) const { return (X * B.X + Y * B.Y + Z * B.Z + W * B.W); }

  // Static constructors
  static Vec4 Zero() { return (Vec4(0, 0, 0, 0)); }
  static Vec4 One() { return (Vec4(1, 1, 1, 1)); }
};

inline Vec4 operator*(float S, const Vec4& V) { return (V * S); }

// Note: AxVec4 is 20 bytes (union includes RGBA[4] struct), Vec4 is 16 bytes.
// Layout compatibility via reinterpret_cast is not possible; conversion uses copy.
static_assert(sizeof(Vec4) == 4 * sizeof(float), "Vec4 must be 4 floats");
static_assert(offsetof(Vec4, X) == 0, "Vec4::X must be at offset 0");
static_assert(offsetof(Vec4, W) == 3 * sizeof(float), "Vec4::W must be at offset 12");

//=============================================================================
// Quat
//=============================================================================

struct Quat
{
  float X, Y, Z, W;

  // Constructors
  Quat() : X(0), Y(0), Z(0), W(1) {}  // identity
  Quat(float X, float Y, float Z, float W) : X(X), Y(Y), Z(Z), W(W) {}
  Quat(const AxQuat& Q) : X(Q.X), Y(Q.Y), Z(Q.Z), W(Q.W) {}
  operator AxQuat() const { return (AxQuat){X, Y, Z, W}; }

  // Composition operator
  Quat operator*(const Quat& B) const
  {
    AxQuat R = QuatMultiply((AxQuat){X, Y, Z, W}, (AxQuat){B.X, B.Y, B.Z, B.W});
    return (Quat(R.X, R.Y, R.Z, R.W));
  }

  bool operator==(const Quat& B) const { return (X == B.X && Y == B.Y && Z == B.Z && W == B.W); }
  bool operator!=(const Quat& B) const { return (!(*this == B)); }

  // Methods
  float Length() const { return (QuatLength((AxQuat){X, Y, Z, W})); }

  Quat Normalized() const
  {
    AxQuat R = QuatNormalize((AxQuat){X, Y, Z, W});
    return (Quat(R.X, R.Y, R.Z, R.W));
  }

  Quat Conjugate() const
  {
    AxQuat R = QuatConjugate((AxQuat){X, Y, Z, W});
    return (Quat(R.X, R.Y, R.Z, R.W));
  }

  Vec3 ToEuler() const
  {
    AxVec3 R = QuatToEuler((AxQuat){X, Y, Z, W});
    return (Vec3(R.X, R.Y, R.Z));
  }

  Quat Slerp(const Quat& B, float T) const
  {
    AxQuat R = QuaternionSlerp((AxQuat){X, Y, Z, W}, (AxQuat){B.X, B.Y, B.Z, B.W}, T);
    return (Quat(R.X, R.Y, R.Z, R.W));
  }

  Vec3 Rotate(const Vec3& V) const
  {
    // q * v * q^-1 (using conjugate since we assume unit quaternion)
    Quat VQ(V.X, V.Y, V.Z, 0.0f);
    Quat Result = *this * VQ * Conjugate();
    return (Vec3(Result.X, Result.Y, Result.Z));
  }

  // Static constructors
  static Quat Identity() { return (Quat(0, 0, 0, 1)); }

  static Quat FromAxisAngle(const Vec3& Axis, float AngleRadians)
  {
    AxQuat R = QuatFromAxisAngle((AxVec3){Axis.X, Axis.Y, Axis.Z}, AngleRadians);
    return (Quat(R.X, R.Y, R.Z, R.W));
  }

  static Quat FromEuler(const Vec3& EulerRadians)
  {
    AxQuat R = QuatFromEuler((AxVec3){EulerRadians.X, EulerRadians.Y, EulerRadians.Z});
    return (Quat(R.X, R.Y, R.Z, R.W));
  }
};

static_assert(sizeof(Quat) == sizeof(AxQuat), "Quat must be layout-compatible with AxQuat");
static_assert(offsetof(Quat, X) == 0, "Quat::X must be at offset 0");
static_assert(offsetof(Quat, W) == 3 * sizeof(float), "Quat::W must be at offset 12");

//=============================================================================
// Mat4
//=============================================================================

struct Mat4
{
  float E[4][4];  // E[column][row], same as AxMat4x4

  // Constructors
  Mat4() : E{} {}  // zero-initialized
  Mat4(const AxMat4x4& M) { memcpy(E, M.E, sizeof(E)); }
  operator AxMat4x4() const { AxMat4x4 R; memcpy(R.E, E, sizeof(E)); return (R); }

  // Operators
  Mat4 operator*(const Mat4& B) const
  {
    AxMat4x4 R = Mat4x4Mul(*this, B);
    return (Mat4(R));
  }

  Vec4 operator*(const Vec4& V) const
  {
    AxVec4 R = Mat4x4MulVec4(*this, (AxVec4){.E = {V.X, V.Y, V.Z, V.W}});
    return (Vec4(R.X, R.Y, R.Z, R.W));
  }

  // Methods
  Mat4 Transposed() const
  {
    AxMat4x4 R = Transpose(*this);
    return (Mat4(R));
  }

  // Static constructors
  static Mat4 Identity()
  {
    AxMat4x4 R = ::Identity();
    return (Mat4(R));
  }

  static Mat4 Translation(const Vec3& T)
  {
    AxMat4x4 R = Translate(::Identity(), (AxVec3){T.X, T.Y, T.Z});
    return (Mat4(R));
  }

  static Mat4 Scale(const Vec3& S)
  {
    AxMat4x4 R = Mat4x4Scale((AxVec3){S.X, S.Y, S.Z});
    return (Mat4(R));
  }

  static Mat4 RotationX(float Angle)
  {
    AxMat4x4 R = XRotation(Angle);
    return (Mat4(R));
  }

  static Mat4 RotationY(float Angle)
  {
    AxMat4x4 R = YRotation(Angle);
    return (Mat4(R));
  }

  static Mat4 RotationZ(float Angle)
  {
    AxMat4x4 R = ZRotation(Angle);
    return (Mat4(R));
  }

  static Mat4 Perspective(float FOV, float Aspect, float Near, float Far)
  {
    AxMat4x4 R = CalcPerspectiveProjection(FOV, Aspect, Near, Far);
    return (Mat4(R));
  }

  static Mat4 Orthographic(float Left, float Right, float Bottom, float Top, float Near, float Far)
  {
    AxMat4x4 R = CalcOrthographicProjection(Left, Right, Bottom, Top, Near, Far);
    return (Mat4(R));
  }
};

static_assert(sizeof(Mat4) == sizeof(AxMat4x4), "Mat4 must be layout-compatible with AxMat4x4");
static_assert(offsetof(Mat4, E) == 0, "Mat4::E must be at offset 0");
