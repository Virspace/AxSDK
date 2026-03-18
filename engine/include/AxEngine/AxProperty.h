#pragma once

/**
 * AxProperty.h - Type-Safe Property Wrapper Template
 *
 * Property<T> is a thin value wrapper that replaces raw member fields in typed
 * node classes. It provides:
 *   - Implicit conversion for zero-overhead reads
 *   - operator= that auto-marks the owning node dirty on writes
 *   - Compile-time type safety (assigning wrong type is a compile error)
 *   - Equality comparison for skip-if-default serialization
 *
 * Compound math types (Vec3, Vec4, Quat) use an inheritance specialization
 * so that member access (Color.X) works naturally. Whole-value assignment
 * (Color = Vec3(1,0,0)) marks the node dirty; component writes (Color.X = 1)
 * work but skip the dirty flag — same tradeoff as Unreal Engine's UPROPERTY.
 */

#include "AxEngine/AxNode.h"
#include "AxEngine/AxMathTypes.h"

//=============================================================================
// Primary template — scalars, strings, enums
//=============================================================================

template<typename T>
class Property
{
public:
  using ValueType = T;

  Property(Node* Owner, const T& Default)
    : Value_(Default), Owner_(Owner) {}

  // Read -- implicit conversion, zero overhead (inlines to value read)
  operator const T&() const { return (Value_); }
  const T& Get() const { return (Value_); }

  // Write -- marks owner dirty
  Property& operator=(const T& V)
  {
    Value_ = V;
    if (Owner_) { Owner_->MarkDirty(); }
    return (*this);
  }

  // Comparison (for skip-if-default serialization)
  bool operator==(const T& Other) const { return (Value_ == Other); }
  bool operator!=(const T& Other) const { return (Value_ != Other); }

private:
  T Value_;
  Node* Owner_;
};

//=============================================================================
// Compound math type specializations — inherit from T for natural .X access
//=============================================================================

#define AX_COMPOUND_PROPERTY(T)                                    \
template<>                                                         \
class Property<T> : public T                                       \
{                                                                  \
public:                                                            \
  using ValueType = T;                                             \
                                                                   \
  Property(Node* Owner, const T& Default)                          \
    : T(Default), Owner_(Owner) {}                                 \
                                                                   \
  const T& Get() const { return (*this); }                         \
                                                                   \
  Property& operator=(const T& V)                                  \
  {                                                                \
    static_cast<T&>(*this) = V;                                    \
    if (Owner_) { Owner_->MarkDirty(); }                           \
    return (*this);                                                \
  }                                                                \
                                                                   \
  bool operator==(const T& Other) const { return (T::operator==(Other)); } \
  bool operator!=(const T& Other) const { return (!(*this == Other)); }    \
                                                                   \
private:                                                           \
  Node* Owner_;                                                    \
};

AX_COMPOUND_PROPERTY(Vec3)
AX_COMPOUND_PROPERTY(Vec4)
AX_COMPOUND_PROPERTY(Quat)

#undef AX_COMPOUND_PROPERTY

/** Shorthand for Property<T> initializer lists: expands to field(this, default) */
#define PROP(field, default) field(this, default)
