/**
 * AxSceneTree.cpp - SceneTree Implementation
 *
 * Implements the SceneTree class that directly owns the node hierarchy,
 * typed-node tracking lists, EventBus, scene settings, and traversal
 * methods for script dispatch and transform propagation.
 *
 * Typed nodes (MeshInstance, CameraNode, LightNode) are tracked in flat
 * arrays populated during CreateNode() for efficient system-level queries.
 */

#include "AxEngine/AxSceneTree.h"
#include "AxEngine/AxScriptBase.h"
#include "AxEngine/AxEventBus.h"
#include "Foundation/AxMath.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>

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
  , Bus_(nullptr)
{
  // Zero-initialize typed-node tracking arrays
  memset(MeshInstances_, 0, sizeof(MeshInstances_));
  memset(CameraNodes_, 0, sizeof(CameraNodes_));
  memset(LightNodes_, 0, sizeof(LightNodes_));

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
  NodeCount_++;
}

SceneTree::~SceneTree()
{
  // Destroy the EventBus
  if (Bus_) {
    delete Bus_;
    Bus_ = nullptr;
  }

  // Destroy the root node (and recursively its children)
  if (Root_) {
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
// Traversal Helpers (moved from ScriptSystem)
//=============================================================================

void SceneTree::TraverseTopDown(Node* Root, float DeltaT,
                                void (ScriptBase::*Callback)(float))
{
  if (!Root) {
    return;
  }

  // If this node is inactive, skip it and its entire subtree.
  if (!Root->IsActive()) {
    return;
  }

  // Node is active: if it has an initialized script, invoke the callback.
  ScriptBase* Script = Root->GetScript();
  if (Script && Script->IsInitialized_) {
    Script->MainCamera = MainCamera_;
    Script->MouseDelta = MouseDelta_;
    (Script->*Callback)(DeltaT);
  }

  // Recurse into children
  Node* Child = Root->GetFirstChild();
  while (Child) {
    TraverseTopDown(Child, DeltaT, Callback);
    Child = Child->GetNextSibling();
  }
}

void SceneTree::TraverseBottomUpInit(Node* Root)
{
  if (!Root) {
    return;
  }

  // Post-order: recurse children first so children are initialized before parent
  Node* Child = Root->GetFirstChild();
  while (Child) {
    TraverseBottomUpInit(Child);
    Child = Child->GetNextSibling();
  }

  // Then initialize this node's script if it has not been initialized yet.
  ScriptBase* Script = Root->GetScript();
  if (Script && !Script->IsInitialized_) {
    Script->MainCamera = MainCamera_;
    Script->IsInitialized_ = true;
    Script->OnInit();

    // OnEnable fires immediately after OnInit only if the node is active
    if (Root->IsActive()) {
      Script->OnEnable();
    }
  }
}

//=============================================================================
// Transform Propagation (moved from TransformSystem)
//=============================================================================

void SceneTree::UpdateNodeTransforms(Node* Current,
                                     const AxMat4x4& ParentWorldMatrix,
                                     bool ParentWasDirty)
{
  if (!Current) {
    return;
  }

  AxTransform& Transform = Current->GetTransform();

  // If parent was dirty, this node is also dirty
  if (ParentWasDirty) {
    Transform.ForwardMatrixDirty = true;
  }

  bool ThisNodeDirty = Transform.ForwardMatrixDirty;

  if (ThisNodeDirty) {
    // Compute local TRS matrix using Foundation math
    AxMat4x4 LocalMatrix = {0};

    // Build the TRS matrix: Scale -> Rotate -> Translate
    AxMat4x4 RotMatrix = QuatToMat4x4(Transform.Rotation);

    // Apply scale to rotation columns
    LocalMatrix.E[0][0] = RotMatrix.E[0][0] * Transform.Scale.X;
    LocalMatrix.E[0][1] = RotMatrix.E[0][1] * Transform.Scale.X;
    LocalMatrix.E[0][2] = RotMatrix.E[0][2] * Transform.Scale.X;
    LocalMatrix.E[0][3] = 0.0f;

    LocalMatrix.E[1][0] = RotMatrix.E[1][0] * Transform.Scale.Y;
    LocalMatrix.E[1][1] = RotMatrix.E[1][1] * Transform.Scale.Y;
    LocalMatrix.E[1][2] = RotMatrix.E[1][2] * Transform.Scale.Y;
    LocalMatrix.E[1][3] = 0.0f;

    LocalMatrix.E[2][0] = RotMatrix.E[2][0] * Transform.Scale.Z;
    LocalMatrix.E[2][1] = RotMatrix.E[2][1] * Transform.Scale.Z;
    LocalMatrix.E[2][2] = RotMatrix.E[2][2] * Transform.Scale.Z;
    LocalMatrix.E[2][3] = 0.0f;

    LocalMatrix.E[3][0] = Transform.Translation.X;
    LocalMatrix.E[3][1] = Transform.Translation.Y;
    LocalMatrix.E[3][2] = Transform.Translation.Z;
    LocalMatrix.E[3][3] = 1.0f;

    // WorldMatrix = Parent.WorldMatrix * Local.TRS
    Transform.CachedForwardMatrix = Mat4x4Mul(ParentWorldMatrix, LocalMatrix);
    Transform.ForwardMatrixDirty = false;
  }

  // Recurse to children, passing this node's world matrix
  Node* Child = Current->GetFirstChild();
  while (Child) {
    UpdateNodeTransforms(Child, Transform.CachedForwardMatrix, ThisNodeDirty);
    Child = Child->GetNextSibling();
  }
}

//=============================================================================
// Frame Loop Methods
//=============================================================================

void SceneTree::Update(float DeltaT)
{
  if (!Root_) {
    return;
  }

  // Step 1: Flush transform hierarchy (eager dirty-flag propagation)
  RootNode* RootPtr = Root_;
  AxTransform& RootTransform = RootPtr->GetTransform();
  bool RootDirty = RootTransform.ForwardMatrixDirty;

  if (RootTransform.ForwardMatrixDirty) {
    RootTransform.CachedForwardMatrix = Identity();
    RootTransform.ForwardMatrixDirty = false;
  }

  Node* Child = RootPtr->GetFirstChild();
  while (Child) {
    UpdateNodeTransforms(Child, RootTransform.CachedForwardMatrix, RootDirty);
    Child = Child->GetNextSibling();
  }

  // Step 2: Bottom-up init scan (initializes any scripts attached since last frame)
  TraverseBottomUpInit(static_cast<Node*>(Root_));

  // Step 3: Top-down OnUpdate dispatch
  TraverseTopDown(static_cast<Node*>(Root_), DeltaT, &ScriptBase::OnUpdate);
}

void SceneTree::FixedUpdate(float DeltaT)
{
  if (!Root_) {
    return;
  }

  TraverseTopDown(static_cast<Node*>(Root_), DeltaT, &ScriptBase::OnFixedUpdate);
}

void SceneTree::LateUpdate(float DeltaT)
{
  if (!Root_) {
    return;
  }

  TraverseTopDown(static_cast<Node*>(Root_), DeltaT, &ScriptBase::OnLateUpdate);
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
// Node Management
//=============================================================================

Node* SceneTree::CreateNode(std::string_view NodeName, NodeType Type, Node* Parent)
{
  if (NodeName.empty()) {
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

  // Attach to parent (default to root if no parent specified)
  if (Parent) {
    Parent->AddChild(NewNode);
  } else {
    Root_->AddChild(NewNode);
  }

  NodeCount_++;

  // Register in typed-node tracking array
  RegisterTypedNode(NewNode);

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

  // Fire AX_EVENT_NODE_DESTROYED before detaching from parent
  FireEvent(AX_EVENT_NODE_DESTROYED, Target, nullptr, 0);

  // Detach from parent
  Node* ParentNode = Target->GetParent();
  if (ParentNode) {
    ParentNode->RemoveChild(Target);
  }

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
