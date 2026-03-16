#pragma once

#include "Foundation/AxTypes.h"
#include "AxEngine/AxSceneTree.h"
#include "AxEngine/AxDebugDraw.h"
#include "AxEngine/AxScriptLog.h"

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

  //=========================================================================
  // Process Control
  //=========================================================================

  /** Enable or disable OnUpdate dispatch for this script. Default: true. */
  void SetProcessing(bool Enabled) { Processing_ = Enabled; }

  /** Enable or disable OnFixedUpdate dispatch for this script. Default: true. */
  void SetPhysicsProcessing(bool Enabled) { PhysicsProcessing_ = Enabled; }

  /** Query whether OnUpdate dispatch is enabled. */
  bool IsProcessing() const { return (Processing_); }

  /** Query whether OnFixedUpdate dispatch is enabled. */
  bool IsPhysicsProcessing() const { return (PhysicsProcessing_); }

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

  /** Find a node by relative path from this script's owner. */
  Node* GetNode(std::string_view Path)
  {
    if (Owner_) { return (Owner_->GetNode(Path)); }
    return (nullptr);
  }

  /** Type-safe variant of GetNode. */
  template<typename T>
  T* GetNode(std::string_view Path)
  {
    if (Owner_) { return (Owner_->GetNode<T>(Path)); }
    return (nullptr);
  }

  /** Add this script's owner to a named group. */
  void AddToGroup(std::string_view GroupName)
  {
    if (Owner_) { Owner_->AddToGroup(GroupName); }
  }

  //=========================================================================
  // Signals — convenience wrappers that delegate to Owner_ node
  //=========================================================================

  void EmitSignal(std::string_view Name)
  {
    if (Owner_) { Owner_->EmitSignal(Name); }
  }

  void EmitSignal(std::string_view Name, float Arg0)
  {
    if (Owner_) { Owner_->EmitSignal(Name, Arg0); }
  }

  void EmitSignal(std::string_view Name, float Arg0, float Arg1)
  {
    if (Owner_) { Owner_->EmitSignal(Name, Arg0, Arg1); }
  }

  uint32_t Connect(Node* Emitter, std::string_view SignalName, SignalCallback Callback)
  {
    if (Emitter) { return (Emitter->Connect(SignalName, std::move(Callback), Owner_)); }
    return (0);
  }

  //=========================================================================
  // Debug — draw and log utilities grouped under Debug member
  //
  // Usage: Debug.DrawLine(...), Debug.LogError("msg"), etc.
  // Log methods automatically include the owner node name.
  // Draw methods are no-ops in shipping builds.
  //=========================================================================

  struct DebugHelper
  {
    ScriptBase& Self;

    // Draw
    void DrawLine(const Vec3& From, const Vec3& To, const Vec4& Color)
    {
      DebugDraw::DrawLine(From, To, Color);
    }

    void DrawRay(const Vec3& Origin, const Vec3& Direction, float Length, const Vec4& Color)
    {
      DebugDraw::DrawRay(Origin, Direction, Length, Color);
    }

    void DrawBox(const Vec3& Center, const Vec3& HalfExtents, const Vec4& Color)
    {
      DebugDraw::DrawBox(Center, HalfExtents, Color);
    }

    void DrawSphere(const Vec3& Center, float Radius, const Vec4& Color, int Segments = 16)
    {
      DebugDraw::DrawSphere(Center, Radius, Color, Segments);
    }

    // Log
    void LogError(std::string_view Message)   { Log_(AX_LOG_LEVEL_ERROR, Message); }
    void LogWarn(std::string_view Message)    { Log_(AX_LOG_LEVEL_WARNING, Message); }
    void LogInfo(std::string_view Message)    { Log_(AX_LOG_LEVEL_INFO, Message); }
    void LogDebug(std::string_view Message)   { Log_(AX_LOG_LEVEL_DEBUG, Message); }

  private:
    void Log_(int Level, std::string_view Message)
    {
      std::string_view Name = Self.Owner_ ? Self.Owner_->GetName() : "detached";
      LogBuffer::Get().Push(Level, Name, Message);
    }
  };

  DebugHelper Debug{*this};

private:

  Node* Owner_              = nullptr;
  bool  IsInitialized_      = false;
  bool  Processing_         = true;
  bool  PhysicsProcessing_  = true;

  friend class Node;
  friend class SceneTree;
};

//=============================================================================
// AX_IMPLEMENT_SCRIPT — Place at the bottom of your game script .cpp file.
// Auto-registers the script class with ScriptRegistry via a static initializer.
// Works in both DLL mode (registration on DLL load) and monolithic shipping
// (registration at program startup).
//
// Example:
//   class Game : public ScriptBase { ... };
//   AX_IMPLEMENT_SCRIPT(Game)
//=============================================================================
#include "AxEngine/AxScriptRegistry.h"

#define AX_IMPLEMENT_SCRIPT(ClassName) \
  static ScriptBase* _CreateScript_##ClassName() { return (new ClassName()); } \
  static struct _ScriptReg_##ClassName { \
    _ScriptReg_##ClassName() { ScriptRegistry::Get().Register(#ClassName, _CreateScript_##ClassName); } \
  } _s_scriptReg_##ClassName;
