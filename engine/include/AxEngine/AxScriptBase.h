#pragma once

#include "Foundation/AxTypes.h"
#include "AxEngine/AxSceneTree.h"

/**
 * AxScriptBase.h - Script Base Class for Node Behavioral Scripting
 *
 * ScriptBase is the base class for all behavioral scripts attached to Nodes.
 * One script per node. Scripts are pure behavior — components remain pure data.
 *
 * Lifecycle:
 *   OnAttach()    - Immediately on attachment; no tree queries safe yet
 *   OnInit()      - Bottom-up after subtree is fully attached (SceneTree driven)
 *   OnEnable()    - On transition to active (including first activation, after OnInit)
 *   OnDisable()   - On transition to inactive, or before OnDetach
 *   OnDetach()    - After OnDisable; pair cleanup with OnAttach
 *
 * Per-frame (suppressed when node is inactive or not yet initialized):
 *   OnUpdate(float DeltaT)
 *   OnFixedUpdate(float DeltaT)
 *   OnLateUpdate(float DeltaT)
 *
 * Owner and IsInitialized are managed by Node and SceneTree respectively.
 */

// Forward declarations
class Node;
class SceneTree;
class CameraNode;

/**
 * ScriptBase - Base class for all behavioral scripts attached to Nodes.
 *
 * Subclass ScriptBase, override the callbacks you need, and attach an
 * instance to a node via node->AttachScript(new MyScript()). The node
 * takes ownership of the script from that point.
 */
class ScriptBase
{
public:
  virtual ~ScriptBase() = default;

  // Non-copyable
  ScriptBase(const ScriptBase&) = delete;
  ScriptBase& operator=(const ScriptBase&) = delete;

  //=========================================================================
  // Lifecycle — structural
  //=========================================================================

  /** Called immediately on attachment; no tree queries safe here. */
  virtual void OnAttach()  {}

  /** Called bottom-up after subtree is fully attached. One call per lifetime. */
  virtual void OnInit()    {}

  /** Called on each transition to active, including the first activation after OnInit. */
  virtual void OnEnable()  {}

  /** Called on each transition to inactive, or immediately before OnDetach. */
  virtual void OnDisable() {}

  /** Called after OnDisable; pair cleanup code with OnAttach. */
  virtual void OnDetach()  {}

  //=========================================================================
  // Lifecycle — per-frame
  // (suppressed when node is inactive or IsInitialized_ is false)
  //=========================================================================

  virtual void OnUpdate(float DeltaT)      { (void)DeltaT; }
  virtual void OnFixedUpdate(float DeltaT) { (void)DeltaT; }
  virtual void OnLateUpdate(float DeltaT)  { (void)DeltaT; }

  //=========================================================================
  // Accessors
  //=========================================================================

  Node* GetOwner() const { return (Owner_); }
  bool IsInitialized() const { return (IsInitialized_); }

protected:
  ScriptBase() = default;

  /** Main camera — set by SceneTree before each traversal. */
  CameraNode* MainCamera{nullptr};

  /** Mouse delta this frame — set by SceneTree before each traversal. */
  AxVec2 MouseDelta{0.0f, 0.0f};

  /** The SceneTree this script's node belongs to — set by SceneTree. */
  SceneTree* Tree_{nullptr};

  /** Access the owning SceneTree (e.g., for runtime node creation). */
  SceneTree* GetSceneTree() const { return (Tree_); }

  /**
   * Create a typed node via the SceneTree, returning the correct subclass pointer.
   * The node is initially parented to Root; call AddChild() to reparent to Owner.
   */
  template<typename T>
  T* CreateNode(std::string_view Name)
  {
    if (!Tree_) { return (nullptr); }
    return (static_cast<T*>(Tree_->CreateNode(Name, T::StaticType)));
  }

  /** Add a child node to this script's owner node. */
  void AddChild(Node* Child)
  {
    if (Owner_ && Child) { Owner_->AddChild(Child); }
  }

private:
  Node* Owner_         = nullptr;
  bool  IsInitialized_ = false;

  friend class Node;
  friend class SceneTree;
};

//=============================================================================
// AX_IMPLEMENT_SCRIPT — Place at the bottom of your game script .cpp file.
// Generates the exported factory function the engine uses to instantiate
// your script from the Game DLL.
//
// Example:
//   class Game : public ScriptBase { ... };
//   AX_IMPLEMENT_SCRIPT(Game)
//=============================================================================
#define AX_IMPLEMENT_SCRIPT(ClassName) \
  extern "C" AXON_DLL_EXPORT ScriptBase* CreateNodeScript() \
  { \
    return (new ClassName()); \
  }
