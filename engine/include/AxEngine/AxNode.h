#pragma once

/**
 * AxNode.h - Node Base System for Hybrid Node Tree + Component Architecture
 *
 * Nodes are lightweight scene tree elements with transforms, names, ordered
 * children, and component registries. They follow the existing AxSceneObject
 * hierarchy pattern (Parent/FirstChild/NextSibling) but as proper C++ classes.
 *
 * This is engine-level code (C++). Foundation types (AxTransform, AxHashTable,
 * AxAllocator) are C11 structs used here but not modified.
 */

#include "Foundation/AxTypes.h"
#include <string>
#include <string_view>

struct AxHashTable;
struct AxHashTableAPI;

class Component;

/**
 * Node type discriminator for the scene hierarchy.
 */
enum class NodeType : uint32_t
{
  Base = 0,
  Node2D,
  Node3D,
  Root
};

/**
 * Node - Base class for all scene hierarchy elements.
 *
 * Contains Parent/FirstChild/NextSibling pointers for efficient
 * hierarchy traversal. Each node holds a local transform relative
 * to its parent, a name, and a component registry backed by
 * AxHashTable for type-to-component mapping.
 */
class Node
{
public:
  Node(std::string_view Name, NodeType Type, AxHashTableAPI* TableAPI);
  virtual ~Node();

  // Non-copyable
  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;

  //=========================================================================
  // Hierarchy Manipulation
  //=========================================================================

  /** Add a child node to the end of the child list. */
  void AddChild(Node* Child);

  /** Remove a direct child node from this node's child list. */
  void RemoveChild(Node* Child);

  /**
   * Set this node's parent, removing from old parent and adding to new.
   * Pass nullptr to detach from any parent.
   */
  void SetParent(Node* NewParent);

  /** Find a direct child by name. Returns nullptr if not found. */
  Node* FindChild(std::string_view ChildName);

  /** Get the number of direct children. */
  uint32_t GetChildCount() const;

  //=========================================================================
  // Lifecycle
  //=========================================================================

  /** Initialize this node and recursively initialize all children. */
  virtual void Initialize();

  /** Shutdown this node and recursively shut down all children. */
  virtual void Shutdown();

  //=========================================================================
  // Component Operations
  //=========================================================================

  /**
   * Add a component to this node. One component per type is enforced.
   * Invokes OnAttach lifecycle hook on the component.
   * @param Comp Component to add (must not be nullptr).
   * @return true if added, false if a component with the same TypeID already exists.
   */
  bool AddComponent(Component* Comp);

  /**
   * Remove a component by its TypeID.
   * Invokes OnDetach lifecycle hook on the component.
   * @param TypeID The type identifier of the component to remove.
   * @return true if removed, false if no component with that TypeID exists.
   */
  bool RemoveComponent(uint32_t TypeID);

  /**
   * Get a component by its TypeID.
   * @param TypeID The type identifier.
   * @return Pointer to the component, or nullptr if not found.
   */
  Component* GetComponent(uint32_t TypeID) const;

  /**
   * Check if a component with the given TypeID exists on this node.
   * @param TypeID The type identifier.
   * @return true if the component exists.
   */
  bool HasComponent(uint32_t TypeID) const;

  /**
   * Template helper to get a component by its static TypeID.
   * Requires T to have a static constexpr uint32_t TypeID member.
   */
  template<typename T>
  T* GetComponent() const
  {
    return (static_cast<T*>(GetComponent(T::TypeID)));
  }

  //=========================================================================
  // Accessors
  //=========================================================================

  std::string_view GetName() const { return (Name_); }
  NodeType GetType() const { return (Type_); }
  uint32_t GetNodeID() const { return (NodeID_); }
  void SetNodeID(uint32_t ID) { NodeID_ = ID; }

  bool IsInitialized() const { return (IsInitialized_); }
  bool IsActive() const { return (IsActive_); }
  void SetActive(bool Active) { IsActive_ = Active; }

  AxTransform& GetTransform() { return (Transform_); }
  const AxTransform& GetTransform() const { return (Transform_); }

  Node* GetParent() const { return (Parent_); }
  Node* GetFirstChild() const { return (FirstChild_); }
  Node* GetNextSibling() const { return (NextSibling_); }

protected:
  std::string Name_;
  NodeType Type_;
  AxTransform Transform_;
  uint32_t NodeID_;

  Node* Parent_;
  Node* FirstChild_;
  Node* NextSibling_;

  // Component registry: maps TypeID string key to Component pointer
  AxHashTable* ComponentRegistry_;
  AxHashTableAPI* HashTableAPI_;

  // Flags
  bool IsInitialized_;
  bool IsActive_;
};

/**
 * Node2D - Lightweight subclass for 2D scene elements.
 */
class Node2D : public Node
{
public:
  Node2D(std::string_view Name, AxHashTableAPI* TableAPI)
    : Node(Name, NodeType::Node2D, TableAPI) {}
};

/**
 * Node3D - Lightweight subclass for 3D scene elements.
 */
class Node3D : public Node
{
public:
  Node3D(std::string_view Name, AxHashTableAPI* TableAPI)
    : Node(Name, NodeType::Node3D, TableAPI) {}
};

/**
 * RootNode - The single root of a scene hierarchy.
 * One per scene, serves as the top-level parent for all nodes.
 */
class RootNode : public Node
{
public:
  RootNode(AxHashTableAPI* TableAPI)
    : Node("Root", NodeType::Root, TableAPI) {}
};
