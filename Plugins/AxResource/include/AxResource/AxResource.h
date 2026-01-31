#pragma once

/**
 * AxResource.h - Unified Resource API
 *
 * This API provides handle-based resource management for game assets including
 * textures, meshes, shaders, materials, and models. Resources are managed with
 * automatic lifetime management through reference counting and deferred destruction.
 *
 * Key Features:
 *   - Generation-based handles prevent use-after-free
 *   - Automatic lifetime management (reference counting)
 *   - Deferred destruction (developer controls when costs are paid)
 *   - Custom allocator support for memory budgeting
 *
 * Usage:
 *   struct AxResourceAPI* API = Registry->Get(AXON_RESOURCE_API_NAME);
 *   API->Initialize(Registry, Allocator, NULL);
 *   AxTextureHandle tex = API->LoadTexture("diffuse.png", NULL);
 *   // Use texture...
 *   API->ReleaseTexture(tex);
 *   API->ProcessPendingReleases(); // At end of frame
 *   API->Shutdown();
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "AxResourceTypes.h"
#include <Foundation/AxAllocator.h>

// Forward declarations
struct AxAPIRegistry;
struct AxTexture;
struct AxMesh;
struct AxShaderData;
struct AxMaterialDesc;

#define AXON_RESOURCE_API_NAME "AxonResourceAPI"

/**
 * Resource API function pointer struct.
 * Retrieved via AxAPIRegistry->Get(AXON_RESOURCE_API_NAME).
 */
struct AxResourceAPI
{
    //=========================================================================
    // Lifecycle
    //=========================================================================

    /**
     * Initialize the resource system.
     * Must be called before any other resource functions.
     *
     * @param Registry API registry for accessing dependencies (AxOpenGLAPI, AxPlatformAPI)
     * @param Allocator Allocator for all resource allocations (required)
     * @param Options Initialization options (NULL for defaults)
     */
    void (*Initialize)(struct AxAPIRegistry* Registry,
                       struct AxAllocator* Allocator,
                       const struct AxResourceInitOptions* Options);

    /**
     * Shutdown the resource system.
     * Releases all resources and processes pending deletions.
     * Call this before destroying the allocator.
     */
    void (*Shutdown)(void);

    /**
     * Check if the resource system has been initialized.
     * @return true if initialized, false otherwise
     */
    bool (*IsInitialized)(void);

    //=========================================================================
    // Texture Management
    //=========================================================================

    /**
     * Load a texture from file (PNG, JPG, BMP, TGA via stb_image).
     * @param Path Path to the texture file
     * @param Options Loading options (NULL for defaults)
     * @return Handle to the loaded texture, or AX_INVALID_HANDLE on failure
     */
    AxTextureHandle (*LoadTexture)(const char* Path,
                                    const struct AxTextureLoadOptions* Options);

    /**
     * Get texture data for rendering.
     * @param Handle Texture handle
     * @return Pointer to texture data, or NULL if invalid handle
     */
    const struct AxTexture* (*GetTexture)(AxTextureHandle Handle);

    /**
     * Get texture metadata.
     * @param Handle Texture handle
     * @param OutInfo Output structure for texture info
     * @return true if successful, false if invalid handle
     */
    bool (*GetTextureInfo)(AxTextureHandle Handle, struct AxTextureInfo* OutInfo);

    /**
     * Check if a texture handle is still valid.
     * @param Handle Texture handle to check
     * @return true if valid, false if invalid or stale
     */
    bool (*IsTextureValid)(AxTextureHandle Handle);

    /**
     * Acquire an additional reference to a texture.
     * Increments the reference count.
     * @param Handle Texture handle
     * @return Same handle if valid, AX_INVALID_HANDLE if input was invalid
     */
    AxTextureHandle (*AcquireTexture)(AxTextureHandle Handle);

    /**
     * Release a reference to a texture.
     * When refcount hits zero, resource is queued for deletion.
     * @param Handle Texture handle
     */
    void (*ReleaseTexture)(AxTextureHandle Handle);

    //=========================================================================
    // Mesh Management
    //=========================================================================

    /**
     * Load a mesh from file (GLTF format).
     * @param Path Path to the mesh file
     * @param Options Loading options (NULL for defaults)
     * @return Handle to the loaded mesh, or AX_INVALID_HANDLE on failure
     */
    AxMeshHandle (*LoadMesh)(const char* Path,
                              const struct AxMeshLoadOptions* Options);

    /**
     * Get mesh data for rendering.
     * @param Handle Mesh handle
     * @return Pointer to mesh data, or NULL if invalid handle
     */
    const struct AxMesh* (*GetMesh)(AxMeshHandle Handle);

    /**
     * Get mesh metadata.
     * @param Handle Mesh handle
     * @param OutInfo Output structure for mesh info
     * @return true if successful, false if invalid handle
     */
    bool (*GetMeshInfo)(AxMeshHandle Handle, struct AxMeshInfo* OutInfo);

    /**
     * Check if a mesh handle is still valid.
     * @param Handle Mesh handle to check
     * @return true if valid, false if invalid or stale
     */
    bool (*IsMeshValid)(AxMeshHandle Handle);

    /**
     * Acquire an additional reference to a mesh.
     * @param Handle Mesh handle
     * @return Same handle if valid, AX_INVALID_HANDLE if input was invalid
     */
    AxMeshHandle (*AcquireMesh)(AxMeshHandle Handle);

    /**
     * Release a reference to a mesh.
     * @param Handle Mesh handle
     */
    void (*ReleaseMesh)(AxMeshHandle Handle);

    //=========================================================================
    // Shader Management
    //=========================================================================

    /**
     * Load a shader program from vertex and fragment shader files.
     * @param VertexPath Path to vertex shader file
     * @param FragmentPath Path to fragment shader file
     * @param Options Loading options (NULL for defaults)
     * @return Handle to the shader, or AX_INVALID_HANDLE on failure
     */
    AxShaderHandle (*LoadShader)(const char* VertexPath,
                                  const char* FragmentPath,
                                  const struct AxShaderLoadOptions* Options);

    /**
     * Get shader data for rendering.
     * @param Handle Shader handle
     * @return Pointer to shader data, or NULL if invalid handle
     */
    const struct AxShaderData* (*GetShader)(AxShaderHandle Handle);

    /**
     * Get OpenGL program ID directly (convenience function).
     * @param Handle Shader handle
     * @return OpenGL program ID, or 0 if invalid handle
     */
    uint32_t (*GetShaderProgram)(AxShaderHandle Handle);

    /**
     * Get shader metadata.
     * @param Handle Shader handle
     * @param OutInfo Output structure for shader info
     * @return true if successful, false if invalid handle
     */
    bool (*GetShaderInfo)(AxShaderHandle Handle, struct AxShaderInfo* OutInfo);

    /**
     * Check if a shader handle is still valid.
     * @param Handle Shader handle to check
     * @return true if valid, false if invalid or stale
     */
    bool (*IsShaderValid)(AxShaderHandle Handle);

    /**
     * Acquire an additional reference to a shader.
     * @param Handle Shader handle
     * @return Same handle if valid, AX_INVALID_HANDLE if input was invalid
     */
    AxShaderHandle (*AcquireShader)(AxShaderHandle Handle);

    /**
     * Release a reference to a shader.
     * @param Handle Shader handle
     */
    void (*ReleaseShader)(AxShaderHandle Handle);

    //=========================================================================
    // Material Management
    //=========================================================================

    /**
     * Create a material from a description.
     * @param Desc Material description
     * @return Handle to the material, or AX_INVALID_HANDLE on failure
     */
    AxMaterialHandle (*CreateMaterial)(const struct AxMaterialDesc* Desc);

    /**
     * Get material data.
     * @param Handle Material handle
     * @return Pointer to material description, or NULL if invalid handle
     */
    const struct AxMaterialDesc* (*GetMaterial)(AxMaterialHandle Handle);

    /**
     * Check if a material handle is still valid.
     * @param Handle Material handle to check
     * @return true if valid, false if invalid or stale
     */
    bool (*IsMaterialValid)(AxMaterialHandle Handle);

    /**
     * Acquire an additional reference to a material.
     * @param Handle Material handle
     * @return Same handle if valid, AX_INVALID_HANDLE if input was invalid
     */
    AxMaterialHandle (*AcquireMaterial)(AxMaterialHandle Handle);

    /**
     * Release a reference to a material.
     * @param Handle Material handle
     */
    void (*ReleaseMaterial)(AxMaterialHandle Handle);

    //=========================================================================
    // Model Management (Handle-based)
    //=========================================================================

    /**
     * Load a complete model from GLTF (meshes, textures, materials).
     * Orchestrates LoadTexture, LoadMesh, and CreateMaterial for all
     * resources referenced in the GLTF file. The model is tracked internally
     * and returns a handle for future reference.
     *
     * @param Path Path to the GLTF file
     * @return Handle to the loaded model, or AX_INVALID_HANDLE on failure
     */
    AxModelHandle (*LoadModel)(const char* Path);

    /**
     * Get model data by handle.
     * @param Handle Model handle
     * @return Pointer to model data, or NULL if invalid handle
     */
    const struct AxModelData* (*GetModel)(AxModelHandle Handle);

    /**
     * Check if a model handle is still valid.
     * @param Handle Model handle to check
     * @return true if valid, false if invalid or stale
     */
    bool (*IsModelValid)(AxModelHandle Handle);

    /**
     * Get the total count of loaded models.
     * @return Number of models currently loaded
     */
    uint32_t (*GetModelCount)(void);

    /**
     * Get a model by index for iteration.
     * Index is NOT the same as handle index - this iterates over all loaded models.
     * @param Index Iteration index (0 to GetModelCount()-1)
     * @return Handle to the model at this index, or AX_INVALID_HANDLE if out of range
     */
    AxModelHandle (*GetModelByIndex)(uint32_t Index);

    /**
     * Acquire an additional reference to a model.
     * Also acquires references to all child resources (textures, meshes, materials).
     * @param Handle Model handle
     * @return Same handle if valid, AX_INVALID_HANDLE if input was invalid
     */
    AxModelHandle (*AcquireModel)(AxModelHandle Handle);

    /**
     * Release a reference to a model.
     * Also releases references to all child resources (textures, meshes, materials).
     * When refcount hits zero, model and all unreferenced children are queued for deletion.
     * @param Handle Model handle
     */
    void (*ReleaseModel)(AxModelHandle Handle);

    //=========================================================================
    // Scene Management
    //=========================================================================

    /**
     * Load a scene from file (.ats format).
     * Parses the scene file and loads all referenced meshes/models.
     * @param Path Path to the scene file
     * @return Handle to the loaded scene, or AX_INVALID_HANDLE on failure
     */
    AxSceneHandle (*LoadScene)(const char* Path);

    /**
     * Get scene data by handle.
     * @param Handle Scene handle
     * @return Pointer to scene data, or NULL if invalid handle
     */
    struct AxScene* (*GetScene)(AxSceneHandle Handle);

    /**
     * Check if a scene handle is still valid.
     * @param Handle Scene handle to check
     * @return true if valid, false if invalid or stale
     */
    bool (*IsSceneValid)(AxSceneHandle Handle);

    /**
     * Acquire an additional reference to a scene.
     * @param Handle Scene handle
     * @return Same handle if valid, AX_INVALID_HANDLE if input was invalid
     */
    AxSceneHandle (*AcquireScene)(AxSceneHandle Handle);

    /**
     * Release a reference to a scene.
     * When refcount hits zero, scene and all referenced models are queued for deletion.
     * @param Handle Scene handle
     */
    void (*ReleaseScene)(AxSceneHandle Handle);

    /**
     * Get reference count for a scene (debugging).
     * @param Handle Scene handle
     * @return Reference count, or 0 if invalid handle
     */
    uint32_t (*GetSceneRefCount)(AxSceneHandle Handle);

    //=========================================================================
    // Deferred Destruction
    //=========================================================================

    /**
     * Process all pending deletions.
     * Call this at end of frame after SwapBuffers to avoid frame hitches.
     */
    void (*ProcessPendingReleases)(void);

    /**
     * Process up to MaxCount deletions.
     * Use this to spread deletion cost over multiple frames.
     * @param MaxCount Maximum number of deletions to process
     */
    void (*ProcessPendingReleasesLimited)(uint32_t MaxCount);

    /**
     * Get number of resources awaiting deletion.
     * @return Number of pending deletions
     */
    uint32_t (*GetPendingReleaseCount)(void);

    //=========================================================================
    // Statistics and Debugging
    //=========================================================================

    /**
     * Get aggregate statistics about the resource system.
     * @param OutStats Output structure for statistics
     */
    void (*GetStats)(struct AxResourceStats* OutStats);

    /**
     * Get reference count for a texture (debugging).
     * @param Handle Texture handle
     * @return Reference count, or 0 if invalid handle
     */
    uint32_t (*GetTextureRefCount)(AxTextureHandle Handle);

    /**
     * Get reference count for a mesh (debugging).
     * @param Handle Mesh handle
     * @return Reference count, or 0 if invalid handle
     */
    uint32_t (*GetMeshRefCount)(AxMeshHandle Handle);

    /**
     * Get reference count for a shader (debugging).
     * @param Handle Shader handle
     * @return Reference count, or 0 if invalid handle
     */
    uint32_t (*GetShaderRefCount)(AxShaderHandle Handle);

    /**
     * Get reference count for a material (debugging).
     * @param Handle Material handle
     * @return Reference count, or 0 if invalid handle
     */
    uint32_t (*GetMaterialRefCount)(AxMaterialHandle Handle);

    /**
     * Get reference count for a model (debugging).
     * @param Handle Model handle
     * @return Reference count, or 0 if invalid handle
     */
    uint32_t (*GetModelRefCount)(AxModelHandle Handle);
};

#if defined(AXON_LINKS_FOUNDATION)
extern struct AxResourceAPI* ResourceAPI;
#endif

#ifdef __cplusplus
}
#endif
