/**
 * AxNode.cpp - Node hierarchy manipulation and component operations
 *
 * Implements the Node base class for the hybrid node tree + component
 * architecture. Follows the AxSceneObject Parent/FirstChild/NextSibling
 * pattern for hierarchy traversal.
 */

#include "AxEngine/AxNode.h"
#include "AxEngine/AxComponent.h"
#include "Foundation/AxAllocator.h"
#include "Foundation/AxHashTable.h"
#include "Foundation/AxMath.h"

// Helper to convert a uint32_t TypeID to a string key for the hash table.
// Uses a small static buffer per call -- safe for single-threaded use within
// the scope of one hash table operation.
static std::string_view TypeIDToKey(uint32_t TypeID)
{
  // 10 digits max for uint32_t + null terminator
  static char Buffer[16];
  std::snprintf(Buffer, sizeof(Buffer), "%u", TypeID);
  return (Buffer);
}

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
  , ComponentRegistry_(nullptr)
  , HashTableAPI_(TableAPI)
  , IsInitialized_(false)
  , IsActive_(true)
{
  // Initialize transform to identity
  Transform_ = TransformIdentity();

  // Create the component registry hash table
  if (HashTableAPI_) {
    ComponentRegistry_ = HashTableAPI_->CreateTable();
  }
}

Node::~Node()
{
  // Shutdown if still initialized
  if (IsInitialized_) {
    Shutdown();
  }

  // Destroy the component registry
  if (ComponentRegistry_ && HashTableAPI_) {
    HashTableAPI_->DestroyTable(ComponentRegistry_);
    free(ComponentRegistry_);
    ComponentRegistry_ = nullptr;
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
// Component Operations
//=============================================================================

bool Node::AddComponent(Component* Comp)
{
  if (!Comp || !ComponentRegistry_ || !HashTableAPI_) {
    return (false);
  }

  uint32_t TypeID = Comp->GetTypeID();
  auto Key = TypeIDToKey(TypeID);

  // Check for duplicate: one component per type per node
  if (HashTableAPI_->Find(ComponentRegistry_, Key.data()) != nullptr) {
    return (false);
  }

  // Set the owner before calling OnAttach
  Comp->Owner_ = this;

  HashTableAPI_->Set(ComponentRegistry_, Key.data(), static_cast<void*>(Comp));

  // Call the OnAttach lifecycle hook
  Comp->OnAttach();

  return (true);
}

bool Node::RemoveComponent(uint32_t TypeID)
{
  if (!ComponentRegistry_ || !HashTableAPI_) {
    return (false);
  }

  auto Key = TypeIDToKey(TypeID);

  // Check if the component exists
  void* Existing = HashTableAPI_->Find(ComponentRegistry_, Key.data());
  if (!Existing) {
    return (false);
  }

  // Call the OnDetach lifecycle hook
  Component* Comp = static_cast<Component*>(Existing);
  Comp->OnDetach();

  // Clear the owner after OnDetach
  Comp->Owner_ = nullptr;

  HashTableAPI_->Remove(ComponentRegistry_, Key.data());
  return (true);
}

Component* Node::GetComponent(uint32_t TypeID) const
{
  if (!ComponentRegistry_ || !HashTableAPI_) {
    return (nullptr);
  }

  auto Key = TypeIDToKey(TypeID);
  void* Result = HashTableAPI_->Find(ComponentRegistry_, Key.data());
  return (static_cast<Component*>(Result));
}

bool Node::HasComponent(uint32_t TypeID) const
{
  if (!ComponentRegistry_ || !HashTableAPI_) {
    return (false);
  }

  auto Key = TypeIDToKey(TypeID);
  return (HashTableAPI_->Find(ComponentRegistry_, Key.data()) != nullptr);
}
