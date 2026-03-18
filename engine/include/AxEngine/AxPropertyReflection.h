#pragma once

/**
 * AxPropertyReflection.h - Property Descriptor and Registry
 *
 * Runtime reflection system for typed node properties. Each property is
 * described by a PropDescriptor containing its name, type, byte offset,
 * flags, default value, and editor hints. Property descriptors are
 * registered per NodeType via AX_REGISTER_PROPERTIES.
 *
 * The PropertyRegistry maps NodeType -> ordered descriptor tables, enabling
 * generic property enumeration for the scene parser and future editor.
 */

#include "AxEngine/AxNode.h"
#include "AxEngine/AxProperty.h"
#include "AxEngine/AxMathTypes.h"
#include "Foundation/AxTypes.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

//=============================================================================
// Property Type Enum
//=============================================================================

enum class PropType : uint8_t
{
  Float,
  Int32,
  UInt32,
  Bool,
  String,
  Vec3,
  Vec4,
  Quat,
  Enum
};

//=============================================================================
// Property Flags
//=============================================================================

enum class PropFlags : uint8_t
{
  None          = 0,
  Serialize     = 1 << 0,
  EditorVisible = 1 << 1,
  ReadOnly      = 1 << 2,
};

inline PropFlags operator|(PropFlags A, PropFlags B)
{
  return (static_cast<PropFlags>(static_cast<uint8_t>(A) | static_cast<uint8_t>(B)));
}

inline PropFlags operator&(PropFlags A, PropFlags B)
{
  return (static_cast<PropFlags>(static_cast<uint8_t>(A) & static_cast<uint8_t>(B)));
}

inline bool HasFlag(PropFlags Flags, PropFlags Test)
{
  return (static_cast<uint8_t>(Flags & Test) != 0);
}

//=============================================================================
// Enum Entry (bidirectional string <-> int mapping)
//=============================================================================

struct EnumEntry
{
  const char* Name;
  int32_t Value;
};

const char* EnumToString(const EnumEntry* Entries, uint8_t Count, int32_t Value);
bool StringToEnum(const EnumEntry* Entries, uint8_t Count, std::string_view Name, int32_t* OutValue);

//=============================================================================
// Property Descriptor
//=============================================================================

struct PropDescriptor
{
  const char* Name;
  PropType Type;
  uint16_t Offset;
  PropFlags Flags;
  const char* Category;

  // Default value (for skip-if-default serialization)
  union {
    float FloatDefault;
    int32_t IntDefault;
    uint32_t UIntDefault;
    bool BoolDefault;
  };

  // Numeric hints
  float Min;
  float Max;
  float Step;

  // Enum entries (null for non-enum)
  const EnumEntry* EnumEntries;
  uint8_t EnumCount;

  // Optional change callback (null = none)
  void (*OnChanged)(Node* Owner);
};

//=============================================================================
// Property Registry
//=============================================================================

class PropertyRegistry
{
public:
  static PropertyRegistry& Get();

  void Register(NodeType Type, const PropDescriptor* Descriptors, uint32_t Count);

  const PropDescriptor* GetProperties(NodeType Type, uint32_t* OutCount) const;

  const PropDescriptor* FindProperty(NodeType Type, std::string_view Name) const;

private:
  static constexpr uint32_t MaxNodeTypes = 32;

  struct TypeEntry
  {
    const PropDescriptor* Descriptors;
    uint32_t Count;
  };

  TypeEntry Entries_[MaxNodeTypes]{};
};

//=============================================================================
// Registration Helpers
//=============================================================================

struct PropRegistrar
{
  PropRegistrar(NodeType Type, const PropDescriptor* Descriptors, uint32_t Count)
  {
    PropertyRegistry::Get().Register(Type, Descriptors, Count);
  }
};

//=============================================================================
// PropDescriptorBuilder (fluent API for constructing descriptors)
//=============================================================================

struct PropDescriptorBuilder
{
  PropDescriptor Desc;

  PropDescriptorBuilder& Range(float Min, float Max, float Step = 0.0f)
  {
    Desc.Min = Min;
    Desc.Max = Max;
    Desc.Step = Step;
    return (*this);
  }

  template<size_t N>
  PropDescriptorBuilder& Enum(const EnumEntry (&Entries)[N])
  {
    Desc.Type = PropType::Enum;
    Desc.EnumEntries = Entries;
    Desc.EnumCount = static_cast<uint8_t>(N);
    return (*this);
  }

  PropDescriptorBuilder& Category(const char* Cat)
  {
    Desc.Category = Cat;
    return (*this);
  }

  PropDescriptorBuilder& OnChanged(void (*Callback)(Node*))
  {
    Desc.OnChanged = Callback;
    return (*this);
  }

  PropDescriptorBuilder& ReadOnly()
  {
    Desc.Flags = Desc.Flags | PropFlags::ReadOnly;
    return (*this);
  }

  operator PropDescriptor() const { return (Desc); }
};

//=============================================================================
// Offset computation from pointer-to-member
//=============================================================================

template<typename Class, typename T>
uint16_t ComputePropOffset(Property<T> Class::*Member)
{
  return (static_cast<uint16_t>(
    reinterpret_cast<uintptr_t>(
      &(static_cast<Class*>(nullptr)->*Member)
    )
  ));
}

//=============================================================================
// MakePropDesc — type-deducing descriptor factory
//=============================================================================

template<typename Class, typename T>
PropDescriptorBuilder MakePropDesc(const char* Name, Property<T> Class::*Member, const T& Default)
{
  PropDescriptorBuilder B{};
  B.Desc.Name = Name;
  B.Desc.Offset = ComputePropOffset(Member);
  B.Desc.Flags = PropFlags::Serialize | PropFlags::EditorVisible;
  B.Desc.Category = nullptr;
  B.Desc.Min = 0.0f;
  B.Desc.Max = 0.0f;
  B.Desc.Step = 0.0f;
  B.Desc.EnumEntries = nullptr;
  B.Desc.EnumCount = 0;
  B.Desc.OnChanged = nullptr;
  B.Desc.FloatDefault = 0.0f;

  if constexpr (std::is_same_v<T, float>) {
    B.Desc.Type = PropType::Float;
    B.Desc.FloatDefault = Default;
  } else if constexpr (std::is_same_v<T, int32_t> || std::is_same_v<T, int>) {
    B.Desc.Type = PropType::Int32;
    B.Desc.IntDefault = static_cast<int32_t>(Default);
  } else if constexpr (std::is_same_v<T, uint32_t>) {
    B.Desc.Type = PropType::UInt32;
    B.Desc.UIntDefault = Default;
  } else if constexpr (std::is_same_v<T, bool>) {
    B.Desc.Type = PropType::Bool;
    B.Desc.BoolDefault = Default;
  } else if constexpr (std::is_same_v<T, std::string>) {
    B.Desc.Type = PropType::String;
    B.Desc.IntDefault = 0;
  } else if constexpr (std::is_same_v<T, Vec3>) {
    B.Desc.Type = PropType::Vec3;
  } else if constexpr (std::is_same_v<T, Vec4>) {
    B.Desc.Type = PropType::Vec4;
  } else if constexpr (std::is_enum_v<T>) {
    B.Desc.Type = PropType::Enum;
    B.Desc.IntDefault = static_cast<int32_t>(Default);
  }

  return (B);
}

//=============================================================================
// AX_PROP / AX_REGISTER_PROPERTIES macros
//=============================================================================

#define AX_PROP(Name, MemberPtr, ...) \
  MakePropDesc(Name, MemberPtr, __VA_ARGS__)

#define AX_REGISTER_PROPERTIES(NodeClass, ...) \
  static const PropDescriptor NodeClass##_Props[] = { __VA_ARGS__ }; \
  static PropRegistrar NodeClass##_Registrar( \
    NodeClass::StaticType, \
    NodeClass##_Props, \
    static_cast<uint32_t>(sizeof(NodeClass##_Props) / sizeof(PropDescriptor)) \
  )

//=============================================================================
// Type-Erased Property Access Helpers
//
// These use the PropDescriptor's byte offset to read/write Property<T>
// values on any node, enabling generic property enumeration without
// knowing the concrete node type at compile time.
//=============================================================================

inline Property<float>* GetPropertyPtr_Float(Node* N, const PropDescriptor* D)
{
  return (reinterpret_cast<Property<float>*>(reinterpret_cast<char*>(N) + D->Offset));
}

inline Property<int32_t>* GetPropertyPtr_Int32(Node* N, const PropDescriptor* D)
{
  return (reinterpret_cast<Property<int32_t>*>(reinterpret_cast<char*>(N) + D->Offset));
}

inline Property<uint32_t>* GetPropertyPtr_UInt32(Node* N, const PropDescriptor* D)
{
  return (reinterpret_cast<Property<uint32_t>*>(reinterpret_cast<char*>(N) + D->Offset));
}

inline Property<bool>* GetPropertyPtr_Bool(Node* N, const PropDescriptor* D)
{
  return (reinterpret_cast<Property<bool>*>(reinterpret_cast<char*>(N) + D->Offset));
}

inline Property<std::string>* GetPropertyPtr_String(Node* N, const PropDescriptor* D)
{
  return (reinterpret_cast<Property<std::string>*>(reinterpret_cast<char*>(N) + D->Offset));
}

inline Property<Vec3>* GetPropertyPtr_Vec3(Node* N, const PropDescriptor* D)
{
  return (reinterpret_cast<Property<Vec3>*>(reinterpret_cast<char*>(N) + D->Offset));
}

inline Property<Vec4>* GetPropertyPtr_Vec4(Node* N, const PropDescriptor* D)
{
  return (reinterpret_cast<Property<Vec4>*>(reinterpret_cast<char*>(N) + D->Offset));
}

inline void SetPropertyFloat(Node* N, const PropDescriptor* D, float Value)
{
  *GetPropertyPtr_Float(N, D) = Value;
}

inline void SetPropertyInt32(Node* N, const PropDescriptor* D, int32_t Value)
{
  *GetPropertyPtr_Int32(N, D) = Value;
}

inline void SetPropertyUInt32(Node* N, const PropDescriptor* D, uint32_t Value)
{
  *GetPropertyPtr_UInt32(N, D) = Value;
}

inline void SetPropertyBool(Node* N, const PropDescriptor* D, bool Value)
{
  *GetPropertyPtr_Bool(N, D) = Value;
}

inline void SetPropertyString(Node* N, const PropDescriptor* D, const std::string& Value)
{
  *GetPropertyPtr_String(N, D) = Value;
}

inline void SetPropertyVec3(Node* N, const PropDescriptor* D, const Vec3& Value)
{
  *GetPropertyPtr_Vec3(N, D) = Value;
}

inline void SetPropertyVec4(Node* N, const PropDescriptor* D, const Vec4& Value)
{
  *GetPropertyPtr_Vec4(N, D) = Value;
}

inline void SetPropertyEnum(Node* N, const PropDescriptor* D, const char* ValueStr)
{
  int32_t EnumVal = 0;
  if (StringToEnum(D->EnumEntries, D->EnumCount, ValueStr, &EnumVal)) {
    *GetPropertyPtr_Int32(N, D) = EnumVal;
  }
}

inline void SetPropertyFromNumeric(Node* N, const PropDescriptor* D, float NumericValue)
{
  switch (D->Type) {
    case PropType::Float:  SetPropertyFloat(N, D, NumericValue); break;
    case PropType::Int32:  SetPropertyInt32(N, D, static_cast<int32_t>(NumericValue)); break;
    case PropType::UInt32: SetPropertyUInt32(N, D, static_cast<uint32_t>(NumericValue)); break;
    default: break;
  }
}

inline float GetPropertyFloat(const Node* N, const PropDescriptor* D)
{
  return (*reinterpret_cast<const Property<float>*>(
    reinterpret_cast<const char*>(N) + D->Offset));
}

inline int32_t GetPropertyInt32(const Node* N, const PropDescriptor* D)
{
  return (*reinterpret_cast<const Property<int32_t>*>(
    reinterpret_cast<const char*>(N) + D->Offset));
}

inline uint32_t GetPropertyUInt32(const Node* N, const PropDescriptor* D)
{
  return (*reinterpret_cast<const Property<uint32_t>*>(
    reinterpret_cast<const char*>(N) + D->Offset));
}

inline bool GetPropertyBool(const Node* N, const PropDescriptor* D)
{
  return (*reinterpret_cast<const Property<bool>*>(
    reinterpret_cast<const char*>(N) + D->Offset));
}

inline const std::string& GetPropertyString(const Node* N, const PropDescriptor* D)
{
  return (*reinterpret_cast<const Property<std::string>*>(
    reinterpret_cast<const char*>(N) + D->Offset));
}

inline Vec3 GetPropertyVec3(const Node* N, const PropDescriptor* D)
{
  return (*reinterpret_cast<const Property<Vec3>*>(
    reinterpret_cast<const char*>(N) + D->Offset));
}

inline Vec4 GetPropertyVec4(const Node* N, const PropDescriptor* D)
{
  return (*reinterpret_cast<const Property<Vec4>*>(
    reinterpret_cast<const char*>(N) + D->Offset));
}
