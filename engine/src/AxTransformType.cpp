/**
 * AxTransformType.cpp - Transform non-inline methods
 *
 * Implements matrix computation, transform operations, and LookAt
 * by delegating to Foundation math functions.
 */

#include "AxEngine/AxTransformType.h"
#include "AxEngine/AxNode.h"
#include "AxEngine/AxSceneTree.h"
#include "Foundation/AxMath.h"

#include <cstring>

//=============================================================================
// Construction
//=============================================================================

Transform::Transform()
  : Translation(0, 0, 0)
  , Rotation()  // identity: (0, 0, 0, 1)
  , Scale(1, 1, 1)
  , ForwardMatrixDirty_(true)
  , InverseMatrixDirty_(true)
  , IsIdentity_(true)
  , OwningNode_(nullptr)
{
  CachedForwardMatrix_ = Mat4::Identity();
  CachedInverseMatrix_ = Mat4::Identity();
}

Transform::Transform(const AxTransform& T)
  : Translation(T.Translation)
  , Rotation(T.Rotation)
  , Scale(T.Scale)
  , CachedForwardMatrix_(T.CachedForwardMatrix)
  , CachedInverseMatrix_(T.CachedInverseMatrix)
  , ForwardMatrixDirty_(T.ForwardMatrixDirty)
  , InverseMatrixDirty_(T.InverseMatrixDirty)
  , IsIdentity_(T.IsIdentity)
  , OwningNode_(nullptr)
{
}

//=============================================================================
// Setters
//=============================================================================

Transform& Transform::SetTranslation(const Vec3& T)
{
  Translation = T;
  MarkDirty();
  return (*this);
}

Transform& Transform::SetTranslation(float X, float Y, float Z)
{
  Translation = Vec3(X, Y, Z);
  MarkDirty();
  return (*this);
}

Transform& Transform::SetRotation(const Quat& R)
{
  Rotation = R.Normalized();
  MarkDirty();
  return (*this);
}

Transform& Transform::SetRotation(float PitchRad, float YawRad, float RollRad)
{
  Rotation = Quat::FromEuler(Vec3(PitchRad, YawRad, RollRad)).Normalized();
  MarkDirty();
  return (*this);
}

Transform& Transform::SetScale(const Vec3& S)
{
  Scale = S;
  MarkDirty();
  return (*this);
}

Transform& Transform::SetScale(float X, float Y, float Z)
{
  Scale = Vec3(X, Y, Z);
  MarkDirty();
  return (*this);
}

//=============================================================================
// Operations
//=============================================================================

void Transform::Translate(const Vec3& Delta, bool WorldSpace)
{
  AxTransform Temp = *this;
  TransformTranslate(&Temp, Delta, WorldSpace);
  Translation = Vec3(Temp.Translation);
  MarkDirty();
}

void Transform::Rotate(const Vec3& EulerAngles, bool WorldSpace)
{
  AxTransform Temp = *this;
  TransformRotate(&Temp, EulerAngles, WorldSpace);
  Rotation = Quat(Temp.Rotation);
  MarkDirty();
}

void Transform::RotateFromMouseDelta(const Vec2& Delta, float Sensitivity)
{
  AxTransform Temp = *this;
  TransformRotateFromMouseDelta(&Temp, Delta, Sensitivity);
  Rotation = Quat(Temp.Rotation);
  MarkDirty();
}

void Transform::LookAt(const Vec3& Target, const Vec3& WorldUp)
{
  AxTransform Temp = *this;
  TransformLookAt(&Temp, Target, WorldUp);
  Rotation = Quat(Temp.Rotation);
  MarkDirty();
}

//=============================================================================
// Direction Queries
//=============================================================================

Vec3 Transform::Forward() const
{
  AxTransform Temp = *this;
  AxVec3 R = TransformForward(&Temp);
  return (Vec3(R));
}

Vec3 Transform::Right() const
{
  AxTransform Temp = *this;
  AxVec3 R = TransformRight(&Temp);
  return (Vec3(R));
}

Vec3 Transform::Up() const
{
  AxTransform Temp = *this;
  AxVec3 R = TransformUp(&Temp);
  return (Vec3(R));
}

//=============================================================================
// Matrix Access (lazy-evaluated)
//=============================================================================

const Mat4& Transform::GetForwardMatrix() const
{
  if (ForwardMatrixDirty_) {
    // Build local TRS matrix
    AxMat4x4 RotMatrix = QuatToMat4x4(Rotation);

    AxMat4x4 LocalMatrix = {0};

    // Apply scale to rotation columns
    LocalMatrix.E[0][0] = RotMatrix.E[0][0] * Scale.X;
    LocalMatrix.E[0][1] = RotMatrix.E[0][1] * Scale.X;
    LocalMatrix.E[0][2] = RotMatrix.E[0][2] * Scale.X;
    LocalMatrix.E[0][3] = 0.0f;

    LocalMatrix.E[1][0] = RotMatrix.E[1][0] * Scale.Y;
    LocalMatrix.E[1][1] = RotMatrix.E[1][1] * Scale.Y;
    LocalMatrix.E[1][2] = RotMatrix.E[1][2] * Scale.Y;
    LocalMatrix.E[1][3] = 0.0f;

    LocalMatrix.E[2][0] = RotMatrix.E[2][0] * Scale.Z;
    LocalMatrix.E[2][1] = RotMatrix.E[2][1] * Scale.Z;
    LocalMatrix.E[2][2] = RotMatrix.E[2][2] * Scale.Z;
    LocalMatrix.E[2][3] = 0.0f;

    LocalMatrix.E[3][0] = Translation.X;
    LocalMatrix.E[3][1] = Translation.Y;
    LocalMatrix.E[3][2] = Translation.Z;
    LocalMatrix.E[3][3] = 1.0f;

    CachedForwardMatrix_ = Mat4(LocalMatrix);
    ForwardMatrixDirty_ = false;
  }

  return (CachedForwardMatrix_);
}

const Mat4& Transform::GetInverseMatrix() const
{
  if (InverseMatrixDirty_) {
    // Inverse scale
    float InvSX = (Scale.X != 0.0f) ? (1.0f / Scale.X) : 1.0f;
    float InvSY = (Scale.Y != 0.0f) ? (1.0f / Scale.Y) : 1.0f;
    float InvSZ = (Scale.Z != 0.0f) ? (1.0f / Scale.Z) : 1.0f;

    AxMat4x4 InvScaleMatrix = ::Identity();
    InvScaleMatrix.E[0][0] = InvSX;
    InvScaleMatrix.E[1][1] = InvSY;
    InvScaleMatrix.E[2][2] = InvSZ;

    // Inverse rotation
    AxQuat InvRot = QuatConjugate(Rotation);
    AxMat4x4 InvRotMatrix = QuatToMat4x4(InvRot);

    // Inverse translation
    AxMat4x4 InvTransMatrix = ::Identity();
    InvTransMatrix.E[3][0] = -Translation.X;
    InvTransMatrix.E[3][1] = -Translation.Y;
    InvTransMatrix.E[3][2] = -Translation.Z;

    // S^-1 * R^-1 * T^-1 (note: Mat4x4Mul(A,B) computes B*A)
    AxMat4x4 SR = Mat4x4Mul(InvRotMatrix, InvScaleMatrix);
    AxMat4x4 Result = Mat4x4Mul(InvTransMatrix, SR);

    CachedInverseMatrix_ = Mat4(Result);
    InverseMatrixDirty_ = false;
  }

  return (CachedInverseMatrix_);
}

//=============================================================================
// View Matrix (computed on demand)
//=============================================================================

Mat4 Transform::GetViewMatrix() const
{
  Vec3 F = Forward();
  Vec3 R = Right();
  Vec3 U = Up();

  Mat4 Result = {};

  // Rotation part (transpose of camera basis)
  Result.E[0][0] = R.X;
  Result.E[1][0] = R.Y;
  Result.E[2][0] = R.Z;

  Result.E[0][1] = U.X;
  Result.E[1][1] = U.Y;
  Result.E[2][1] = U.Z;

  Result.E[0][2] = -F.X;
  Result.E[1][2] = -F.Y;
  Result.E[2][2] = -F.Z;

  // Translation part (dot products)
  Result.E[3][0] = -(R.X * Translation.X + R.Y * Translation.Y + R.Z * Translation.Z);
  Result.E[3][1] = -(U.X * Translation.X + U.Y * Translation.Y + U.Z * Translation.Z);
  Result.E[3][2] =  (F.X * Translation.X + F.Y * Translation.Y + F.Z * Translation.Z);
  Result.E[3][3] = 1.0f;

  return (Result);
}

//=============================================================================
// Dirty Tracking
//=============================================================================

void Transform::MarkDirty()
{
  ForwardMatrixDirty_ = true;
  InverseMatrixDirty_ = true;
  IsIdentity_ = false;
  NotifyDirty();
}

void Transform::NotifyDirty()
{
  if (OwningNode_ && OwningNode_->GetOwningTree()) {
    OwningNode_->GetOwningTree()->MarkTransformDirty(OwningNode_);
  }
}

//=============================================================================
// Assignment
//=============================================================================

Transform& Transform::operator=(const Transform& Other)
{
  if (this != &Other) {
    Translation = Other.Translation;
    Rotation = Other.Rotation;
    Scale = Other.Scale;
    MarkDirty();
  }
  return (*this);
}

Transform& Transform::operator=(const AxTransform& T)
{
  Translation = Vec3(T.Translation);
  Rotation = Quat(T.Rotation);
  Scale = Vec3(T.Scale);
  MarkDirty();
  return (*this);
}

//=============================================================================
// Conversion
//=============================================================================

Transform::operator AxTransform() const
{
  AxTransform Result = TransformIdentity();
  Result.Translation = Translation;
  Result.Rotation = Rotation;
  Result.Scale = Scale;
  Result.ForwardMatrixDirty = ForwardMatrixDirty_;
  Result.InverseMatrixDirty = InverseMatrixDirty_;
  Result.IsIdentity = IsIdentity_;
  memcpy(Result.CachedForwardMatrix.E, CachedForwardMatrix_.E, sizeof(Result.CachedForwardMatrix.E));
  memcpy(Result.CachedInverseMatrix.E, CachedInverseMatrix_.E, sizeof(Result.CachedInverseMatrix.E));
  return (Result);
}

//=============================================================================
// Static
//=============================================================================

Transform Transform::Identity()
{
  return (Transform());
}
