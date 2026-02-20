#pragma once

/**
 * AxComponentFactory.h - Component Factory for Runtime Type Registration
 *
 * The ComponentFactory enables runtime registration and creation of component
 * types by name string. It is engine-internal and used only during
 * initialization — there is no C API wrapper layer.
 *
 * Each registered type gets an auto-assigned TypeID that is consistent within
 * a session. Components are created and destroyed through the factory using
 * the provided AxAllocator (polymorphic, per DEC-006).
 */

#include "Foundation/AxTypes.h"
#include "Foundation/AxAllocator.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// Forward declarations
class Component;
struct AxAllocator;

/**
 * Function pointer type for creating a component instance.
 * The factory allocates memory via AxAllocator, then calls CreateFunc
 * to placement-construct the component at the allocated address.
 *
 * @param Memory Pointer to allocated memory of at least DataSize bytes.
 * @return Pointer to the constructed Component instance.
 */
typedef Component* (*ComponentCreateFunc)(void* Memory);

/**
 * Function pointer type for destroying a component instance.
 * Called before the factory frees memory via AxAllocator.
 *
 * @param Comp Pointer to the component to destroy.
 */
typedef void (*ComponentDestroyFunc)(Component* Comp);

/**
 * ComponentTypeInfo - Metadata for a registered component type.
 */
struct ComponentTypeInfo
{
  std::string TypeName;           // Registered type name string
  uint32_t TypeID;                // Auto-assigned unique type identifier
  ComponentCreateFunc CreateFunc; // Factory function for creating instances
  ComponentDestroyFunc DestroyFunc; // Factory function for destroying instances
  size_t DataSize;                // Size in bytes for allocation
};

/**
 * ComponentFactory - Registry of component types with runtime creation.
 *
 * Uses an internal std::unordered_map to map type name strings to
 * ComponentTypeInfo. TypeIDs are auto-assigned via an incrementing counter
 * starting at 1. Insertion order is tracked for index-based access.
 */
class ComponentFactory
{
public:
  ComponentFactory();
  ~ComponentFactory();

  // Non-copyable
  ComponentFactory(const ComponentFactory&) = delete;
  ComponentFactory& operator=(const ComponentFactory&) = delete;

  /**
   * Register a new component type.
   * @param TypeName Unique name string for this type (must not be empty).
   * @param CreateFunc Function to construct a component (must not be null).
   * @param DestroyFunc Function to destruct a component (may be null for trivial types).
   * @param DataSize Size in bytes for memory allocation.
   * @return true if registered successfully, false if name is empty, CreateFunc
   *         is null, or a type with the same name is already registered.
   */
  bool RegisterType(std::string_view TypeName, ComponentCreateFunc CreateFunc,
                    ComponentDestroyFunc DestroyFunc, size_t DataSize);

  /**
   * Unregister a previously registered component type.
   * @param TypeName Name of the type to unregister.
   * @return true if the type was found and removed, false otherwise.
   */
  bool UnregisterType(std::string_view TypeName);

  /**
   * Create a component instance of the named type.
   * Allocates memory via the provided AxAllocator, then calls the registered
   * CreateFunc to construct the component in-place. Sets TypeID and TypeName
   * on the created instance.
   *
   * @param TypeName Name of the registered type to create.
   * @param Allocator Allocator to use for memory (polymorphic, per DEC-006).
   * @return Pointer to the created Component, or nullptr if the type is not
   *         registered or allocation fails.
   */
  Component* CreateComponent(std::string_view TypeName, AxAllocator* Allocator);

  /**
   * Destroy a component instance previously created by this factory.
   * Calls the registered DestroyFunc (if any), then frees memory via
   * the provided AxAllocator.
   *
   * @param Comp Component to destroy.
   * @param Allocator Allocator that was used to create this component.
   */
  void DestroyComponent(Component* Comp, AxAllocator* Allocator);

  /**
   * Find the type info for a registered type by name.
   * @param TypeName Name of the type to look up.
   * @return Pointer to the ComponentTypeInfo, or nullptr if not found.
   */
  const ComponentTypeInfo* FindType(std::string_view TypeName) const;

  /**
   * Get the total number of registered component types.
   */
  uint32_t GetTypeCount() const;

  /**
   * Get the type info at a specific index for iteration (insertion order).
   * @param Index Zero-based index (0 to GetTypeCount()-1).
   * @return Pointer to the ComponentTypeInfo, or nullptr if index is out of range.
   */
  const ComponentTypeInfo* GetTypeAtIndex(uint32_t Index) const;

private:
  std::unordered_map<std::string, ComponentTypeInfo> TypeMap_;
  std::vector<std::string> InsertionOrder_; // Tracks registration order for GetTypeAtIndex
  uint32_t NextTypeID_ = 1;                 // Auto-incrementing TypeID counter
};

//=============================================================================
// Global Accessor
//=============================================================================

/**
 * Get the global ComponentFactory instance (engine-internal use).
 * The factory is a static object initialized at DLL load; no Init/Term needed.
 */
ComponentFactory* AxComponentFactory_Get();
