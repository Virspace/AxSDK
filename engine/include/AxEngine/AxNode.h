#pragma once

/**
 * AxNode.h - Node Base System for Godot-Style Typed Node Hierarchy
 *
 * Nodes are lightweight scene tree elements with transforms, names, and ordered
 * children. They follow the Parent/FirstChild/NextSibling hierarchy pattern
 * as proper C++ classes. Typed node subclasses carry their data directly
 * (e.g. MeshInstance holds mesh path and model handle, CameraNode holds AxCamera).
 *
 * This is engine-level code (C++). Foundation types (AxTransform, AxHashTable,
 * AxAllocator) are C11 structs used here but not modified.
 */

#include "Foundation/AxTypes.h"
#include <string>
#include <string_view>

struct AxHashTable;
struct AxHashTableAPI;

class ScriptBase;

/**
 * Node type discriminator for the scene hierarchy.
 * Each typed node subclass sets its Type_ to the corresponding enum value.
 */
enum class NodeType : uint32_t
{
  Base = 0,
  Node2D,
  Node3D,
  Root,
  // 3D typed nodes
  MeshInstance,
  Camera,
  Light,
  RigidBody,
  Collider,
  AudioSource,
  AudioListener,
  Animator,
  ParticleEmitter,
  // 2D typed nodes
  Sprite
};

/**
 * Node - Base class for all scene hierarchy elements.
 *
 * Contains Parent/FirstChild/NextSibling pointers for efficient
 * hierarchy traversal. Each node holds a local transform relative
 * to its parent and a name.
 *
 * Each node may have at most one behavioral script attached via
 * AttachScript. The node owns the script and destroys it on destruction.
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
  // Script Operations
  //=========================================================================

  /**
   * Attach a behavioral script to this node. Takes ownership.
   * If a script is already attached, it is fully detached and destroyed first.
   * Immediately calls Script->OnAttach().
   * @param Script Script to attach (must not be nullptr).
   */
  void AttachScript(ScriptBase* Script);

  /**
   * Detach the current script from this node and return ownership to caller.
   * Calls OnDisable() if the node is active and the script is initialized,
   * then calls OnDetach() unconditionally. Clears the script's owner.
   * @return The detached script pointer (caller must delete), or nullptr if none.
   */
  ScriptBase* DetachScript();

  /**
   * Get the currently attached script, or nullptr if none.
   */
  ScriptBase* GetScript() const;

  //=========================================================================
  // Accessors
  //=========================================================================

  std::string_view GetName() const { return (Name_); }
  NodeType GetType() const { return (Type_); }
  uint32_t GetNodeID() const { return (NodeID_); }
  void SetNodeID(uint32_t ID) { NodeID_ = ID; }

  bool IsInitialized() const { return (IsInitialized_); }
  bool IsActive() const { return (IsActive_); }

  /**
   * Set the active state of this node.
   * Fires OnDisable on the script when transitioning to inactive (if initialized).
   * Fires OnEnable on the script when transitioning to active (if initialized).
   */
  void SetActive(bool Active);

  AxTransform& GetTransform() { return (Transform_); }
  const AxTransform& GetTransform() const { return (Transform_); }

  /**
   * Get the cached world transform matrix.
   * This matrix is populated by SceneTree::UpdateNodeTransforms at the
   * top of each Update() call. Returns the CachedForwardMatrix from
   * the node's transform.
   * @return Reference to the cached world matrix.
   */
  const AxMat4x4& GetWorldTransform() const { return (Transform_.CachedForwardMatrix); }

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

  AxHashTableAPI* HashTableAPI_;

  // Script slot -- one behavioral script per node; node owns it
  ScriptBase* Script_;

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
