#pragma once

#include "Foundation/AxTypes.h"
#include "AxOpenGL/AxOpenGLTypes.h"

class SceneTree;
class Node;
struct Tokenizer;
struct AxAPIRegistry;
struct AxAllocator;
struct AxAllocatorAPI;
struct AxPlatformAPI;
struct AxHashTableAPI;

/**
 * Scene API result codes for error handling
 */
enum AxSceneResult {
    AX_SCENE_SUCCESS = 0,               // Operation completed successfully
    AX_SCENE_ERROR_INVALID_ARGUMENTS,  // NULL pointer or invalid parameter
    AX_SCENE_ERROR_OUT_OF_MEMORY,      // Memory allocation failed or limit reached
    AX_SCENE_ERROR_NOT_FOUND,          // Event handler not found in registry
    AX_SCENE_ERROR_ALREADY_EXISTS,     // Event handler already registered
    AX_SCENE_ERROR_INTERNAL            // Internal error occurred
};

/**
 * Scene Events structure provides callback interface for scene parsing adapters.
 * Adapters can register callbacks to be notified when scene elements are parsed.
 * NULL function pointers are skipped (no-op behavior).
 */
struct AxSceneEvents {
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
};

/**
 * SceneParser - Internal scene file parser for .ats and .axp formats.
 *
 * Owns tokenizer state, event handlers, and API references needed for parsing.
 * Used directly by AxEngine (not exposed as a plugin API).
 */
class SceneParser
{
public:
    SceneParser() = default;
    ~SceneParser() = default;

    // Non-copyable
    SceneParser(const SceneParser&) = delete;
    SceneParser& operator=(const SceneParser&) = delete;

    // === Lifecycle ===

    void Init(AxAPIRegistry* Registry);
    void Term();

    // === File Loading and Parsing ===

    /**
     * Parse a scene from a string with custom memory allocator.
     * @param Source Scene file content as string
     * @param Allocator Memory allocator for scene data (unified interface)
     * @return Parsed scene, or nullptr on failure
     */
    SceneTree* ParseFromString(const char* Source, AxAllocator* Allocator);

    /**
     * Parse a scene from a file with custom memory allocator.
     * @param FilePath Path to .ats scene file
     * @param Allocator Memory allocator for scene data (unified interface)
     * @return Parsed scene, or nullptr on failure
     */
    SceneTree* ParseFromFile(const char* FilePath, AxAllocator* Allocator);

    /**
     * Load a scene from a .ats file with automatic memory management.
     * Uses the default scene memory size for allocation.
     * @param FilePath Path to the .ats scene file
     * @return Loaded scene, or nullptr on failure
     */
    SceneTree* LoadSceneFromFile(const char* FilePath);

    /**
     * Load a scene from a string containing .ats format data.
     * Uses the default scene memory size for allocation.
     * @param SceneData Scene file content as string
     * @return Loaded scene, or nullptr on failure
     */
    SceneTree* LoadSceneFromString(const char* SceneData);

    // === Prefab Loading and Instantiation ===

    /**
     * Parse a prefab (.axp) string into a node subtree attached to Scene.
     * Prefab format: a single top-level "node" block with no scene wrapper.
     */
    Node* ParsePrefab(const char* PrefabData, SceneTree* Scene);

    /**
     * Deep-copy a prefab node subtree into Scene, creating new independent nodes.
     * Creates new typed node instances via Scene->CreateNode and copies data fields.
     */
    Node* InstantiatePrefab(SceneTree* Scene, Node* PrefabRoot, Node* Parent);

    // === Configuration and Error Handling ===

    void SetDefaultSceneMemorySize(size_t MemorySize);
    size_t GetDefaultSceneMemorySize() const;
    const char* GetLastError() const;

    // === Event Handler Management ===

    AxSceneResult RegisterEventHandler(const AxSceneEvents* Events);
    AxSceneResult UnregisterEventHandler(const AxSceneEvents* Events);
    void ClearEventHandlers();

    // Used by internal file-local parsing helpers in AxSceneParser.cpp
    void SetParserError(Tokenizer* T, const char* Format, ...);

private:
    // API references (set during Init)
    AxAPIRegistry* Registry_{nullptr};
    AxAllocatorAPI* AllocatorAPI_{nullptr};
    AxPlatformAPI*  PlatformAPI_{nullptr};
    AxHashTableAPI* HashTableAPI_{nullptr};

    // Error buffers
    char LastErrorMessage_[256]{};
    char ParserLastErrorMessage_[256]{};

    // Event handlers
    static constexpr int MaxEventHandlers_ = 16;
    AxSceneEvents EventHandlers_[MaxEventHandlers_]{};
    int EventHandlerCount_{0};

    // Configuration
    size_t DefaultSceneMemorySize_{1024 * 1024};

    // Internal parsing (implementation in AxSceneParser.cpp)
    Node* ParseNodeImpl(Tokenizer* T, SceneTree* Scene, Node* Parent);
    SceneTree* ParseSceneImpl(Tokenizer* T);

    // Internal helpers
    void SetError(const char* Format, ...);
    void InvokeOnLightParsed(const AxLight* Light);
    void InvokeOnSceneParsed(const SceneTree* Scene);
    void InvokeOnNodeParsed(Node* ParsedNode);
};
