/**
 * AxScene.cpp - Scene Container Implementation
 *
 * Implements the AxScene class that directly owns the node hierarchy,
 * global component lists, system execution loop, EventBus, and scene
 * settings. Ported from AxSceneEx.cpp and light/camera management
 * from AxScene.c.
 */

#include "AxEngine/AxScene.h"
#include "AxEngine/AxEventBus.h"
#include "Foundation/AxMath.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>

//=============================================================================
// Construction / Destruction
//=============================================================================

AxScene::AxScene(AxHashTableAPI* TableAPI, struct AxAllocator* SceneAllocator)
  : Allocator(SceneAllocator)
  , Lights(nullptr)
  , LightCount(0)
  , Cameras(nullptr)
  , CameraTransforms(nullptr)
  , CameraCount(0)
  , Root_(nullptr)
  , NodeCount_(0)
  , NextNodeID_(1)
  , ComponentListMap_(nullptr)
  , HashTableAPI_(TableAPI)
  , ComponentListCount_(0)
  , SystemCount_(0)
  , Bus_(nullptr)
{
  // Zero-initialize arrays
  memset(ComponentLists_, 0, sizeof(ComponentLists_));
  memset(Systems_, 0, sizeof(Systems_));

  // Initialize scene settings to defaults
  AmbientLight = {0.1f, 0.1f, 0.1f};
  Gravity = {0.0f, -9.81f, 0.0f};
  FogDensity = 0.0f;
  FogColor = {0.5f, 0.5f, 0.5f};

  // Create the component list hash map
  if (HashTableAPI_) {
    ComponentListMap_ = HashTableAPI_->CreateTable();
  }

  // Create the EventBus
  Bus_ = new EventBus();

  // Create the root node
  Root_ = new RootNode(HashTableAPI_);
  Root_->SetNodeID(NextNodeID_++);
  NodeCount_++;
}

AxScene::~AxScene()
{
  // Unregister all systems (calls Shutdown on each)
  while (SystemCount_ > 0) {
    System* Sys = Systems_[SystemCount_ - 1];
    if (Sys) {
      Sys->Shutdown();
    }
    SystemCount_--;
  }

  // Destroy all component lists
  for (uint32_t i = 0; i < ComponentListCount_; ++i) {
    free(ComponentLists_[i].Components);
    ComponentLists_[i].Components = nullptr;
  }

  // Destroy the component list map
  if (ComponentListMap_ && HashTableAPI_) {
    HashTableAPI_->DestroyTable(ComponentListMap_);
    free(ComponentListMap_);
    ComponentListMap_ = nullptr;
  }

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
// Private Helpers
//=============================================================================

std::string_view AxScene::TypeIDToKey(uint32_t TypeID)
{
  static char Buffer[16];
  snprintf(Buffer, sizeof(Buffer), "%u", TypeID);
  return (Buffer);
}

AxComponentList* AxScene::GetOrCreateComponentList(uint32_t TypeID)
{
  if (!HashTableAPI_ || !ComponentListMap_) {
    return (nullptr);
  }

  auto Key = TypeIDToKey(TypeID);
  void* Found = HashTableAPI_->Find(ComponentListMap_, Key.data());

  if (Found) {
    return (static_cast<AxComponentList*>(Found));
  }

  // Create new list if room
  if (ComponentListCount_ >= AX_SCENE_MAX_COMPONENT_TYPES) {
    return (nullptr);
  }

  uint32_t NewIndex = ComponentListCount_;
  AxComponentList* List = &ComponentLists_[NewIndex];
  List->TypeID = TypeID;
  List->Count = 0;
  List->Capacity = 32; // Initial capacity
  List->Components = static_cast<Component**>(
    malloc(sizeof(Component*) * List->Capacity));

  if (!List->Components) {
    return (nullptr);
  }

  // Store the AxComponentList pointer in the hash table
  HashTableAPI_->Set(ComponentListMap_, Key.data(),
                     static_cast<void*>(List));

  ComponentListCount_++;
  return (List);
}

AxComponentList* AxScene::FindComponentList(uint32_t TypeID)
{
  if (!HashTableAPI_ || !ComponentListMap_) {
    return (nullptr);
  }

  std::string_view Key = TypeIDToKey(TypeID);
  void* Found = HashTableAPI_->Find(ComponentListMap_, Key.data());

  if (!Found) {
    return (nullptr);
  }

  return (static_cast<AxComponentList*>(Found));
}

void AxScene::FireEvent(AxEventType Type, Node* Sender,
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

Node* AxScene::FindNodeRecursive(Node* Current, std::string_view NodeName)
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

void AxScene::RemoveNodeComponentsFromScene(Node* Target)
{
  if (!Target) {
    return;
  }

  for (uint32_t i = 0; i < ComponentListCount_; ++i) {
    AxComponentList* List = &ComponentLists_[i];
    for (uint32_t j = 0; j < List->Count; /* no increment */) {
      if (List->Components[j]->GetOwner() == Target) {
        // Shift remaining elements
        for (uint32_t k = j; k < List->Count - 1; ++k) {
          List->Components[k] = List->Components[k + 1];
        }
        List->Count--;
      } else {
        ++j;
      }
    }
  }
}

void AxScene::RemoveSubtreeComponentsFromScene(Node* Target)
{
  if (!Target) {
    return;
  }

  // Remove this node's components
  RemoveNodeComponentsFromScene(Target);

  // Recurse to children
  Node* Child = Target->GetFirstChild();
  while (Child) {
    Node* Next = Child->GetNextSibling();
    RemoveSubtreeComponentsFromScene(Child);
    Child = Next;
  }
}

uint32_t AxScene::CountNodesInSubtree(Node* SubtreeRoot)
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

bool AxScene::SystemComesFirst(const System* A, const System* B)
{
  if (static_cast<uint32_t>(A->GetPhase()) != static_cast<uint32_t>(B->GetPhase())) {
    return (static_cast<uint32_t>(A->GetPhase()) < static_cast<uint32_t>(B->GetPhase()));
  }
  return (A->GetPriority() < B->GetPriority());
}

//=============================================================================
// Node Management
//=============================================================================

Node* AxScene::CreateNode(std::string_view NodeName, NodeType Type, Node* Parent)
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

  // Fire AX_EVENT_NODE_CREATED on the scene's EventBus
  FireEvent(AX_EVENT_NODE_CREATED, NewNode, nullptr, 0);

  return (NewNode);
}

void AxScene::DestroyNode(Node* Target)
{
  if (!Target) {
    return;
  }

  // Don't allow destroying the root
  if (Target == static_cast<Node*>(Root_)) {
    return;
  }

  // Count nodes being removed (for updating NodeCount)
  uint32_t RemovedCount = CountNodesInSubtree(Target);
  (void)RemovedCount; // Used conceptually; actual decrement is per-node

  // Remove all components in this subtree from global lists
  RemoveSubtreeComponentsFromScene(Target);

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

  // Update count (subtract only 1 here since children were counted recursively)
  if (NodeCount_ > 0) {
    NodeCount_--;
  }
}

Node* AxScene::FindNode(std::string_view NodeName)
{
  if (NodeName.empty() || !Root_) {
    return (nullptr);
  }

  return (FindNodeRecursive(static_cast<Node*>(Root_), NodeName));
}

//=============================================================================
// Component Management (Scene-Level)
//=============================================================================

bool AxScene::AddComponent(Node* Target, Component* Comp)
{
  if (!Target || !Comp) {
    return (false);
  }

  // Add to the node first (enforces one-per-type)
  if (!Target->AddComponent(Comp)) {
    return (false);
  }

  // Add to the scene's global component list for this type
  AxComponentList* List = GetOrCreateComponentList(Comp->GetTypeID());
  if (!List) {
    // Failed to get list - remove from node to keep consistent
    Target->RemoveComponent(Comp->GetTypeID());
    return (false);
  }

  // Grow the list if needed
  if (List->Count >= List->Capacity) {
    uint32_t NewCapacity = List->Capacity * 2;
    Component** NewArray = static_cast<Component**>(
      realloc(List->Components, sizeof(Component*) * NewCapacity));
    if (!NewArray) {
      Target->RemoveComponent(Comp->GetTypeID());
      return (false);
    }
    List->Components = NewArray;
    List->Capacity = NewCapacity;
  }

  List->Components[List->Count] = Comp;
  List->Count++;

  // Fire AX_EVENT_COMPONENT_ADDED on the scene's EventBus
  FireEvent(AX_EVENT_COMPONENT_ADDED, Target,
            static_cast<void*>(Comp), sizeof(Component*));

  return (true);
}

bool AxScene::RemoveComponent(Node* Target, uint32_t TypeID)
{
  if (!Target) {
    return (false);
  }

  // Get the component before removing it from the node
  Component* Comp = Target->GetComponent(TypeID);
  if (!Comp) {
    return (false);
  }

  // Fire AX_EVENT_COMPONENT_REMOVED before actual removal
  FireEvent(AX_EVENT_COMPONENT_REMOVED, Target,
            static_cast<void*>(Comp), sizeof(Component*));

  // Remove from the scene's global component list
  AxComponentList* List = FindComponentList(TypeID);
  if (List) {
    for (uint32_t i = 0; i < List->Count; ++i) {
      if (List->Components[i] == Comp) {
        // Shift remaining elements
        for (uint32_t j = i; j < List->Count - 1; ++j) {
          List->Components[j] = List->Components[j + 1];
        }
        List->Count--;
        break;
      }
    }
  }

  // Remove from the node (calls OnDetach)
  Target->RemoveComponent(TypeID);

  return (true);
}

Component* AxScene::GetComponent(Node* Target, uint32_t TypeID)
{
  if (!Target) {
    return (nullptr);
  }

  return (Target->GetComponent(TypeID));
}

Component** AxScene::GetComponentsByType(uint32_t TypeID, uint32_t* OutCount)
{
  if (!OutCount) {
    return (nullptr);
  }

  AxComponentList* List = FindComponentList(TypeID);
  if (!List || List->Count == 0) {
    *OutCount = 0;
    return (nullptr);
  }

  *OutCount = List->Count;
  return (List->Components);
}

//=============================================================================
// System Management
//=============================================================================

void AxScene::RegisterSystem(System* Sys)
{
  if (!Sys) {
    return;
  }

  if (SystemCount_ >= AX_SCENE_MAX_SYSTEMS) {
    return;
  }

  // Check for duplicate
  for (uint32_t i = 0; i < SystemCount_; ++i) {
    if (Systems_[i] == Sys) {
      return;
    }
  }

  // Insert sorted by Phase then Priority using insertion sort
  uint32_t InsertIndex = SystemCount_;
  for (uint32_t i = 0; i < SystemCount_; ++i) {
    if (SystemComesFirst(Sys, Systems_[i])) {
      InsertIndex = i;
      break;
    }
  }

  // Shift elements to make room
  for (uint32_t i = SystemCount_; i > InsertIndex; --i) {
    Systems_[i] = Systems_[i - 1];
  }

  Systems_[InsertIndex] = Sys;
  SystemCount_++;

  // Call Init on the system
  Sys->Init();
}

void AxScene::UnregisterSystem(System* Sys)
{
  if (!Sys) {
    return;
  }

  for (uint32_t i = 0; i < SystemCount_; ++i) {
    if (Systems_[i] == Sys) {
      // Call Shutdown on the system
      Sys->Shutdown();

      // Compact the array
      for (uint32_t j = i; j < SystemCount_ - 1; ++j) {
        Systems_[j] = Systems_[j + 1];
      }
      Systems_[SystemCount_ - 1] = nullptr;
      SystemCount_--;
      return;
    }
  }
}

void AxScene::UpdateSystems(float DeltaT)
{
  for (uint32_t i = 0; i < SystemCount_; ++i) {
    System* Sys = Systems_[i];

    // Skip FixedUpdate systems -- those are called by FixedUpdateSystems
    if (Sys->GetPhase() == SystemPhase::FixedUpdate) {
      continue;
    }

    // Get the component list for this system's type
    uint32_t Count = 0;
    Component** Components = GetComponentsByType(
      Sys->GetComponentTypeID(), &Count);

    Sys->Update(DeltaT, Components, Count);
  }
}

void AxScene::FixedUpdateSystems(float FixedDeltaT)
{
  for (uint32_t i = 0; i < SystemCount_; ++i) {
    System* Sys = Systems_[i];

    // Only call FixedUpdate phase systems
    if (Sys->GetPhase() != SystemPhase::FixedUpdate) {
      continue;
    }

    // Get the component list for this system's type
    uint32_t Count = 0;
    Component** Components = GetComponentsByType(
      Sys->GetComponentTypeID(), &Count);

    Sys->Update(FixedDeltaT, Components, Count);
  }
}

//=============================================================================
// Light Management
//=============================================================================

AxLight* AxScene::CreateLight(std::string_view LightName, AxLightType Type)
{
  if (LightName.empty()) {
    return (nullptr);
  }

  // Allocate space for new light
  if (Lights == nullptr) {
    if (Allocator) {
      Lights = static_cast<AxLight*>(AxAlloc(Allocator, sizeof(AxLight)));
    } else {
      Lights = static_cast<AxLight*>(malloc(sizeof(AxLight)));
    }
  } else {
    AxLight* NewLights = nullptr;
    if (Allocator) {
      NewLights = static_cast<AxLight*>(
        AxAlloc(Allocator, sizeof(AxLight) * (LightCount + 1)));
    } else {
      NewLights = static_cast<AxLight*>(
        malloc(sizeof(AxLight) * (LightCount + 1)));
    }
    if (!NewLights) {
      return (nullptr);
    }

    // Copy existing lights
    for (uint32_t i = 0; i < LightCount; i++) {
      NewLights[i] = Lights[i];
    }

    // Free old array only if using malloc (allocator memory is bulk-freed)
    if (!Allocator) {
      free(Lights);
    }

    Lights = NewLights;
  }

  if (!Lights) {
    return (nullptr);
  }

  // Initialize new light
  AxLight* Light = &Lights[LightCount];
  strncpy(Light->Name, LightName.data(), sizeof(Light->Name) - 1);
  Light->Name[sizeof(Light->Name) - 1] = '\0';
  Light->Type = Type;
  Light->Position = {0.0f, 0.0f, 0.0f};
  Light->Direction = {0.0f, -1.0f, 0.0f};
  Light->Color = {1.0f, 1.0f, 1.0f};
  Light->Intensity = 1.0f;
  Light->Range = 0.0f;
  Light->InnerConeAngle = 0.5f;
  Light->OuterConeAngle = 1.0f;

  LightCount++;
  return (Light);
}

AxLight* AxScene::FindLight(std::string_view LightName)
{
  if (LightName.empty() || !Lights) {
    return (nullptr);
  }

  for (uint32_t i = 0; i < LightCount; i++) {
    if (Lights[i].Name == LightName) {
      return (&Lights[i]);
    }
  }

  return (nullptr);
}

//=============================================================================
// Camera Management
//=============================================================================

bool AxScene::AddCamera(const AxCamera* Camera, const AxTransform* Transform)
{
  if (!Camera || !Transform) {
    return (false);
  }

  if (Cameras == nullptr) {
    if (Allocator) {
      Cameras = static_cast<AxCamera*>(AxAlloc(Allocator, sizeof(AxCamera)));
      CameraTransforms = static_cast<AxTransform*>(AxAlloc(Allocator, sizeof(AxTransform)));
    } else {
      Cameras = static_cast<AxCamera*>(malloc(sizeof(AxCamera)));
      CameraTransforms = static_cast<AxTransform*>(malloc(sizeof(AxTransform)));
    }

    if (!Cameras || !CameraTransforms) {
      return (false);
    }

    memset(Cameras, 0, sizeof(AxCamera));
    memset(CameraTransforms, 0, sizeof(AxTransform));
  } else {
    AxCamera* NewCameras = nullptr;
    AxTransform* NewTransforms = nullptr;

    if (Allocator) {
      NewCameras = static_cast<AxCamera*>(
        AxAlloc(Allocator, sizeof(AxCamera) * (CameraCount + 1)));
      NewTransforms = static_cast<AxTransform*>(
        AxAlloc(Allocator, sizeof(AxTransform) * (CameraCount + 1)));
    } else {
      NewCameras = static_cast<AxCamera*>(
        malloc(sizeof(AxCamera) * (CameraCount + 1)));
      NewTransforms = static_cast<AxTransform*>(
        malloc(sizeof(AxTransform) * (CameraCount + 1)));
    }

    if (!NewCameras || !NewTransforms) {
      if (!Allocator) {
        free(NewCameras);
        free(NewTransforms);
      }
      return (false);
    }

    // Copy existing cameras and transforms
    for (uint32_t i = 0; i < CameraCount; i++) {
      NewCameras[i] = Cameras[i];
      NewTransforms[i] = CameraTransforms[i];
    }

    // Free old arrays only if using malloc
    if (!Allocator) {
      free(Cameras);
      free(CameraTransforms);
    }

    Cameras = NewCameras;
    CameraTransforms = NewTransforms;
  }

  // Copy camera and transform to scene
  Cameras[CameraCount] = *Camera;
  CameraTransforms[CameraCount] = *Transform;

  CameraCount++;
  return (true);
}

AxCamera* AxScene::GetCamera(uint32_t Index, AxTransform** OutTransform)
{
  if (!Cameras || !CameraTransforms || Index >= CameraCount) {
    return (nullptr);
  }

  if (OutTransform) {
    *OutTransform = &CameraTransforms[Index];
  }

  return (&Cameras[Index]);
}
