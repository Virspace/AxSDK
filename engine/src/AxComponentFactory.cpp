/**
 * AxComponentFactory.cpp - Component Factory Implementation
 *
 * Manages runtime registration and creation of component types. Uses an
 * internal std::unordered_map to map type name strings to ComponentTypeInfo.
 * TypeIDs are auto-assigned via an incrementing counter starting at 1.
 *
 * Memory for component instances is allocated/freed through the caller's
 * AxAllocator (polymorphic, per DEC-006).
 *
 * The global ComponentFactory is a plain static object — it initializes at
 * DLL load and destructs at DLL unload with no Init/Term calls required.
 */

#include "AxEngine/AxComponentFactory.h"
#include "AxEngine/AxComponent.h"

#include <cstdio>
#include <string>

//=============================================================================
// Construction / Destruction
//=============================================================================

ComponentFactory::ComponentFactory() = default;

ComponentFactory::~ComponentFactory() = default;

//=============================================================================
// Type Registration
//=============================================================================

bool ComponentFactory::RegisterType(std::string_view TypeName,
                                    ComponentCreateFunc CreateFunc,
                                    ComponentDestroyFunc DestroyFunc,
                                    size_t DataSize)
{
  if (TypeName.empty()) {
    return (false);
  }
  if (!CreateFunc) {
    return (false);
  }

  std::string Key(TypeName);

  // Check for duplicate registration
  if (TypeMap_.find(Key) != TypeMap_.end()) {
    return (false);
  }

  ComponentTypeInfo Info;
  Info.TypeName   = Key;
  Info.TypeID     = NextTypeID_++;
  Info.CreateFunc  = CreateFunc;
  Info.DestroyFunc = DestroyFunc;
  Info.DataSize    = DataSize;

  TypeMap_.emplace(Key, std::move(Info));
  InsertionOrder_.push_back(Key);

  return (true);
}

bool ComponentFactory::UnregisterType(std::string_view TypeName)
{
  if (TypeName.empty()) {
    return (false);
  }

  std::string Key(TypeName);

  auto It = TypeMap_.find(Key);
  if (It == TypeMap_.end()) {
    return (false);
  }

  TypeMap_.erase(It);

  // Remove from insertion-order list
  for (auto OIt = InsertionOrder_.begin(); OIt != InsertionOrder_.end(); ++OIt) {
    if (*OIt == Key) {
      InsertionOrder_.erase(OIt);
      break;
    }
  }

  return (true);
}

//=============================================================================
// Component Creation / Destruction
//=============================================================================

Component* ComponentFactory::CreateComponent(std::string_view TypeName,
                                             AxAllocator* Allocator)
{
  if (TypeName.empty() || !Allocator) {
    return (nullptr);
  }

  std::string Key(TypeName);

  auto It = TypeMap_.find(Key);
  if (It == TypeMap_.end()) {
    return (nullptr);
  }

  const ComponentTypeInfo& Info = It->second;

  // Allocate memory through the provided allocator
  void* Memory = AxAlloc(Allocator, Info.DataSize);
  if (!Memory) {
    return (nullptr);
  }

  // Call the create function to construct the component in-place
  Component* Comp = Info.CreateFunc(Memory);
  if (!Comp) {
    AxFree(Allocator, Memory);
    return (nullptr);
  }

  // Set the TypeID and TypeName on the created component
  Comp->TypeID_   = Info.TypeID;
  Comp->TypeName_ = Info.TypeName;

  return (Comp);
}

void ComponentFactory::DestroyComponent(Component* Comp,
                                        AxAllocator* Allocator)
{
  if (!Comp || !Allocator) {
    return;
  }

  // Find the type info to get the DestroyFunc
  if (!Comp->TypeName_.empty()) {
    auto It = TypeMap_.find(Comp->TypeName_);
    if (It != TypeMap_.end()) {
      const ComponentTypeInfo& Info = It->second;
      if (Info.DestroyFunc) {
        Info.DestroyFunc(Comp);
      }
    }
  }

  // Free memory through the allocator
  AxFree(Allocator, static_cast<void*>(Comp));
}

//=============================================================================
// Type Queries
//=============================================================================

const ComponentTypeInfo* ComponentFactory::FindType(std::string_view TypeName) const
{
  if (TypeName.empty()) {
    return (nullptr);
  }

  std::string Key(TypeName);

  auto It = TypeMap_.find(Key);
  if (It == TypeMap_.end()) {
    return (nullptr);
  }

  return (&It->second);
}

uint32_t ComponentFactory::GetTypeCount() const
{
  return (static_cast<uint32_t>(TypeMap_.size()));
}

const ComponentTypeInfo* ComponentFactory::GetTypeAtIndex(uint32_t Index) const
{
  if (Index >= static_cast<uint32_t>(InsertionOrder_.size())) {
    return (nullptr);
  }

  const std::string& Key = InsertionOrder_[Index];

  auto It = TypeMap_.find(Key);
  if (It == TypeMap_.end()) {
    return (nullptr);
  }

  return (&It->second);
}

//=============================================================================
// Global Instance
//=============================================================================

static ComponentFactory gComponentFactory;

/**
 * Get the global ComponentFactory instance (engine-internal use).
 */
ComponentFactory* AxComponentFactory_Get()
{
  return (&gComponentFactory);
}
