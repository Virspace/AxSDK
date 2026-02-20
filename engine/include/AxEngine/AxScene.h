#pragma once

/**
 * AxScene.h - Scene Container for Node/Component/System Architecture
 *
 * AxScene is the engine-level scene container that directly owns the node
 * hierarchy, global component lists by type, system execution loop, EventBus,
 * and scene-level settings (ambient light, gravity, fog). It also manages
 * light and camera arrays for renderer consumption.
 *
 * This replaces the old AxSceneExtension side-table pattern. AxScene IS the
 * scene -- no mapping tables, no C/C++ split, no indirection.
 *
 * This is engine-level code (C++).
 */

#include "Foundation/AxTypes.h"
#include "AxEngine/AxSceneTypes.h"
#include "Foundation/AxHashTable.h"
#include "Foundation/AxAllocator.h"
#include "AxEngine/AxNode.h"
#include "AxEngine/AxComponent.h"
#include "AxEngine/AxSystem.h"
#include "AxEngine/AxEventBus.h"

#include <string>
#include <string_view>

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of different component types tracked per scene. */
#define AX_SCENE_MAX_COMPONENT_TYPES 128

/** Maximum number of components per type in the global list. */
#define AX_SCENE_MAX_COMPONENTS_PER_TYPE 1024

/** Maximum number of systems registered with a scene. */
#define AX_SCENE_MAX_SYSTEMS 64

/** Maximum number of lights in a scene. */
#define AX_SCENE_MAX_LIGHTS 64

/** Maximum number of cameras in a scene. */
#define AX_SCENE_MAX_CAMERAS 16

//=============================================================================
// AxComponentList - Flat array of Component pointers for a single type
//=============================================================================

/**
 * AxComponentList - Flat array of Component pointers for a single type.
 * Used for efficient system iteration over all components of a given type.
 */
struct AxComponentList
{
  uint32_t TypeID;
  Component** Components;
  uint32_t Count;
  uint32_t Capacity;
};

//=============================================================================
// AxScene Class
//=============================================================================

/**
 * AxScene - Self-contained scene container for the engine.
 *
 * Directly owns the root node, component lists, systems, EventBus,
 * scene settings, and light/camera arrays. Constructed with a
 * AxHashTableAPI (required) and an optional AxAllocator for scene-scoped
 * memory (lights, cameras, etc.).
 */
class AxScene
{
public:
  //=========================================================================
  // Construction / Destruction
  //=========================================================================

  /**
   * Construct a scene with a hash table API and optional allocator.
   * Creates the RootNode, EventBus, and initializes defaults.
   * @param TableAPI Required hash table API for component list mapping.
   * @param SceneAllocator Optional allocator for scene-scoped memory.
   */
  AxScene(AxHashTableAPI* TableAPI, struct AxAllocator* SceneAllocator = nullptr);

  /**
   * Destroy the scene and all owned resources.
   * Tears down systems, component lists, nodes, and EventBus in correct order.
   */
  ~AxScene();

  // Non-copyable
  AxScene(const AxScene&) = delete;
  AxScene& operator=(const AxScene&) = delete;

  //=========================================================================
  // Node Management
  //=========================================================================

  /**
   * Create a new node in the scene.
   * Fires AX_EVENT_NODE_CREATED on the scene's EventBus.
   * @param Name Node name.
   * @param Type Node type.
   * @param Parent Parent node (nullptr for root children).
   * @return Pointer to the created Node, or nullptr on failure.
   */
  Node* CreateNode(std::string_view NodeName, NodeType Type, Node* Parent = nullptr);

  /**
   * Destroy a node and all its children, removing components from global lists.
   * Fires AX_EVENT_NODE_DESTROYED on the scene's EventBus.
   * @param Target Node to destroy.
   */
  void DestroyNode(Node* Target);

  /**
   * Find a node by name (recursive search from root).
   * @param Name Name to search for.
   * @return Pointer to the found Node, or nullptr.
   */
  Node* FindNode(std::string_view Name);

  //=========================================================================
  // Component Management (Scene-Level)
  //=========================================================================

  /**
   * Add a component to a node and register in the scene's global component list.
   * Fires AX_EVENT_COMPONENT_ADDED on the scene's EventBus.
   * @param Target Node to add component to.
   * @param Comp Component to add.
   * @return true if added successfully.
   */
  bool AddComponent(Node* Target, Component* Comp);

  /**
   * Remove a component from a node and unregister from the scene's global list.
   * Fires AX_EVENT_COMPONENT_REMOVED on the scene's EventBus.
   * @param Target Node to remove component from.
   * @param TypeID Component type identifier.
   * @return true if removed successfully.
   */
  bool RemoveComponent(Node* Target, uint32_t TypeID);

  /**
   * Get a component from a node by TypeID.
   * @param Target Node to query.
   * @param TypeID Component type identifier.
   * @return Pointer to the component, or nullptr.
   */
  Component* GetComponent(Node* Target, uint32_t TypeID);

  /**
   * Get all components of a given type across the entire scene.
   * @param TypeID Component type identifier.
   * @param OutCount Receives the number of components returned.
   * @return Array of Component pointers, or nullptr if none.
   */
  Component** GetComponentsByType(uint32_t TypeID, uint32_t* OutCount);

  //=========================================================================
  // System Management
  //=========================================================================

  /**
   * Register a system with the scene. Inserts sorted by Phase then Priority.
   * @param Sys System to register.
   */
  void RegisterSystem(System* Sys);

  /**
   * Unregister a system from the scene.
   * @param Sys System to unregister.
   */
  void UnregisterSystem(System* Sys);

  /**
   * Update all systems (except FixedUpdate phase) with the given delta time.
   * @param DeltaT Time elapsed since last frame (seconds).
   */
  void UpdateSystems(float DeltaT);

  /**
   * Update only FixedUpdate phase systems with the fixed delta time.
   * @param FixedDeltaT Fixed timestep interval (seconds).
   */
  void FixedUpdateSystems(float FixedDeltaT);

  //=========================================================================
  // Light Management
  //=========================================================================

  /**
   * Create a new light in the scene.
   * @param Name Light name.
   * @param Type Light type.
   * @return Pointer to the new light, or nullptr on failure.
   */
  AxLight* CreateLight(std::string_view Name, AxLightType Type);

  /**
   * Find a light by name.
   * @param Name Light name to search for.
   * @return Pointer to the light, or nullptr if not found.
   */
  AxLight* FindLight(std::string_view Name);

  //=========================================================================
  // Camera Management
  //=========================================================================

  /**
   * Add a camera to the scene.
   * @param Camera Camera data to copy.
   * @param Transform Camera transform to copy.
   * @return true on success, false on failure.
   */
  bool AddCamera(const AxCamera* Camera, const AxTransform* Transform);

  /**
   * Get a camera by index.
   * @param Index Camera index.
   * @param OutTransform Receives pointer to the camera's transform.
   * @return Pointer to the camera, or nullptr if index out of range.
   */
  AxCamera* GetCamera(uint32_t Index, AxTransform** OutTransform);

  //=========================================================================
  // Accessors
  //=========================================================================

  RootNode* GetRootNode() const { return (Root_); }
  uint32_t GetNodeCount() const { return (NodeCount_); }
  uint32_t GetSystemCount() const { return (SystemCount_); }
  EventBus* GetEventBus() const { return (Bus_); }
  AxHashTableAPI* GetHashTableAPI() const { return (HashTableAPI_); }

  //=========================================================================
  // Public Members
  //=========================================================================

  // Scene metadata
  std::string Name;
  struct AxAllocator* Allocator;

  // Scene settings (public for direct access)
  AxVec3 AmbientLight;
  AxVec3 Gravity;
  float FogDensity;
  AxVec3 FogColor;

  // Light/Camera arrays (public for renderer access)
  AxLight* Lights;
  uint32_t LightCount;
  AxCamera* Cameras;
  AxTransform* CameraTransforms;
  uint32_t CameraCount;

private:
  //=========================================================================
  // Private Helpers
  //=========================================================================

  static Node* FindNodeRecursive(Node* Current, std::string_view Name);
  void RemoveNodeComponentsFromScene(Node* Target);
  void RemoveSubtreeComponentsFromScene(Node* Target);
  static uint32_t CountNodesInSubtree(Node* Root);
  static bool SystemComesFirst(const System* A, const System* B);
  void FireEvent(AxEventType Type, Node* Sender, void* Data, size_t DataSize);
  static std::string_view TypeIDToKey(uint32_t TypeID);
  AxComponentList* GetOrCreateComponentList(uint32_t TypeID);
  AxComponentList* FindComponentList(uint32_t TypeID);

  //=========================================================================
  // Private Members
  //=========================================================================

  // Node hierarchy
  RootNode* Root_;
  uint32_t NodeCount_;
  uint32_t NextNodeID_;

  // Global component lists by TypeID
  AxHashTable* ComponentListMap_;
  AxHashTableAPI* HashTableAPI_;
  AxComponentList ComponentLists_[AX_SCENE_MAX_COMPONENT_TYPES];
  uint32_t ComponentListCount_;

  // Systems sorted by Phase then Priority
  System* Systems_[AX_SCENE_MAX_SYSTEMS];
  uint32_t SystemCount_;

  // Scene-scoped EventBus
  EventBus* Bus_;
};
