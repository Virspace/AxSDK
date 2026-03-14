/**
 * AxNode.cpp - Node hierarchy manipulation and script operations
 *
 * Implements the Node base class for the Godot-style typed node hierarchy.
 * Follows the Parent/FirstChild/NextSibling pattern for hierarchy traversal.
 *
 * Transform setters notify the owning SceneTree via MarkTransformDirty()
 * so that only changed subtrees are processed each frame.
 * Script attach/detach notifies the SceneTree's pending init queue and
 * script process list via the OwningTree_ back-pointer.
 */

#include "AxEngine/AxNode.h"
#include "AxEngine/AxScriptBase.h"
#include "AxEngine/AxSceneTree.h"
#include "Foundation/AxAllocator.h"
#include "Foundation/AxHashTable.h"
#include "Foundation/AxMath.h"

//=============================================================================
// Node Construction / Destruction
//=============================================================================

Node::Node(std::string_view Name, NodeType Type, AxHashTableAPI* TableAPI)
  : Name_(Name)
  , Type_(Type)
  , NodeID_(0)
  , Parent_(nullptr)
  , FirstChild_(nullptr)
  , NextSibling_(nullptr)
  , HashTableAPI_(TableAPI)
  , Script_(nullptr)
  , IsInitialized_(false)
  , IsActive_(true)
  , OwningTree_(nullptr)
  , InDirtyList_(false)
{
  // Transform default constructor handles identity initialization
  Transform_.OwningNode_ = this;
  WorldMatrix_ = Mat4::Identity();
}

Node::~Node()
{
  // Shutdown if still initialized
  if (IsInitialized_) {
    Shutdown();
  }

  // Destroy the attached script (DetachScript fires OnDisable/OnDetach first)
  if (Script_) {
    delete DetachScript();
  }

  // Detach from parent to clean up sibling links
  if (Parent_) {
    Parent_->RemoveChild(this);
  }
}

//=============================================================================
// Hierarchy Manipulation
//=============================================================================

void Node::AddChild(Node* Child)
{
  if (!Child || Child == this) {
    return;
  }

  // If the child already has a parent, remove it from the old parent first
  if (Child->Parent_) {
    Child->Parent_->RemoveChild(Child);
  }

  Child->Parent_ = this;
  Child->NextSibling_ = nullptr;

  // Append to end of child list to preserve insertion order
  if (!FirstChild_) {
    FirstChild_ = Child;
  } else {
    Node* Last = FirstChild_;
    while (Last->NextSibling_) {
      Last = Last->NextSibling_;
    }
    Last->NextSibling_ = Child;
  }
}

void Node::RemoveChild(Node* Child)
{
  if (!Child || Child->Parent_ != this) {
    return;
  }

  // Find and unlink the child from the sibling list
  if (FirstChild_ == Child) {
    FirstChild_ = Child->NextSibling_;
  } else {
    Node* Prev = FirstChild_;
    while (Prev && Prev->NextSibling_ != Child) {
      Prev = Prev->NextSibling_;
    }
    if (Prev) {
      Prev->NextSibling_ = Child->NextSibling_;
    }
  }

  Child->Parent_ = nullptr;
  Child->NextSibling_ = nullptr;
}

void Node::SetParent(Node* NewParent)
{
  if (NewParent == Parent_) {
    return;
  }

  // Remove from old parent
  if (Parent_) {
    Parent_->RemoveChild(this);
  }

  // Add to new parent
  if (NewParent) {
    NewParent->AddChild(this);
  } else {
    // Detaching from hierarchy entirely
    Parent_ = nullptr;
    NextSibling_ = nullptr;
  }
}

Node* Node::FindChild(std::string_view ChildName)
{
  if (ChildName.empty()) {
    return (nullptr);
  }

  Node* Current = FirstChild_;
  while (Current) {
    if (Current->Name_ == ChildName) {
      return (Current);
    }
    Current = Current->NextSibling_;
  }

  return (nullptr);
}

uint32_t Node::GetChildCount() const
{
  uint32_t Count = 0;
  Node* Current = FirstChild_;
  while (Current) {
    Count++;
    Current = Current->NextSibling_;
  }

  return (Count);
}

//=============================================================================
// Scene Queries
//=============================================================================

Node* Node::GetNode(std::string_view Path)
{
  if (Path.empty()) {
    return (this);
  }

  // Strip trailing slash
  if (Path.back() == '/') {
    Path = Path.substr(0, Path.size() - 1);
  }

  if (Path.empty()) {
    return (this);
  }

  Node* Current = this;

  while (!Path.empty() && Current) {
    // Find next segment
    size_t SlashPos = Path.find('/');
    std::string_view Segment = (SlashPos != std::string_view::npos)
      ? Path.substr(0, SlashPos)
      : Path;

    if (Segment == "..") {
      Current = Current->Parent_;
    } else {
      Current = Current->FindChild(Segment);
    }

    // Advance past this segment
    if (SlashPos != std::string_view::npos) {
      Path = Path.substr(SlashPos + 1);
    } else {
      break;
    }
  }

  return (Current);
}

//=============================================================================
// Groups
//=============================================================================

void Node::AddToGroup(std::string_view GroupName)
{
  if (OwningTree_ && !GroupName.empty()) {
    OwningTree_->AddNodeToGroup(this, GroupName);
  }
}

void Node::RemoveFromGroup(std::string_view GroupName)
{
  if (OwningTree_ && !GroupName.empty()) {
    OwningTree_->RemoveNodeFromGroup(this, GroupName);
  }
}

bool Node::IsInGroup(std::string_view GroupName) const
{
  if (OwningTree_ && !GroupName.empty()) {
    return (OwningTree_->IsNodeInGroup(this, GroupName));
  }
  return (false);
}

//=============================================================================
// Lifecycle
//=============================================================================

void Node::Initialize()
{
  if (IsInitialized_) {
    return;
  }

  IsInitialized_ = true;

  // Recursively initialize children
  Node* Child = FirstChild_;
  while (Child) {
    Child->Initialize();
    Child = Child->NextSibling_;
  }
}

void Node::Shutdown()
{
  if (!IsInitialized_) {
    return;
  }

  // Recursively shutdown children
  Node* Child = FirstChild_;
  while (Child) {
    Child->Shutdown();
    Child = Child->NextSibling_;
  }

  IsInitialized_ = false;
}

//=============================================================================
// Script Operations
//=============================================================================

void Node::AttachScript(ScriptBase* Script)
{
  if (!Script) {
    return;
  }

  // If a script is already present, fully detach and destroy it first
  if (Script_) {
    delete DetachScript();
  }

  // Set ownership and assign
  Script->Owner_ = this;
  Script_ = Script;

  // Notify script it has been attached
  Script_->OnAttach();

  // Register with SceneTree's pending init queue
  if (OwningTree_) {
    OwningTree_->RegisterPendingInit(this);
  }
}

ScriptBase* Node::DetachScript()
{
  if (!Script_) {
    return (nullptr);
  }

  ScriptBase* Detached = Script_;

  // If the node is active and the script is initialized, fire OnDisable first
  if (IsActive_ && Detached->IsInitialized_) {
    Detached->OnDisable();
  }

  // Always fire OnDetach
  Detached->OnDetach();

  // Clear the owner reference
  Detached->Owner_ = nullptr;

  // Release the slot
  Script_ = nullptr;

  // Unregister from SceneTree's script process list
  if (OwningTree_) {
    OwningTree_->UnregisterScriptNode(this);
  }

  return (Detached);
}

ScriptBase* Node::GetScript() const
{
  return (Script_);
}

//=============================================================================
// Fluent Transform Setters
//=============================================================================

Node& Node::SetPosition(float X, float Y, float Z)
{
  Transform_.SetTranslation(X, Y, Z);
  return (*this);
}

Node& Node::SetPosition(const Vec3& Pos)
{
  Transform_.SetTranslation(Pos);
  return (*this);
}

Node& Node::SetRotation(const Quat& Rot)
{
  Transform_.SetRotation(Rot);
  return (*this);
}

Node& Node::SetRotation(float PitchRad, float YawRad, float RollRad)
{
  Transform_.SetRotation(PitchRad, YawRad, RollRad);
  return (*this);
}

Node& Node::SetScale(float X, float Y, float Z)
{
  Transform_.SetScale(X, Y, Z);
  return (*this);
}

Node& Node::SetScale(const Vec3& S)
{
  Transform_.SetScale(S);
  return (*this);
}

//=============================================================================
// Active State
//=============================================================================

void Node::SetActive(bool Active)
{
  if (IsActive_ == Active) {
    return;
  }

  IsActive_ = Active;

  // Fire script callbacks on active state transitions, if script is initialized
  if (Script_ && Script_->IsInitialized_) {
    if (!IsActive_) {
      Script_->OnDisable();
    } else {
      Script_->OnEnable();
    }
  }
}
