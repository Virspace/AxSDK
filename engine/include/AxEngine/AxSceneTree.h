#pragma once

/**
 * AxSceneTree.h - SceneTree: Godot-inspired Runtime Scene Container
 *
 * SceneTree is the engine-level runtime scene container that owns the node
 * hierarchy, typed-node tracking lists, EventBus, and scene-level settings
 * (ambient light, gravity, fog).
 *
 * Unlike the old AxScene + System pattern, SceneTree directly owns the
 * traversal methods (Update, FixedUpdate, LateUpdate) rather than
 * delegating to a system registry. The frame loop in AxEngine::Tick
 * calls these methods explicitly.
 *
 * Typed nodes (MeshInstance, CameraNode, LightNode) are tracked in flat
 * arrays for efficient system-level queries via GetNodesByType().
 *
 * "Scene" as a concept is the serialized file (.ats) and the node subtree
 * it produces, not the runtime container class.
 *
 * This is engine-level code (C++).
 */

#include "Foundation/AxTypes.h"
#include "AxEngine/AxSceneTypes.h"
#include "Foundation/AxHashTable.h"
#include "Foundation/AxAllocator.h"
#include "AxEngine/AxNode.h"
#include "AxEngine/AxTypedNodes.h"
#include "AxEngine/AxEventBus.h"

#include <string>
#include <string_view>

// Forward declarations
class ScriptBase;

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of lights in a scene tree. */
#define AX_SCENE_TREE_MAX_LIGHTS 64

/** Maximum number of cameras in a scene tree. */
#define AX_SCENE_TREE_MAX_CAMERAS 16

/** Maximum number of nodes per typed-node tracking array. */
#define AX_SCENE_TREE_MAX_TYPED_NODES 1024

//=============================================================================
// SceneTree Class
//=============================================================================

/**
 * SceneTree - Self-contained runtime scene container.
 *
 * Directly owns the root node, typed-node tracking lists, EventBus,
 * scene settings, and traversal methods for script dispatch and
 * transform propagation.
 *
 * Constructed with a AxHashTableAPI (required), an optional AxResourceAPI
 * for scene loading, and an optional AxAllocator for scene-scoped memory.
 */
class SceneTree
{
public:
  //=========================================================================
  // Construction / Destruction
  //=========================================================================

  /**
   * Construct a SceneTree with a hash table API and optional allocator.
   * Creates the RootNode, EventBus, and initializes defaults.
   * @param TableAPI Required hash table API for node operations.
   * @param SceneAllocator Optional allocator for scene-scoped memory.
   */
  SceneTree(AxHashTableAPI* TableAPI, struct AxAllocator* SceneAllocator = nullptr);

  /**
   * Destroy the scene tree and all owned resources.
   * Tears down nodes and EventBus in correct order.
   */
  ~SceneTree();

  // Non-copyable
  SceneTree(const SceneTree&) = delete;
  SceneTree& operator=(const SceneTree&) = delete;

  //=========================================================================
  // Frame Loop Methods
  //=========================================================================

  /**
   * Variable-rate update: flush transforms, run bottom-up init scan,
   * then dispatch OnUpdate top-down.
   * @param DeltaT Time elapsed since last frame (seconds).
   */
  void Update(float DeltaT);

  /**
   * Fixed-rate update: dispatch OnFixedUpdate top-down.
   * @param DeltaT Fixed timestep interval (seconds).
   */
  void FixedUpdate(float DeltaT);

  /**
   * Late update: dispatch OnLateUpdate top-down.
   * @param DeltaT Time elapsed since last frame (seconds).
   */
  void LateUpdate(float DeltaT);

  //=========================================================================
  // Node Management
  //=========================================================================

  /**
   * Create a new node in the scene tree.
   * Allocates the correct typed node subclass based on NodeType.
   * Registers the node in the corresponding typed-node tracking array.
   * Fires AX_EVENT_NODE_CREATED on the EventBus.
   * @param NodeName Node name.
   * @param Type Node type (determines which subclass to allocate).
   * @param Parent Parent node (nullptr for root children).
   * @return Pointer to the created Node, or nullptr on failure.
   */
  Node* CreateNode(std::string_view NodeName, NodeType Type, Node* Parent = nullptr);

  /**
   * Destroy a node and all its children, removing from typed-node lists.
   * Fires AX_EVENT_NODE_DESTROYED on the EventBus.
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
  // Typed Node Queries
  //=========================================================================

  /**
   * Get all nodes of a given type across the entire scene.
   * @param Type Node type to query.
   * @param OutCount Receives the number of nodes returned.
   * @return Array of Node pointers, or nullptr if none.
   */
  Node** GetNodesByType(NodeType Type, uint32_t* OutCount);

  //=========================================================================
  // Accessors
  //=========================================================================

  RootNode* GetRootNode() const { return (Root_); }
  uint32_t GetNodeCount() const { return (NodeCount_); }
  EventBus* GetEventBus() const { return (Bus_); }
  AxHashTableAPI* GetHashTableAPI() const { return (HashTableAPI_); }

  /** Store the main camera pointer for propagation to scripts. */
  void SetMainCamera(AxCamera* Camera);

  /** Store the mouse delta for propagation to scripts this frame. */
  void UpdateMouseDelta(AxVec2 Delta);

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

private:
  //=========================================================================
  // Traversal Helpers (moved from ScriptSystem and TransformSystem)
  //=========================================================================

  void TraverseTopDown(Node* Root, float DeltaT,
                       void (ScriptBase::*Callback)(float));
  void TraverseBottomUpInit(Node* Root);
  void UpdateNodeTransforms(Node* Current, const AxMat4x4& ParentWorldMatrix,
                            bool ParentWasDirty);

  //=========================================================================
  // Private Helpers
  //=========================================================================

  static Node* FindNodeRecursive(Node* Current, std::string_view Name);
  static uint32_t CountNodesInSubtree(Node* Root);
  void FireEvent(AxEventType Type, Node* Sender, void* Data, size_t DataSize);

  /** Register a typed node in the corresponding tracking array. */
  void RegisterTypedNode(Node* NewNode);

  /** Unregister a typed node from the corresponding tracking array. */
  void UnregisterTypedNode(Node* Target);

  /** Unregister all typed nodes in a subtree. */
  void UnregisterSubtreeTypedNodes(Node* Target);

  //=========================================================================
  // Private Members
  //=========================================================================

  // Node hierarchy
  RootNode* Root_;
  uint32_t NodeCount_;
  uint32_t NextNodeID_;

  // Hash table API
  AxHashTableAPI* HashTableAPI_;

  // Typed-node tracking arrays for efficient system-level queries
  Node* MeshInstances_[AX_SCENE_TREE_MAX_TYPED_NODES];
  uint32_t MeshInstanceCount_;
  Node* CameraNodes_[AX_SCENE_TREE_MAX_TYPED_NODES];
  uint32_t CameraNodeCount_;
  Node* LightNodes_[AX_SCENE_TREE_MAX_TYPED_NODES];
  uint32_t LightNodeCount_;

  // Scene-scoped EventBus
  EventBus* Bus_;

  // Engine state propagated to scripts during traversal
  AxCamera* MainCamera_{nullptr};
  AxVec2 MouseDelta_;
};
