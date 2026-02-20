#pragma once

/**
 * AxSceneParserInternal.h - Internal API for AxScene Parser
 *
 * Provides non-exported initialization functions so that AxResource can
 * compile the parser sources directly and call init without going through
 * the plugin registry. This avoids the segfault caused by the missing
 * libAxScene.dll registry entry.
 *
 * These functions are NOT exported via AXON_DLL_EXPORT -- they are only
 * used internally within the AxResource DLL which compiles these sources.
 */

// Forward declarations
struct AxAPIRegistry;
struct AxSceneParserAPI;
struct AxAllocator;
class AxScene;
class Node;

/**
 * Initialize the scene parser subsystem.
 * Sets up the AllocatorAPI, PlatformAPI, HashTableAPI, ComponentFactory,
 * and registers all core systems.
 *
 * Called by ResourceSystem::Initialize() instead of relying on a separate
 * AxScene DLL being loaded.
 *
 * @param Registry The global API registry.
 */
void AxSceneParser_Init(struct AxAPIRegistry* Registry);

/**
 * Terminate the scene parser subsystem.
 * Unregisters all core systems and cleans up state.
 *
 * Called by ResourceSystem::Shutdown().
 */
void AxSceneParser_Term(void);

/**
 * Get the internal AxSceneParserAPI pointer.
 * Valid after AxSceneParser_Init() has been called.
 *
 * @return Pointer to the AxSceneParserAPI struct, or nullptr if not initialized.
 */
struct AxSceneParserAPI* AxSceneParser_GetAPI(void);

/**
 * Parse a prefab (.axp) string into a node subtree attached to Scene.
 * Prefab format: a single top-level "node" block with no scene wrapper.
 * Exposed for direct use from test code and prefab-loading tools.
 */
Node* ParsePrefab(const char* PrefabData, AxScene* Scene);

/**
 * Deep-copy a prefab node subtree into Scene, creating new independent nodes
 * and re-registering all components in the scene's global component lists.
 * Uses the global ComponentFactory (initialized via AxSceneParser_Init).
 */
Node* InstantiatePrefab(AxScene* Scene, Node* PrefabRoot,
                         Node* Parent, struct AxAllocator* Allocator);
