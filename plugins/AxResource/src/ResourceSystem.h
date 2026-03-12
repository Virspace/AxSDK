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
#include "AxOpenGL/AxOpenGLTypes.h"
#include "Foundation/AxAllocator.h"
#include "Foundation/AxAPIRegistry.h"

#include <string_view>

//=============================================================================
// Resource Type Enumeration
//=============================================================================

enum class ResourceType : uint32_t {
    Texture = 0,
    Mesh,
    Shader,
    Material,
    Model,
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
    AxTextureHandle LoadTexture(std::string_view Path,
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
    AxMeshHandle LoadMesh(std::string_view Path,
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
    AxShaderHandle LoadShader(std::string_view VertexPath,
                               std::string_view FragmentPath,
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
    // Programmatic Resource Creation
    //=========================================================================

    AxMeshHandle CreateMeshFromData(struct AxVertex* Vertices, uint32_t* Indices,
                                     uint32_t VertexCount, uint32_t IndexCount,
                                     const char* Name);
    AxModelHandle CreateModelFromMesh(AxMeshHandle MeshHandle,
                                       const char* Name, const char* Path);

    //=========================================================================
    // Model Management (Handle-based - Phase 6)
    //=========================================================================

    AxModelHandle LoadModel(std::string_view Path);
    AxModelHandle CreateModelSlot();
    const struct AxModelData* GetModel(AxModelHandle Handle) const;
    bool IsModelValid(AxModelHandle Handle) const;
    uint32_t GetModelCount() const;
    AxModelHandle GetModelByIndex(uint32_t Index) const;
    AxModelHandle AcquireModel(AxModelHandle Handle);
    void ReleaseModel(AxModelHandle Handle);
    uint32_t GetModelRefCount(AxModelHandle Handle) const;

    ResourceSlot<AxModelData>* GetModelSlot(AxModelHandle Handle);
    const ResourceSlot<AxModelData>* GetModelSlot(AxModelHandle Handle) const;

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
    char* ReadFileToString(std::string_view Path, uint64_t* OutSize);

    // Internal model loading helper (populates model data)
    bool LoadModelInternal(std::string_view Path, struct AxModelData* OutModel);

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

    // Pending deletion queue
    PendingDeletion* m_PendingDeletions;
    uint32_t m_PendingCount;
    uint32_t m_PendingCapacity;

    // Texture path cache for deduplication
    TexturePathCacheEntry* m_TexturePathCache;
    uint32_t m_TexturePathCacheCapacity;
    uint32_t m_TexturePathCacheCount;

    // Texture path cache helpers
    uint32_t HashPath(std::string_view NormalizedPath) const;
    AxTextureHandle TexturePathCacheFind(std::string_view NormalizedPath);
    void TexturePathCacheInsert(std::string_view NormalizedPath, AxTextureHandle Handle);
    void TexturePathCacheRemove(AxTextureHandle Handle);

    bool m_Initialized;
};

#endif // __cplusplus
