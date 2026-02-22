#pragma once

#include "Foundation/AxTypes.h"
#include "AxEngine/AxSceneTypes.h"

// Forward declaration for the C++ SceneTree class.
// In C++ code, include "AxEngine/AxSceneTree.h" for the full definition.
#ifndef __cplusplus
typedef struct SceneTree SceneTree;
#else
class SceneTree;
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define AXON_SCENE_PARSER_API_NAME "AxonSceneParserAPI"

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
 * Adapters can register callbacks to be notified when scene elements are parsed.
 * NULL function pointers are skipped (no-op behavior).
 */
typedef struct AxSceneEvents {
    // Light callback
    void (*OnLightParsed)(const AxLight* Light, void* UserData);

    // Scene structure callback
    void (*OnSceneParsed)(const SceneTree* Scene, void* UserData);

    // User data for adapter-specific state
    void* UserData;

    /**
     * Called when a node is parsed from a .ats or .axp file.
     * The NodePtr is a void* pointing to a Node* (C++ type).
     * @param NodePtr The parsed node (void* -> Node*)
     * @param UserData User data from the event handler
     */
    void (*OnNodeParsed)(void* NodePtr, void* UserData);

    /**
     * Called when a component is parsed and attached to a node.
     * The CompPtr is a void* pointing to a Component* (C++ type).
     * @param CompPtr The parsed component (void* -> Component*)
     * @param NodePtr The owning node (void* -> Node*)
     * @param UserData User data from the event handler
     */
    void (*OnComponentParsed)(void* CompPtr, void* NodePtr, void* UserData);
} AxSceneEvents;

/**
 * Scene Parser API provides .ats file loading, parsing, and event management.
 * Scene creation uses the C++ SceneTree class internally.
 * Callers receive SceneTree* which is the runtime scene container.
 */
struct AxSceneParserAPI {
    // === File Loading and Parsing ===

    /**
     * Parse a scene from a string with custom memory allocator.
     * @param Source Scene file content as string
     * @param Allocator Memory allocator for scene data (unified interface)
     * @return Parsed scene, or NULL on failure
     */
    SceneTree* (*ParseFromString)(const char* Source, struct AxAllocator* Allocator);

    /**
     * Parse a scene from a file with custom memory allocator.
     * @param FilePath Path to .ats scene file
     * @param Allocator Memory allocator for scene data (unified interface)
     * @return Parsed scene, or NULL on failure
     */
    SceneTree* (*ParseFromFile)(const char* FilePath, struct AxAllocator* Allocator);

    /**
     * Load a scene from a .ats file with automatic memory management.
     * Uses the default scene memory size for allocation.
     * @param FilePath Path to the .ats scene file
     * @return Loaded scene, or NULL on failure
     */
    SceneTree* (*LoadSceneFromFile)(const char* FilePath);

    /**
     * Load a scene from a string containing .ats format data.
     * Uses the default scene memory size for allocation.
     * @param SceneData Scene file content as string
     * @return Loaded scene, or NULL on failure
     */
    SceneTree* (*LoadSceneFromString)(const char* SceneData);

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

#ifdef __cplusplus
}
#endif
