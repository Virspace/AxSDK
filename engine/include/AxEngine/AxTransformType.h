#pragma once

/**
 * AxTransformType.h - C++ Transform class for the engine layer.
 *
 * Wraps TRS (Translation, Rotation, Scale) with cached matrix computation,
 * dirty tracking, and SceneTree notification via an owning Node back-pointer.
 *
 * Layout is NOT compatible with AxTransform (different fields/order).
 * Conversion is done by copying TRS values.
 */

#include "AxEngine/AxMathTypes.h"

// Forward declarations
class Node;
class SceneTree;

class Transform
{
public:
  // TRS components (public for direct access, matching current patterns)
  Vec3 Translation;
  Quat Rotation;
  Vec3 Scale;

  // Constructors
  Transform();
  Transform(const AxTransform& T);

  // Setters (mark dirty, notify SceneTree via OwningNode_)
  Transform& SetTranslation(const Vec3& T);
  Transform& SetTranslation(float X, float Y, float Z);
  Transform& SetRotation(const Quat& R);
  Transform& SetRotation(float PitchRad, float YawRad, float RollRad);
  Transform& SetScale(const Vec3& S);
  Transform& SetScale(float X, float Y, float Z);

  // Operations
  void Translate(const Vec3& Delta, bool WorldSpace = false);
  void Rotate(const Vec3& EulerAngles, bool WorldSpace = false);
  void RotateFromMouseDelta(const Vec2& Delta, float Sensitivity);
  void LookAt(const Vec3& Target, const Vec3& WorldUp = Vec3::Up());

  // Direction queries (computed from Rotation)
  Vec3 Forward() const;
  Vec3 Right() const;
  Vec3 Up() const;

  // Matrix access (cached, lazy-evaluated)
  const Mat4& GetForwardMatrix() const;
  const Mat4& GetInverseMatrix() const;

  // View matrix (computed on demand, not cached -- only cameras need this)
  Mat4 GetViewMatrix() const;

  // Dirty tracking
  void MarkDirty();
  bool IsDirty() const { return (ForwardMatrixDirty_); }

  // Assignment (copies TRS only, preserves OwningNode_)
  Transform& operator=(const Transform& Other);
  Transform& operator=(const AxTransform& T);

  // Conversion
  operator AxTransform() const;

  // Static
  static Transform Identity();

private:
  // Cache (lazy-evaluated from TRS)
  mutable Mat4 CachedForwardMatrix_;
  mutable Mat4 CachedInverseMatrix_;
  mutable bool ForwardMatrixDirty_;
  mutable bool InverseMatrixDirty_;
  bool IsIdentity_;

  // Back-pointer for SceneTree dirty notification (set by Node)
  Node* OwningNode_;

  // Notify SceneTree if OwningNode_ is set
  void NotifyDirty();

  friend class Node;
  friend class SceneTree;
};
