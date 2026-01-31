#pragma once

#include "Foundation/AxTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AXON_SCENE_API_NAME "AxonSceneAPI"

/**
 * Scene API result codes for error handling
 */
typedef enum AxSceneResult {
    AX_SCENE_SUCCESS = 0,               // Operation completed successfully
    AX_SCENE_ERROR_INVALID_ARGUMENTS,  // NULL pointer or invalid parameter
    AX_SCENE_ERROR_OUT_OF_MEMORY,      // Memory allocation failed or limit reached
    AX_SCENE_ERROR_NOT_FOUND,          // Event handler not found in registry
    AX_SCENE_ERROR_ALREADY_EXISTS,     // Event handler already registered
    AX_SCENE_ERROR_INTERNAL            // Internal error occurred
} AxSceneResult;

/**
 * Scene Events structure provides callback interface for scene parsing adapters.
 * Adapters can register callbacks to be notified when scene objects are parsed.
 * NULL function pointers are skipped (no-op behavior).
 */
typedef struct AxSceneEvents {
    // Object callbacks using existing types
    void (*OnLightParsed)(const AxLight* Light, void* UserData);
    void (*OnMaterialParsed)(const AxMaterial* Material, void* UserData);
    void (*OnObjectParsed)(const AxSceneObject* Object, void* UserData);
    void (*OnTransformParsed)(const AxTransform* Transform, void* UserData);

    // Scene structure callbacks
    void (*OnSceneParsed)(const AxScene* Scene, void* UserData);

    // User data for adapter-specific state
    void* UserData;
} AxSceneEvents;

/**
 * Unified Scene API provides comprehensive scene management functionality:
 * - Scene creation and destruction
 * - Object hierarchy manipulation
 * - File loading and parsing (.ats format)
 * - Memory management
 */
struct AxSceneAPI {
    // === Scene Management ===

    /**
     * Create a new scene with the specified name and memory capacity.
     * @param Name Scene identifier (will be copied)
     * @param MemorySize Maximum memory for scene objects and data
     * @return Scene handle, or NULL on failure
     */
    AxScene* (*CreateScene)(const char* Name, size_t MemorySize);

    /**
     * Destroy a scene and free all associated memory.
     * @param Scene Scene to destroy
     */
    void (*DestroyScene)(AxScene* Scene);

    // === Object Management ===

    /**
     * Create a new scene object within the specified scene.
     * @param Scene Target scene
     * @param Name Object name (will be copied)
     * @param Parent Parent object (NULL for root object)
     * @return Scene object handle, or NULL on failure
     */
    AxSceneObject* (*CreateObject)(AxScene* Scene, const char* Name, AxSceneObject* Parent);

    /**
     * Set the transform for a scene object.
     * @param Object Target object
     * @param Transform New transform (local to parent)
     */
    void (*SetObjectTransform)(AxSceneObject* Object, const AxTransform* Transform);

    /**
     * Set the mesh asset path for a scene object.
     * @param Object Target object
     * @param MeshPath Path to mesh asset (will be copied)
     */
    void (*SetObjectMesh)(AxSceneObject* Object, const char* MeshPath);

    /**
     * Set the material asset path for a scene object.
     * @param Object Target object
     * @param MaterialPath Path to material asset (will be copied)
     */
    void (*SetObjectMaterial)(AxSceneObject* Object, const char* MaterialPath);

    // === Material Management ===

    /**
     * Create a new material definition within the scene.
     * @param Scene Target scene
     * @param Name Material name (will be copied)
     * @param VertexShaderPath Path to vertex shader (will be copied)
     * @param FragmentShaderPath Path to fragment shader (will be copied)
     * @return Material pointer, or NULL on failure
     */
    AxMaterial* (*CreateMaterial)(AxScene* Scene, const char* Name, const char* VertexShaderPath, const char* FragmentShaderPath);

    /**
     * Find a material by name within a scene.
     * @param Scene Scene to search
     * @param Name Material name to find
     * @return Material pointer, or NULL if not found
     */
    AxMaterial* (*FindMaterial)(AxScene* Scene, const char* Name);

    /**
     * Find a scene object by name.
     * @param Scene Scene to search
     * @param Name Object name to find
     * @return Object handle, or NULL if not found
     */
    AxSceneObject* (*FindObject)(const AxScene* Scene, const char* Name);

    // === Light Management ===

    /**
     * Create a new light within the scene.
     * @param Scene Target scene
     * @param Name Light name (will be copied)
     * @param Type Light type (directional, point, or spot)
     * @return Light pointer, or NULL on failure
     */
    AxLight* (*CreateLight)(AxScene* Scene, const char* Name, AxLightType Type);

    /**
     * Find a light by name within a scene.
     * @param Scene Scene to search
     * @param Name Light name to find
     * @return Light pointer, or NULL if not found
     */
    AxLight* (*FindLight)(AxScene* Scene, const char* Name);

    // === Camera Management ===

    /**
     * Add a camera to the scene with its transform.
     * Memory is allocated from the scene's allocator.
     * @param Scene Target scene
     * @param Camera Camera to add (will be copied)
     * @param Transform Camera transform (will be copied)
     * @return AX_SCENE_SUCCESS on success, error code on failure
     */
    AxSceneResult (*AddCamera)(AxScene* Scene, const AxCamera* Camera, const AxTransform* Transform);

    /**
     * Get a camera from the scene by index.
     * @param Scene Target scene
     * @param Index Camera index (0-based)
     * @param OutTransform Optional pointer to receive camera transform (can be NULL)
     * @return Camera pointer, or NULL if index out of bounds
     */
    AxCamera* (*GetCamera)(const AxScene* Scene, uint32_t Index, AxTransform** OutTransform);

    /**
     * Get the world transform for a scene object (accumulated through hierarchy).
     * @param Object Target object
     * @param OutTransform Output transform (will be modified)
     */
    void (*GetWorldTransform)(AxSceneObject* Object, AxTransform* OutTransform);

    // === File Loading and Parsing ===

    /**
     * Load a scene from a .ats file with automatic memory management.
     * Uses the default scene memory size for allocation.
     * @param FilePath Path to the .ats scene file
     * @return Loaded scene, or NULL on failure
     */
    AxScene* (*LoadSceneFromFile)(const char* FilePath);

    /**
     * Load a scene from a string containing .ats format data.
     * Uses the default scene memory size for allocation.
     * @param SceneData Scene file content as string
     * @return Loaded scene, or NULL on failure
     */
    AxScene* (*LoadSceneFromString)(const char* SceneData);

    /**
     * Parse a scene from a string with custom memory allocator.
     * Provides lower-level control over memory allocation.
     * @param Source Scene file content as string
     * @param Allocator Memory allocator for scene data (unified interface)
     * @return Parsed scene, or NULL on failure
     */
    AxScene* (*ParseFromString)(const char* Source, struct AxAllocator* Allocator);

    /**
     * Parse a scene from a file with custom memory allocator.
     * Provides lower-level control over memory allocation.
     * @param FilePath Path to .ats scene file
     * @param Allocator Memory allocator for scene data (unified interface)
     * @return Parsed scene, or NULL on failure
     */
    AxScene* (*ParseFromFile)(const char* FilePath, struct AxAllocator* Allocator);

    // === Configuration and Error Handling ===

    /**
     * Set the default memory size for new scenes (in bytes).
     * This affects LoadSceneFromFile and LoadSceneFromString functions.
     * @param MemorySize Default memory size for scene allocators
     */
    void (*SetDefaultSceneMemorySize)(size_t MemorySize);

    /**
     * Get the current default memory size for new scenes.
     * @return Default memory size in bytes
     */
    size_t (*GetDefaultSceneMemorySize)(void);

    /**
     * Get the last error message from scene operations.
     * This covers errors from loading, parsing, and scene operations.
     * @return Error message string, or NULL if no error
     */
    const char* (*GetLastError)(void);

    // === Event Handler Management ===

    /**
     * Register an event handler for scene parsing callbacks.
     * Multiple handlers can be registered and will be called in sequence.
     * @param Events Event handler structure with callback functions
     * @return AX_SCENE_SUCCESS on success, error code on failure
     */
    AxSceneResult (*RegisterEventHandler)(const AxSceneEvents* Events);

    /**
     * Unregister a previously registered event handler.
     * @param Events Event handler structure to remove
     * @return AX_SCENE_SUCCESS on success, AX_SCENE_ERROR_NOT_FOUND if not registered
     */
    AxSceneResult (*UnregisterEventHandler)(const AxSceneEvents* Events);

    /**
     * Clear all registered event handlers.
     * This removes all event handlers from the chain.
     */
    void (*ClearEventHandlers)(void);
};

extern struct AxSceneAPI* SceneAPI;

#ifdef __cplusplus
}
#endif
