#pragma once

/**
 * ResourceSystem.h - Internal Resource System Implementation
 *
 * This is the internal C++ implementation of the resource system.
 * It provides the core functionality for handle management, reference counting,
 * deferred destruction, and resource loading. This header is NOT part of the public API.
 *
 * Phase 3 adds actual resource loading functionality:
 * - TextureManager: stb_image loading + OpenGL upload
 * - MeshManager: cgltf loading + OpenGL buffer creation
 * - ShaderManager: GLSL compilation + program linking
 * - MaterialManager: AxMaterialDesc creation
 *
 * Phase 4 adds model loading convenience API:
 * - LoadModel: orchestrates texture/mesh/material loading for GLTF files
 * - ReleaseModel: releases all handles acquired by LoadModel
 * - Statistics and debugging functions
 *
 * Phase 6 adds handle-based model tracking:
 * - ModelManager: slot array for model tracking with reference counting
 * - LoadModel returns AxModelHandle instead of bool
 * - AcquireModel/ReleaseModel for proper lifetime management
 */

#ifdef __cplusplus

#include "AxResource/AxResourceTypes.h"
#include "Foundation/AxAllocator.h"
#include "Foundation/AxAPIRegistry.h"

#include <cstdint>
#include <cstddef>

//=============================================================================
// Resource Type Enumeration
//=============================================================================

enum class ResourceType : uint32_t {
    Texture = 0,
    Mesh,
    Shader,
    Material,
    Model,
    Scene,
    Count
};

//=============================================================================
// Pending Deletion Entry
//=============================================================================

struct PendingDeletion {
    ResourceType Type;
    uint32_t SlotIndex;
    uint32_t Generation;  // Store generation at time of queuing
    uint32_t FrameQueued; // Optional: for frame-delay strategy
};

//=============================================================================
// Texture Path Cache Entry
//=============================================================================

/**
 * Entry in the texture path cache for deduplication.
 * Maps normalized paths to texture handles for O(1) lookup.
 */
struct TexturePathCacheEntry {
    char Path[260];           // Normalized path (MAX_PATH)
    AxTextureHandle Handle;   // Texture handle
    uint32_t Hash;            // Cached hash for faster comparison
    bool InUse;               // True if entry is occupied
};

//=============================================================================
// Generic Resource Slot Template
//=============================================================================

/**
 * ResourceSlot holds resource data along with reference counting and
 * generation tracking for handle validation.
 */
template<typename T>
struct ResourceSlot {
    T* Data;                 // Resource data (nullptr if slot is free)
    uint32_t RefCount;       // Reference count (0 = free)
    uint32_t Generation;     // Current generation (increments on reuse)
    uint32_t NextFree;       // Index of next free slot (for free list)
    bool InUse;              // True if slot contains valid data
};

//=============================================================================
// Resource Slot Array
//=============================================================================

/**
 * Generic resource slot array with handle management.
 * Manages a growable array of resource slots with generation-based handles.
 */
template<typename T>
class ResourceSlotArray {
public:
    ResourceSlotArray();
    ~ResourceSlotArray();

    // Initialize with allocator and initial capacity
    bool Initialize(struct AxAllocator* Allocator, uint32_t InitialCapacity);

    // Shutdown and free all resources
    void Shutdown();

    // Allocate a new slot, returns handle
    struct AxResourceHandle Allocate();

    // Free a slot (marks as free, increments generation)
    void Free(uint32_t Index);

    // Check if a handle is valid
    bool IsValid(struct AxResourceHandle Handle) const;

    // Get slot by handle (returns nullptr if invalid)
    ResourceSlot<T>* GetSlot(struct AxResourceHandle Handle);
    const ResourceSlot<T>* GetSlot(struct AxResourceHandle Handle) const;

    // Get slot by index directly (no validation)
    ResourceSlot<T>* GetSlotByIndex(uint32_t Index);
    const ResourceSlot<T>* GetSlotByIndex(uint32_t Index) const;

    // Get count statistics
    uint32_t GetCapacity() const { return m_Capacity; }
    uint32_t GetUsedCount() const { return m_UsedCount; }

private:
    bool Grow(uint32_t NewCapacity);

    ResourceSlot<T>* m_Slots;
    struct AxAllocator* m_Allocator;
    uint32_t m_Capacity;
    uint32_t m_UsedCount;
    uint32_t m_FreeListHead;  // Head of free slot list
};

//=============================================================================
// Resource System Class
//=============================================================================

// Forward declare texture/mesh/shader data structures
struct AxTexture;
struct AxMesh;
struct AxShaderData;
struct AxMaterialDesc;
struct AxVertex;
struct AxModelData;
struct AxScene;
struct AxSceneAPI;

/**
 * ResourceSystem is the main coordinator for all resource management.
 * It owns the slot arrays for each resource type and manages deferred destruction.
 */
class ResourceSystem {
public:
    static ResourceSystem& Get();

    // Lifecycle
    bool Initialize(struct AxAPIRegistry* Registry,
                    struct AxAllocator* Allocator,
                    const struct AxResourceInitOptions* Options);
    void Shutdown();
    bool IsInitialized() const { return m_Initialized; }

    // Accessors
    struct AxAllocator* GetAllocator() { return m_Allocator; }
    struct AxAPIRegistry* GetRegistry() { return m_Registry; }

    //=========================================================================
    // Texture Management
    //=========================================================================
    AxTextureHandle LoadTexture(const char* Path,
                                const struct AxTextureLoadOptions* Options);
    AxTextureHandle CreateTextureSlot();
    bool IsTextureValid(AxTextureHandle Handle) const;
    ResourceSlot<AxTexture>* GetTextureSlot(AxTextureHandle Handle);
    const ResourceSlot<AxTexture>* GetTextureSlot(AxTextureHandle Handle) const;
    AxTextureHandle AcquireTexture(AxTextureHandle Handle);
    void ReleaseTexture(AxTextureHandle Handle);
    uint32_t GetTextureRefCount(AxTextureHandle Handle) const;
    uint32_t GetTextureCount() const;

    //=========================================================================
    // Mesh Management
    //=========================================================================
    AxMeshHandle LoadMesh(const char* Path,
                          const struct AxMeshLoadOptions* Options);
    AxMeshHandle CreateMeshSlot();
    bool IsMeshValid(AxMeshHandle Handle) const;
    ResourceSlot<AxMesh>* GetMeshSlot(AxMeshHandle Handle);
    const ResourceSlot<AxMesh>* GetMeshSlot(AxMeshHandle Handle) const;
    AxMeshHandle AcquireMesh(AxMeshHandle Handle);
    void ReleaseMesh(AxMeshHandle Handle);
    uint32_t GetMeshRefCount(AxMeshHandle Handle) const;
    uint32_t GetMeshCount() const;

    //=========================================================================
    // Shader Management
    //=========================================================================
    AxShaderHandle LoadShader(const char* VertexPath,
                               const char* FragmentPath,
                               const struct AxShaderLoadOptions* Options);
    AxShaderHandle CreateShaderSlot();
    bool IsShaderValid(AxShaderHandle Handle) const;
    ResourceSlot<AxShaderData>* GetShaderSlot(AxShaderHandle Handle);
    const ResourceSlot<AxShaderData>* GetShaderSlot(AxShaderHandle Handle) const;
    AxShaderHandle AcquireShader(AxShaderHandle Handle);
    void ReleaseShader(AxShaderHandle Handle);
    uint32_t GetShaderRefCount(AxShaderHandle Handle) const;
    uint32_t GetShaderCount() const;

    //=========================================================================
    // Material Management
    //=========================================================================
    AxMaterialHandle CreateMaterial(const struct AxMaterialDesc* Desc);
    AxMaterialHandle CreateMaterialSlot();
    bool IsMaterialValid(AxMaterialHandle Handle) const;
    ResourceSlot<AxMaterialDesc>* GetMaterialSlot(AxMaterialHandle Handle);
    const ResourceSlot<AxMaterialDesc>* GetMaterialSlot(AxMaterialHandle Handle) const;
    AxMaterialHandle AcquireMaterial(AxMaterialHandle Handle);
    void ReleaseMaterial(AxMaterialHandle Handle);
    uint32_t GetMaterialRefCount(AxMaterialHandle Handle) const;
    uint32_t GetMaterialCount() const;

    //=========================================================================
    // Model Management (Handle-based - Phase 6)
    //=========================================================================

    /**
     * Load a complete model from GLTF file.
     * Orchestrates LoadTexture, LoadMesh, and CreateMaterial for all
     * resources referenced in the GLTF file. Returns a handle to the model.
     *
     * @param Path Path to the GLTF file
     * @return Handle to the loaded model, or AX_INVALID_HANDLE on failure
     */
    AxModelHandle LoadModel(const char* Path);

    /**
     * Get model data by handle.
     * @param Handle Model handle
     * @return Pointer to model data, or NULL if invalid handle
     */
    const struct AxModelData* GetModel(AxModelHandle Handle) const;

    /**
     * Check if a model handle is still valid.
     * @param Handle Model handle to check
     * @return true if valid, false if invalid or stale
     */
    bool IsModelValid(AxModelHandle Handle) const;

    /**
     * Get the total count of loaded models.
     * @return Number of models currently loaded
     */
    uint32_t GetModelCount() const;

    /**
     * Get a model handle by iteration index.
     * @param Index Iteration index (0 to GetModelCount()-1)
     * @return Handle to the model at this index, or AX_INVALID_HANDLE if out of range
     */
    AxModelHandle GetModelByIndex(uint32_t Index) const;

    /**
     * Acquire an additional reference to a model.
     * Also acquires references to all child resources (textures, meshes, materials).
     * @param Handle Model handle
     * @return Same handle if valid, AX_INVALID_HANDLE if input was invalid
     */
    AxModelHandle AcquireModel(AxModelHandle Handle);

    /**
     * Release a reference to a model.
     * Also releases references to all child resources (textures, meshes, materials).
     * When refcount hits zero, model is queued for deletion.
     * @param Handle Model handle
     */
    void ReleaseModel(AxModelHandle Handle);

    /**
     * Get reference count for a model.
     * @param Handle Model handle
     * @return Reference count, or 0 if invalid handle
     */
    uint32_t GetModelRefCount(AxModelHandle Handle) const;

    //=========================================================================
    // Scene Management (Handle-based)
    //=========================================================================

    /**
     * Load a scene from file (.ats format) and all referenced models.
     * @param Path Path to the scene file
     * @return Handle to the loaded scene, or AX_INVALID_HANDLE on failure
     */
    AxSceneHandle LoadScene(const char* Path);

    /**
     * Get scene data by handle.
     * @param Handle Scene handle
     * @return Pointer to scene data, or NULL if invalid handle
     */
    struct AxScene* GetScene(AxSceneHandle Handle);

    /**
     * Check if a scene handle is still valid.
     * @param Handle Scene handle to check
     * @return true if valid, false if invalid or stale
     */
    bool IsSceneValid(AxSceneHandle Handle) const;

    /**
     * Acquire an additional reference to a scene.
     * @param Handle Scene handle
     * @return Same handle if valid, AX_INVALID_HANDLE if input was invalid
     */
    AxSceneHandle AcquireScene(AxSceneHandle Handle);

    /**
     * Release a reference to a scene.
     * When refcount hits zero, scene and all referenced models are queued for deletion.
     * @param Handle Scene handle
     */
    void ReleaseScene(AxSceneHandle Handle);

    /**
     * Get reference count for a scene.
     * @param Handle Scene handle
     * @return Reference count, or 0 if invalid handle
     */
    uint32_t GetSceneRefCount(AxSceneHandle Handle) const;

    //=========================================================================
    // Deferred Destruction
    //=========================================================================
    void QueueForDeletion(ResourceType Type, uint32_t SlotIndex, uint32_t Generation);
    void ProcessPendingReleases();
    void ProcessPendingReleasesLimited(uint32_t MaxCount);
    uint32_t GetPendingReleaseCount() const;

    //=========================================================================
    // Statistics
    //=========================================================================
    void GetStats(struct AxResourceStats* OutStats) const;

private:
    ResourceSystem();
    ~ResourceSystem();

    // No copying
    ResourceSystem(const ResourceSystem&) = delete;
    ResourceSystem& operator=(const ResourceSystem&) = delete;

    // Process a single deletion
    void ProcessDeletion(const PendingDeletion& Deletion);

    // Internal helpers for resource loading
    char* ReadFileToString(const char* Path, uint64_t* OutSize);

    // Helper to extract base path from file path
    void ExtractBasePath(const char* FilePath, char* OutBasePath, size_t MaxLen);

    // Internal model loading helper (populates model data)
    bool LoadModelInternal(const char* Path, struct AxModelData* OutModel);

    // Dependencies
    struct AxAllocator* m_Allocator;
    struct AxAPIRegistry* m_Registry;
    struct AxOpenGLAPI* m_RenderAPI;
    struct AxPlatformAPI* m_PlatformAPI;

    // Resource slot arrays
    ResourceSlotArray<AxTexture> m_Textures;
    ResourceSlotArray<AxMesh> m_Meshes;
    ResourceSlotArray<AxShaderData> m_Shaders;
    ResourceSlotArray<AxMaterialDesc> m_Materials;
    ResourceSlotArray<AxModelData> m_Models;
    ResourceSlotArray<AxScene> m_Scenes;

    // SceneAPI for scene loading
    struct AxSceneAPI* m_SceneAPI;

    // Pending deletion queue
    PendingDeletion* m_PendingDeletions;
    uint32_t m_PendingCount;
    uint32_t m_PendingCapacity;

    // Texture path cache for deduplication
    TexturePathCacheEntry* m_TexturePathCache;
    uint32_t m_TexturePathCacheCapacity;
    uint32_t m_TexturePathCacheCount;

    // Texture path cache helpers
    uint32_t HashPath(const char* NormalizedPath) const;
    AxTextureHandle TexturePathCacheFind(const char* NormalizedPath);
    void TexturePathCacheInsert(const char* NormalizedPath, AxTextureHandle Handle);
    void TexturePathCacheRemove(AxTextureHandle Handle);

    bool m_Initialized;
};

#endif // __cplusplus
