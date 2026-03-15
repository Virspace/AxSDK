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
#include "AxEngine/AxMathTypes.h"
#include "AxEngine/AxTransformType.h"
#include "AxEngine/AxSignal.h"
#include <string>
#include <string_view>

struct AxHashTable;
struct AxHashTableAPI;

class ScriptBase;
class SceneTree;

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
  // Children Iteration
  //=========================================================================

  /** Lightweight range adapter for iterating direct children via range-for. */
  class ChildRange
  {
  public:
    struct Iterator
    {
      Node* Current;
      Node* operator*() const { return (Current); }
      Iterator& operator++() { Current = Current->GetNextSibling(); return (*this); }
      bool operator!=(const Iterator& Other) const { return (Current != Other.Current); }
    };

    explicit ChildRange(Node* First) : First_(First) {}
    Iterator begin() const { return {First_}; }
    Iterator end() const { return {nullptr}; }

  private:
    Node* First_;
  };

  /** Get an iterable range of direct children. Zero allocation. */
  ChildRange GetChildren() const { return (ChildRange(FirstChild_)); }

  //=========================================================================
  // Scene Queries
  //=========================================================================

  /** Find a node by relative path. Supports "/" for child traversal, ".." for parent.
   *  Returns nullptr if any segment fails to resolve. Empty string returns this. */
  Node* GetNode(std::string_view Path);

  /** Type-safe variant. Returns nullptr if the node exists but is the wrong type. */
  template<typename T>
  T* GetNode(std::string_view Path)
  {
    Node* Found = GetNode(Path);
    if (Found) {
      return (Found->As<T>());
    }
    return (nullptr);
  }

  /** Find the first direct child of the given type. */
  template<typename T>
  T* FindChildByType()
  {
    for (Node* C = FirstChild_; C; C = C->NextSibling_) {
      if (C->GetType() == T::StaticType) {
        return (static_cast<T*>(C));
      }
    }
    return (nullptr);
  }

  //=========================================================================
  // Groups
  //=========================================================================

  /** Add this node to a named group. No-op if already in the group or no OwningTree_. */
  void AddToGroup(std::string_view GroupName);

  /** Remove this node from a named group. No-op if not in the group or no OwningTree_. */
  void RemoveFromGroup(std::string_view GroupName);

  /** Check if this node belongs to a named group. */
  bool IsInGroup(std::string_view GroupName) const;

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
   * If OwningTree_ is set, registers the node in the pending init queue.
   * @param Script Script to attach (must not be nullptr).
   */
  void AttachScript(ScriptBase* Script);

  /**
   * Detach the current script from this node and return ownership to caller.
   * Calls OnDisable() if the node is active and the script is initialized,
   * then calls OnDetach() unconditionally. Clears the script's owner.
   * If OwningTree_ is set, unregisters the node from the script process list.
   * @return The detached script pointer (caller must delete), or nullptr if none.
   */
  ScriptBase* DetachScript();

  /**
   * Get the currently attached script, or nullptr if none.
   */
  ScriptBase* GetScript() const;

  //=========================================================================
  // Signals
  //=========================================================================

  /** Emit a signal with no arguments. */
  void EmitSignal(std::string_view Name);

  /** Emit a signal with a single float argument. */
  void EmitSignal(std::string_view Name, float Arg0);

  /** Emit a signal with two float arguments. */
  void EmitSignal(std::string_view Name, float Arg0, float Arg1);

  /** Emit a signal with a pre-built SignalArgs container. */
  void EmitSignalArgs(std::string_view Name, const SignalArgs& Args);

  /** Connect a callback to a signal on this node. Returns a unique connection ID. */
  uint32_t Connect(std::string_view SignalName, SignalCallback Callback, Node* Receiver = nullptr);

  /** Disconnect a specific callback by signal name and connection ID. */
  void Disconnect(std::string_view SignalName, uint32_t ConnectionID);

  //=========================================================================
  // Accessors
  //=========================================================================

  std::string_view GetName() const { return (Name_); }
  NodeType GetType() const { return (Type_); }

  /** Safe downcast. Returns T* if this node's type matches T::StaticType, nullptr otherwise. */
  template<typename T>
  T* As()
  {
    if (Type_ == T::StaticType) {
      return (static_cast<T*>(this));
    }
    return (nullptr);
  }

  /** Const variant. */
  template<typename T>
  const T* As() const
  {
    if (Type_ == T::StaticType) {
      return (static_cast<const T*>(this));
    }
    return (nullptr);
  }

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

  Transform& GetTransform() { return (Transform_); }
  const Transform& GetTransform() const { return (Transform_); }

  /** Fluent transform setters -- delegate to Transform, which auto-notifies SceneTree. */
  Node& SetPosition(float X, float Y, float Z);
  Node& SetPosition(const Vec3& Pos);
  Node& SetRotation(const Quat& Rot);
  Node& SetRotation(float PitchRad, float YawRad, float RollRad);
  Node& SetScale(float X, float Y, float Z);
  Node& SetScale(const Vec3& S);

  /**
   * Get the cached world transform matrix.
   * This matrix is populated by SceneTree::UpdateNodeTransforms at the
   * top of each Update() call.
   * @return Reference to the cached world matrix.
   */
  const Mat4& GetWorldTransform() const { return (WorldMatrix_); }

  Node* GetParent() const { return (Parent_); }
  Node* GetFirstChild() const { return (FirstChild_); }
  Node* GetNextSibling() const { return (NextSibling_); }

  /** Get the SceneTree that owns this node, or nullptr if standalone. */
  SceneTree* GetOwningTree() const { return (OwningTree_); }

protected:
  std::string Name_;
  NodeType Type_;
  Transform Transform_;
  Mat4 WorldMatrix_;
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

private:
  // Signal storage
  std::vector<SignalSlot> Signals_;
  uint32_t NextConnectionID_{1};
  std::vector<OutgoingConnection> OutgoingConnections_;

  /** Clean up all signal connections (called from destructor). */
  void CleanupSignals();

  // Back-pointer to the owning SceneTree (set by SceneTree::CreateNode,
  // cleared by SceneTree::DestroyNode). Enables Node to notify SceneTree
  // of transform changes and script attach/detach without callers passing
  // SceneTree explicitly.
  SceneTree* OwningTree_;

  // True if this node is currently in the SceneTree's TransformDirtyRoots_ list.
  // Used for O(1) duplicate prevention.
  bool InDirtyList_;

  friend class SceneTree;
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
