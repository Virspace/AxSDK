/**
 * AxPropertyReflection.cpp - PropertyRegistry Implementation
 *
 * Implements the global property registry and enum string mapping utilities.
 */

#include "AxEngine/AxPropertyReflection.h"

#include <cstring>

//=============================================================================
// Enum String Mapping
//=============================================================================

const char* EnumToString(const EnumEntry* Entries, uint8_t Count, int32_t Value)
{
  if (!Entries || Count == 0) {
    return (nullptr);
  }

  for (uint8_t I = 0; I < Count; ++I) {
    if (Entries[I].Value == Value) {
      return (Entries[I].Name);
    }
  }
  return (nullptr);
}

bool StringToEnum(const EnumEntry* Entries, uint8_t Count, std::string_view Name, int32_t* OutValue)
{
  if (!Entries || !OutValue || Count == 0) {
    return (false);
  }

  for (uint8_t I = 0; I < Count; ++I) {
    if (Name == Entries[I].Name) {
      *OutValue = Entries[I].Value;
      return (true);
    }
  }
  return (false);
}

//=============================================================================
// PropertyRegistry
//=============================================================================

PropertyRegistry& PropertyRegistry::Get()
{
  static PropertyRegistry Instance;
  return (Instance);
}

void PropertyRegistry::Register(NodeType Type, const PropDescriptor* Descriptors, uint32_t Count)
{
  uint32_t Index = static_cast<uint32_t>(Type);
  if (Index < MaxNodeTypes) {
    Entries_[Index].Descriptors = Descriptors;
    Entries_[Index].Count = Count;
  }
}

const PropDescriptor* PropertyRegistry::GetProperties(NodeType Type, uint32_t* OutCount) const
{
  if (!OutCount) {
    return (nullptr);
  }

  uint32_t Index = static_cast<uint32_t>(Type);
  if (Index < MaxNodeTypes && Entries_[Index].Count > 0) {
    *OutCount = Entries_[Index].Count;
    return (Entries_[Index].Descriptors);
  }
  *OutCount = 0;
  return (nullptr);
}

const PropDescriptor* PropertyRegistry::FindProperty(NodeType Type, std::string_view Name) const
{
  uint32_t Index = static_cast<uint32_t>(Type);
  if (Index >= MaxNodeTypes) {
    return (nullptr);
  }

  const TypeEntry& Entry = Entries_[Index];
  for (uint32_t I = 0; I < Entry.Count; ++I) {
    if (Name == Entry.Descriptors[I].Name) {
      return (&Entry.Descriptors[I]);
    }
  }
  return (nullptr);
}
