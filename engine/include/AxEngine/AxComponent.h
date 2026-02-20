#pragma once

/**
 * AxComponent.h - Component Base Class for Hybrid Node Tree + Component Architecture
 *
 * Components are data containers with optional behavior that attach to Nodes.
 * One component per type per node is enforced. Components have lifecycle hooks
 * (OnAttach, OnDetach, OnInitialize, OnShutdown) and an editor integration
 * method (OnInspectorGUI).
 *
 * IMPORTANT: The first field (TypeID_) MUST remain at offset 0. AxNode.cpp
 * reads the TypeID via reinterpret_cast<uint32_t*>(Comp) to avoid including
 * this header in AxNode.cpp when Component is forward-declared.
 */

#include "Foundation/AxTypes.h"
#include <string>
#include <string_view>

// Forward declaration
class Node;

/**
 * Component - Base class for all components that attach to Nodes.
 *
 * Components store data and optionally define behavior via virtual methods.
 * The TypeID and TypeName are assigned by the ComponentFactory at registration
 * time and set during component creation.
 */
class Component
{
public:
  // IMPORTANT: TypeID_ MUST be the first field at offset 0.
  // AxNode.cpp reads TypeID via reinterpret_cast<uint32_t*>(Comp).
  uint32_t TypeID_;
  std::string TypeName_;
  Node* Owner_;
  bool IsEnabled_;

  Component()
    : TypeID_(0)
    , TypeName_("")
    , Owner_(nullptr)
    , IsEnabled_(true)
  {}

  virtual ~Component() {}

  // Non-copyable
  Component(const Component&) = delete;
  Component& operator=(const Component&) = delete;

  //=========================================================================
  // Lifecycle Hooks
  //=========================================================================

  /**
   * Called when this component is added to a node.
   * Owner_ is already set when this is called.
   */
  virtual void OnAttach() {}

  /**
   * Called when this component is removed from a node.
   * Owner_ is still valid when this is called; it will be cleared after.
   */
  virtual void OnDetach() {}

  /**
   * Called during the node's Initialize() pass.
   */
  virtual void OnInitialize() {}

  /**
   * Called during the node's Shutdown() pass.
   */
  virtual void OnShutdown() {}

  //=========================================================================
  // Editor Integration
  //=========================================================================

  /**
   * Called by the editor to render inspector GUI for this component.
   * Default implementation is a no-op.
   */
  virtual void OnInspectorGUI() {}

  //=========================================================================
  // Accessors
  //=========================================================================

  uint32_t GetTypeID() const { return (TypeID_); }
  std::string_view GetTypeName() const { return (TypeName_); }
  Node* GetOwner() const { return (Owner_); }
  bool IsEnabled() const { return (IsEnabled_); }
  void SetEnabled(bool Enabled) { IsEnabled_ = Enabled; }

  /**
   * Returns the size of this component instance for allocation purposes.
   * Derived classes should override this to return their actual size.
   */
  virtual size_t GetSize() const { return (sizeof(Component)); }
};
