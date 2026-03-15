/**
 * AxSceneTree.cpp - SceneTree Implementation
 *
 * Implements the SceneTree class that directly owns the node hierarchy,
 * typed-node tracking lists, EventBus, scene settings, and traversal
 * methods for script dispatch and transform propagation.
 *
 * Optimization: Uses three flat lists to avoid O(N) full-tree traversals:
 *   - TransformDirtyRoots_: nodes whose transforms changed since last flush
 *   - ScriptNodes_: nodes with initialized scripts for per-frame dispatch
 *   - PendingInitScripts_: nodes with scripts awaiting OnInit
 *
 * Typed nodes (MeshInstance, CameraNode, LightNode) are tracked in flat
 * arrays populated during CreateNode() for efficient system-level queries.
 */

#include "AxEngine/AxSceneTree.h"
#include "AxEngine/AxScriptBase.h"
#include "AxEngine/AxScriptLog.h"
#include "AxEngine/AxEventBus.h"
#include "Foundation/AxMath.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <algorithm>

//=============================================================================
// File-local Helpers
//=============================================================================

/**
 * Compute the depth of a node in the tree hierarchy.
 * Root is depth 0, its children are depth 1, etc.
 */
static uint32_t NodeDepth(Node* N)
{
  uint32_t Depth = 0;
  Node* Current = N->GetParent();
  while (Current) {
    Depth++;
    Current = Current->GetParent();
  }
  return (Depth);
}

/**
 * Check whether a node is effectively active by walking up the parent chain.
 * Returns false if the node or any ancestor is inactive.
 * This preserves the old TraverseTopDown behavior where an inactive parent
 * caused its entire subtree to be skipped.
 */
static bool IsNodeEffectivelyActive(Node* N)
{
  Node* Current = N;
  while (Current) {
    if (!Current->IsActive()) {
      return (false);
    }
    Current = Current->GetParent();
  }
  return (true);
}

//=============================================================================
// Construction / Destruction
//=============================================================================

SceneTree::SceneTree(AxHashTableAPI* TableAPI, struct AxAllocator* SceneAllocator)
  : Allocator(SceneAllocator)
  , Root_(nullptr)
  , NodeCount_(0)
  , NextNodeID_(1)
  , HashTableAPI_(TableAPI)
  , MeshInstanceCount_(0)
  , CameraNodeCount_(0)
  , LightNodeCount_(0)
  , TransformDirtyRootCount_(0)
  , ScriptNodeCount_(0)
  , PendingInitCount_(0)
  , Bus_(nullptr)
{
  // Zero-initialize typed-node tracking arrays
  memset(MeshInstances_, 0, sizeof(MeshInstances_));
  memset(CameraNodes_, 0, sizeof(CameraNodes_));
  memset(LightNodes_, 0, sizeof(LightNodes_));

  // Zero-initialize optimization arrays
  memset(TransformDirtyRoots_, 0, sizeof(TransformDirtyRoots_));
  memset(ScriptNodes_, 0, sizeof(ScriptNodes_));
  memset(PendingInitScripts_, 0, sizeof(PendingInitScripts_));

  // Initialize scene settings to defaults
  AmbientLight = {0.1f, 0.1f, 0.1f};
  Gravity = {0.0f, -9.81f, 0.0f};
  FogDensity = 0.0f;
  FogColor = {0.5f, 0.5f, 0.5f};

  // Create the EventBus
  Bus_ = new EventBus();

  // Create the root node
  Root_ = new RootNode(HashTableAPI_);
  Root_->SetNodeID(NextNodeID_++);
  Root_->OwningTree_ = this;
  NodeCount_++;
}

SceneTree::~SceneTree()
{
  // Destroy the EventBus
  if (Bus_) {
    delete Bus_;
    Bus_ = nullptr;
  }

  // Clear the root's OwningTree_ before deletion to prevent
  // the destructor from trying to unregister from lists
  if (Root_) {
    SetOwningTreeRecursive(static_cast<Node*>(Root_), nullptr);
    delete Root_;
    Root_ = nullptr;
  }
}

//=============================================================================
// Engine State Setters
//=============================================================================

void SceneTree::SetMainCamera(CameraNode* Camera)
{
  MainCamera_ = Camera;
}

void SceneTree::UpdateMouseDelta(AxVec2 Delta)
{
  MouseDelta_ = Delta;
}

//=============================================================================
// Transform Propagation
//=============================================================================

void SceneTree::UpdateNodeTransforms(Node* Current,
                                     const Mat4& ParentWorldMatrix,
                                     bool ParentWasDirty)
{
  if (!Current) {
    return;
  }

  Transform& T = Current->Transform_;
  bool ThisNodeDirty = T.IsDirty() || ParentWasDirty;

  if (ThisNodeDirty) {
    // Get the local TRS matrix (lazy-evaluated by Transform)
    const Mat4& LocalMatrix = T.GetForwardMatrix();

    // WorldMatrix = Parent.WorldMatrix * Local.TRS
    Current->WorldMatrix_ = ParentWorldMatrix * LocalMatrix;
  }

  // Recurse to children, passing this node's world matrix
  Node* Child = Current->GetFirstChild();
  while (Child) {
    UpdateNodeTransforms(Child, Current->WorldMatrix_, ThisNodeDirty);
    Child = Child->GetNextSibling();
  }
}

//=============================================================================
// Optimization List Registration
//=============================================================================

void SceneTree::MarkTransformDirty(Node* DirtyNode)
{
  if (!DirtyNode) {
    return;
  }

  // O(1) duplicate prevention
  if (DirtyNode->InDirtyList_) {
    return;
  }

  // Add to dirty roots if there is space
  if (TransformDirtyRootCount_ < AX_SCENE_TREE_MAX_TYPED_NODES) {
    TransformDirtyRoots_[TransformDirtyRootCount_++] = DirtyNode;
    DirtyNode->InDirtyList_ = true;
  }
}

void SceneTree::RegisterPendingInit(Node* PendingNode)
{
  if (!PendingNode) {
    return;
  }

  // Avoid duplicates: check if already in the queue
  for (uint32_t i = 0; i < PendingInitCount_; ++i) {
    if (PendingInitScripts_[i] == PendingNode) {
      return;
    }
  }

  if (PendingInitCount_ < AX_SCENE_TREE_MAX_TYPED_NODES) {
    PendingInitScripts_[PendingInitCount_++] = PendingNode;
  }
}

void SceneTree::RegisterScriptNode(Node* ScriptNode)
{
  if (!ScriptNode) {
    return;
  }

  // Avoid duplicates
  for (uint32_t i = 0; i < ScriptNodeCount_; ++i) {
    if (ScriptNodes_[i] == ScriptNode) {
      return;
    }
  }

  if (ScriptNodeCount_ < AX_SCENE_TREE_MAX_TYPED_NODES) {
    ScriptNodes_[ScriptNodeCount_++] = ScriptNode;
  }
}

void SceneTree::UnregisterScriptNode(Node* ScriptNode)
{
  if (!ScriptNode) {
    return;
  }

  // Swap-with-last removal for O(1) performance
  for (uint32_t i = 0; i < ScriptNodeCount_; ++i) {
    if (ScriptNodes_[i] == ScriptNode) {
      ScriptNodes_[i] = ScriptNodes_[ScriptNodeCount_ - 1];
      ScriptNodes_[ScriptNodeCount_ - 1] = nullptr;
      ScriptNodeCount_--;
      return;
    }
  }
}

//=============================================================================
// Pending Init Processing
//=============================================================================

void SceneTree::ProcessPendingInits()
{
  if (PendingInitCount_ == 0) {
    return;
  }

  // Sort pending inits by depth (deepest first) for bottom-up initialization.
  // Children must be initialized before parents.
  std::sort(PendingInitScripts_, PendingInitScripts_ + PendingInitCount_,
    [](Node* A, Node* B) {
      return (NodeDepth(A) > NodeDepth(B));
    });

  // Process each pending init
  for (uint32_t i = 0; i < PendingInitCount_; ++i) {
    Node* PendingNode = PendingInitScripts_[i];
    if (!PendingNode) {
      continue;
    }

    ScriptBase* Script = PendingNode->GetScript();
    if (!Script || Script->IsInitialized_) {
      continue;
    }

    // Set SceneTree reference and main camera on the script
    Script->MainCamera = MainCamera_;
    Script->Tree_ = this;
    Script->IsInitialized_ = true;
    Script->OnInit();

    // OnEnable fires immediately after OnInit only if the node is active
    if (PendingNode->IsActive()) {
      Script->OnEnable();
    }

    // Move to the script process list for per-frame dispatch
    RegisterScriptNode(PendingNode);
  }

  // Clear the pending init queue
  memset(PendingInitScripts_, 0, PendingInitCount_ * sizeof(Node*));
  PendingInitCount_ = 0;
}

//=============================================================================
// Frame Loop Methods
//=============================================================================

void SceneTree::Update(float DeltaT)
{
  if (!Root_) {
    return;
  }

  // Step 1: Flush dirty transform roots (ALWAYS runs, even in Edit mode)
  // Process only nodes whose transforms changed since last frame.
  // On initial load, all nodes are dirty and the dirty list contains all
  // of them, gracefully degrading to equivalent of full traversal.
  // Ensure root's world matrix is identity
  RootNode* RootPtr = Root_;
  static const Mat4 IdentityMatrix = Mat4::Identity();
  RootPtr->WorldMatrix_ = IdentityMatrix;

  if (TransformDirtyRootCount_ > 0) {
    for (uint32_t i = 0; i < TransformDirtyRootCount_; ++i) {
      Node* DirtyRoot = TransformDirtyRoots_[i];
      if (!DirtyRoot) {
        continue;
      }

      // Determine parent world matrix for this dirty root
      const Mat4& ParentMatrix = DirtyRoot->GetParent()
        ? DirtyRoot->GetParent()->GetWorldTransform()
        : IdentityMatrix;

      UpdateNodeTransforms(DirtyRoot, ParentMatrix, false);

      // Clear the InDirtyList_ flag
      DirtyRoot->InDirtyList_ = false;
    }

    // Clear the dirty roots array
    memset(TransformDirtyRoots_, 0, TransformDirtyRootCount_ * sizeof(Node*));
    TransformDirtyRootCount_ = 0;
  }

  // Steps 2-3: Script processing -- skipped when scripts are disabled (Edit mode)
  if (!ScriptsEnabled_) {
    return;
  }

  // Step 2: Process pending script initializations (bottom-up order)
  ProcessPendingInits();

  // Step 3: Dispatch OnUpdate to all scripted nodes
  // Check IsNodeEffectivelyActive to preserve subtree-skip behavior:
  // if a parent is inactive, its children are also skipped.
  for (uint32_t i = 0; i < ScriptNodeCount_; ++i) {
    Node* ScriptNode = ScriptNodes_[i];
    if (!ScriptNode || !IsNodeEffectivelyActive(ScriptNode)) {
      continue;
    }

    ScriptBase* Script = ScriptNode->GetScript();
    if (Script && Script->IsInitialized_ && Script->IsProcessing()) {
      Script->MainCamera = MainCamera_;
      Script->MouseDelta = MouseDelta_;
      Script->Tree_ = this;
      Script->OnUpdate(DeltaT);
    }
  }
}

void SceneTree::FixedUpdate(float DeltaT)
{
  if (!Root_ || !ScriptsEnabled_) {
    return;
  }

  // Dispatch OnFixedUpdate to all scripted nodes
  for (uint32_t i = 0; i < ScriptNodeCount_; ++i) {
    Node* ScriptNode = ScriptNodes_[i];
    if (!ScriptNode || !IsNodeEffectivelyActive(ScriptNode)) {
      continue;
    }

    ScriptBase* Script = ScriptNode->GetScript();
    if (Script && Script->IsInitialized_ && Script->IsPhysicsProcessing()) {
      Script->MainCamera = MainCamera_;
      Script->MouseDelta = MouseDelta_;
      Script->Tree_ = this;
      Script->OnFixedUpdate(DeltaT);
    }
  }
}

void SceneTree::LateUpdate(float DeltaT)
{
  if (!Root_ || !ScriptsEnabled_) {
    return;
  }

  // Dispatch OnLateUpdate to all scripted nodes
  for (uint32_t i = 0; i < ScriptNodeCount_; ++i) {
    Node* ScriptNode = ScriptNodes_[i];
    if (!ScriptNode || !IsNodeEffectivelyActive(ScriptNode)) {
      continue;
    }

    ScriptBase* Script = ScriptNode->GetScript();
    if (Script && Script->IsInitialized_) {
      Script->MainCamera = MainCamera_;
      Script->MouseDelta = MouseDelta_;
      Script->Tree_ = this;
      Script->OnLateUpdate(DeltaT);
    }
  }
}

//=============================================================================
// Private Helpers
//=============================================================================

void SceneTree::FireEvent(AxEventType Type, Node* Sender,
                          void* Data, size_t DataSize)
{
  if (!Bus_) {
    return;
  }

  AxEvent Event;
  Event.Type = Type;
  Event.Sender = Sender;
  Event.Data = Data;
  Event.DataSize = DataSize;

  Bus_->Publish(Event);
}

Node* SceneTree::FindNodeRecursive(Node* Current, std::string_view NodeName)
{
  if (!Current || NodeName.empty()) {
    return (nullptr);
  }

  if (Current->GetName() == NodeName) {
    return (Current);
  }

  // Search children
  Node* Child = Current->GetFirstChild();
  while (Child) {
    Node* Found = FindNodeRecursive(Child, NodeName);
    if (Found) {
      return (Found);
    }
    Child = Child->GetNextSibling();
  }

  return (nullptr);
}

uint32_t SceneTree::CountNodesInSubtree(Node* SubtreeRoot)
{
  if (!SubtreeRoot) {
    return (0);
  }

  uint32_t Count = 1;
  Node* Child = SubtreeRoot->GetFirstChild();
  while (Child) {
    Count += CountNodesInSubtree(Child);
    Child = Child->GetNextSibling();
  }

  return (Count);
}

void SceneTree::SetOwningTreeRecursive(Node* Target, SceneTree* Tree)
{
  if (!Target) {
    return;
  }

  Target->OwningTree_ = Tree;

  Node* Child = Target->GetFirstChild();
  while (Child) {
    SetOwningTreeRecursive(Child, Tree);
    Child = Child->GetNextSibling();
  }
}

//=============================================================================
// Typed Node Tracking
//=============================================================================

void SceneTree::RegisterTypedNode(Node* NewNode)
{
  if (!NewNode) {
    return;
  }

  switch (NewNode->GetType()) {
    case NodeType::MeshInstance:
      if (MeshInstanceCount_ < AX_SCENE_TREE_MAX_TYPED_NODES) {
        MeshInstances_[MeshInstanceCount_++] = NewNode;
      }
      break;
    case NodeType::Camera:
      if (CameraNodeCount_ < AX_SCENE_TREE_MAX_TYPED_NODES) {
        CameraNodes_[CameraNodeCount_++] = NewNode;
      }
      break;
    case NodeType::Light:
      if (LightNodeCount_ < AX_SCENE_TREE_MAX_TYPED_NODES) {
        LightNodes_[LightNodeCount_++] = NewNode;
      }
      break;
    default:
      break;
  }
}

void SceneTree::UnregisterTypedNode(Node* Target)
{
  if (!Target) {
    return;
  }

  auto RemoveFromArray = [](Node** Array, uint32_t& Count, Node* Target) {
    for (uint32_t i = 0; i < Count; ++i) {
      if (Array[i] == Target) {
        // Shift remaining elements
        for (uint32_t j = i; j < Count - 1; ++j) {
          Array[j] = Array[j + 1];
        }
        Count--;
        return;
      }
    }
  };

  switch (Target->GetType()) {
    case NodeType::MeshInstance:
      RemoveFromArray(MeshInstances_, MeshInstanceCount_, Target);
      break;
    case NodeType::Camera:
      RemoveFromArray(CameraNodes_, CameraNodeCount_, Target);
      break;
    case NodeType::Light:
      RemoveFromArray(LightNodes_, LightNodeCount_, Target);
      break;
    default:
      break;
  }
}

void SceneTree::UnregisterSubtreeTypedNodes(Node* Target)
{
  if (!Target) {
    return;
  }

  // Unregister this node
  UnregisterTypedNode(Target);

  // Recurse to children
  Node* Child = Target->GetFirstChild();
  while (Child) {
    Node* Next = Child->GetNextSibling();
    UnregisterSubtreeTypedNodes(Child);
    Child = Next;
  }
}

//=============================================================================
// Optimization List Cleanup
//=============================================================================

void SceneTree::UnregisterFromAllLists(Node* Target)
{
  if (!Target) {
    return;
  }

  // Remove from TransformDirtyRoots_ (swap-with-last)
  if (Target->InDirtyList_) {
    for (uint32_t i = 0; i < TransformDirtyRootCount_; ++i) {
      if (TransformDirtyRoots_[i] == Target) {
        TransformDirtyRoots_[i] = TransformDirtyRoots_[TransformDirtyRootCount_ - 1];
        TransformDirtyRoots_[TransformDirtyRootCount_ - 1] = nullptr;
        TransformDirtyRootCount_--;
        break;
      }
    }
    Target->InDirtyList_ = false;
  }

  // Remove from ScriptNodes_ (swap-with-last)
  UnregisterScriptNode(Target);

  // Remove from PendingInitScripts_ (swap-with-last)
  for (uint32_t i = 0; i < PendingInitCount_; ++i) {
    if (PendingInitScripts_[i] == Target) {
      PendingInitScripts_[i] = PendingInitScripts_[PendingInitCount_ - 1];
      PendingInitScripts_[PendingInitCount_ - 1] = nullptr;
      PendingInitCount_--;
      break;
    }
  }
}

void SceneTree::UnregisterSubtreeFromAllLists(Node* Target)
{
  if (!Target) {
    return;
  }

  // Unregister this node from all optimization lists
  UnregisterFromAllLists(Target);

  // Recurse to children
  Node* Child = Target->GetFirstChild();
  while (Child) {
    Node* Next = Child->GetNextSibling();
    UnregisterSubtreeFromAllLists(Child);
    Child = Next;
  }
}

//=============================================================================
// Node Management
//=============================================================================

Node* SceneTree::CreateNode(std::string_view NodeName, NodeType Type, Node* Parent)
{
  if (NodeName.empty()) {
    Log::Warn("CreateNode called with empty name");
    return (nullptr);
  }

  Node* NewNode = nullptr;

  switch (Type) {
    case NodeType::Node2D:
      NewNode = new Node2D(NodeName, HashTableAPI_);
      break;
    case NodeType::Node3D:
      NewNode = new Node3D(NodeName, HashTableAPI_);
      break;
    case NodeType::Root:
      // Only one root per scene; return existing root
      return (static_cast<Node*>(Root_));
    case NodeType::MeshInstance:
      NewNode = new MeshInstance(NodeName, HashTableAPI_);
      break;
    case NodeType::Camera:
      NewNode = new CameraNode(NodeName, HashTableAPI_);
      break;
    case NodeType::Light:
      NewNode = new LightNode(NodeName, HashTableAPI_);
      break;
    case NodeType::RigidBody:
      NewNode = new RigidBodyNode(NodeName, HashTableAPI_);
      break;
    case NodeType::Collider:
      NewNode = new ColliderNode(NodeName, HashTableAPI_);
      break;
    case NodeType::AudioSource:
      NewNode = new AudioSourceNode(NodeName, HashTableAPI_);
      break;
    case NodeType::AudioListener:
      NewNode = new AudioListenerNode(NodeName, HashTableAPI_);
      break;
    case NodeType::Animator:
      NewNode = new AnimatorNode(NodeName, HashTableAPI_);
      break;
    case NodeType::ParticleEmitter:
      NewNode = new ParticleEmitterNode(NodeName, HashTableAPI_);
      break;
    case NodeType::Sprite:
      NewNode = new SpriteNode(NodeName, HashTableAPI_);
      break;
    default:
      NewNode = new Node(NodeName, Type, HashTableAPI_);
      break;
  }

  if (!NewNode) {
    return (nullptr);
  }

  // Assign node ID
  NewNode->SetNodeID(NextNodeID_++);

  // Set the OwningTree_ back-pointer
  NewNode->OwningTree_ = this;

  // Attach to parent (default to root if no parent specified)
  if (Parent) {
    Parent->AddChild(NewNode);
  } else {
    Root_->AddChild(NewNode);
  }

  NodeCount_++;

  // Register in typed-node tracking array
  RegisterTypedNode(NewNode);

  // Mark the new node's transform as dirty so it gets processed
  // on the next Update() call
  MarkTransformDirty(NewNode);

  // Fire AX_EVENT_NODE_CREATED on the scene's EventBus
  FireEvent(AX_EVENT_NODE_CREATED, NewNode, nullptr, 0);

  return (NewNode);
}

void SceneTree::DestroyNode(Node* Target)
{
  if (!Target) {
    return;
  }

  // Don't allow destroying the root
  if (Target == static_cast<Node*>(Root_)) {
    return;
  }

  // Unregister all typed nodes in this subtree from tracking arrays
  UnregisterSubtreeTypedNodes(Target);

  // Unregister from all optimization lists (dirty roots, script nodes, pending inits)
  UnregisterSubtreeFromAllLists(Target);

  // Remove from all groups
  RemoveSubtreeFromAllGroups(Target);

  // Fire AX_EVENT_NODE_DESTROYED before detaching from parent
  FireEvent(AX_EVENT_NODE_DESTROYED, Target, nullptr, 0);

  // Detach from parent
  Node* ParentNode = Target->GetParent();
  if (ParentNode) {
    ParentNode->RemoveChild(Target);
  }

  // Clear OwningTree_ on the entire subtree to prevent Node destructors
  // from trying to unregister from lists (we already did it above)
  SetOwningTreeRecursive(Target, nullptr);

  // Recursively destroy children first
  while (Target->GetFirstChild()) {
    DestroyNode(Target->GetFirstChild());
  }

  // Now safe to delete the leaf node
  delete Target;

  // Update count
  if (NodeCount_ > 0) {
    NodeCount_--;
  }
}

Node* SceneTree::FindNode(std::string_view NodeName)
{
  if (NodeName.empty() || !Root_) {
    return (nullptr);
  }

  return (FindNodeRecursive(static_cast<Node*>(Root_), NodeName));
}

//=============================================================================
// Typed Node Queries
//=============================================================================

Node** SceneTree::GetNodesByType(NodeType Type, uint32_t* OutCount)
{
  if (!OutCount) {
    return (nullptr);
  }

  switch (Type) {
    case NodeType::MeshInstance:
      *OutCount = MeshInstanceCount_;
      return (MeshInstanceCount_ > 0 ? MeshInstances_ : nullptr);
    case NodeType::Camera:
      *OutCount = CameraNodeCount_;
      return (CameraNodeCount_ > 0 ? CameraNodes_ : nullptr);
    case NodeType::Light:
      *OutCount = LightNodeCount_;
      return (LightNodeCount_ > 0 ? LightNodes_ : nullptr);
    default:
      *OutCount = 0;
      return (nullptr);
  }
}

//=============================================================================
// Groups
//=============================================================================

const std::vector<Node*> SceneTree::EmptyNodeVector_;

const std::vector<Node*>& SceneTree::GetNodesInGroup(std::string_view GroupName) const
{
  auto It = Groups_.find(std::string(GroupName));
  if (It != Groups_.end()) {
    return (It->second);
  }
  return (EmptyNodeVector_);
}

uint32_t SceneTree::GetGroupSize(std::string_view GroupName) const
{
  auto It = Groups_.find(std::string(GroupName));
  if (It != Groups_.end()) {
    return (static_cast<uint32_t>(It->second.size()));
  }
  return (0);
}

void SceneTree::AddNodeToGroup(Node* Target, std::string_view GroupName)
{
  if (!Target || GroupName.empty()) {
    return;
  }

  auto& Vec = Groups_[std::string(GroupName)];

  // Check for duplicate
  for (Node* N : Vec) {
    if (N == Target) {
      return;
    }
  }

  Vec.push_back(Target);
}

void SceneTree::RemoveNodeFromGroup(Node* Target, std::string_view GroupName)
{
  if (!Target || GroupName.empty()) {
    return;
  }

  auto It = Groups_.find(std::string(GroupName));
  if (It == Groups_.end()) {
    return;
  }

  auto& Vec = It->second;
  for (auto VIt = Vec.begin(); VIt != Vec.end(); ++VIt) {
    if (*VIt == Target) {
      Vec.erase(VIt);
      return;
    }
  }
}

bool SceneTree::IsNodeInGroup(const Node* Target, std::string_view GroupName) const
{
  if (!Target || GroupName.empty()) {
    return (false);
  }

  auto It = Groups_.find(std::string(GroupName));
  if (It == Groups_.end()) {
    return (false);
  }

  for (const Node* N : It->second) {
    if (N == Target) {
      return (true);
    }
  }
  return (false);
}

void SceneTree::RemoveNodeFromAllGroups(Node* Target)
{
  if (!Target) {
    return;
  }

  for (auto& [Name, Vec] : Groups_) {
    for (auto It = Vec.begin(); It != Vec.end(); ++It) {
      if (*It == Target) {
        Vec.erase(It);
        break;
      }
    }
  }
}

void SceneTree::RemoveSubtreeFromAllGroups(Node* Target)
{
  if (!Target) {
    return;
  }

  RemoveNodeFromAllGroups(Target);

  Node* Child = Target->GetFirstChild();
  while (Child) {
    Node* Next = Child->GetNextSibling();
    RemoveSubtreeFromAllGroups(Child);
    Child = Next;
  }
}
