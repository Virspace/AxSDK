#pragma once

#include "Foundation/AxTypes.h"
#include "Foundation/AxAPIRegistry.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AX_ENGINE_API_NAME "AxEngineAPI"

/**
 * Engine Handle - Opaque handle to engine instance
 */
typedef struct AxEngineHandle AxEngineHandle;

/**
 * Engine API - High-level orchestration layer for AxSDK
 *
 * The Engine provides convenient resource loading and scene management
 * by orchestrating multiple plugins (AxOpenGL, AxScene, etc.). It moves
 * resource loading logic from game code into a reusable layer.
 *
 * Games can use Engine for convenience or bypass it to use plugins directly.
 */
typedef struct AxEngineAPI {
    /**
     * Create engine instance
     * @param Registry API registry for plugin access
     * @return Engine handle or NULL on failure
     */
    AxEngineHandle* (*Create)(struct AxAPIRegistry* Registry);

    /**
     * Initialize engine and query required plugin APIs
     * @param Engine Engine handle
     * @return true on success, false if required plugins not available
     */
    bool (*Initialize)(AxEngineHandle* Engine);

    /**
     * Update engine systems (future: resource streaming, etc.)
     * @param Engine Engine handle
     * @param DeltaTime Time since last frame in seconds
     */
    void (*Update)(AxEngineHandle* Engine, float DeltaTime);

    /**
     * Destroy engine instance and free resources
     * @param Engine Engine handle
     */
    void (*Destroy)(AxEngineHandle* Engine);

    /**
     * Load scene from file using AxScene plugin
     * @param Engine Engine handle
     * @param ScenePath Path to scene file
     * @return AxScene pointer on success, NULL on failure
     */
    AxScene* (*LoadScene)(AxEngineHandle* Engine, const char* ScenePath);

    /**
     * Get currently loaded scene
     * @param Engine Engine handle
     * @return AxScene pointer or NULL if no scene loaded
     */
    AxScene* (*GetScene)(AxEngineHandle* Engine);

    /**
     * Get scene camera by index or create default if none exist
     * @param Engine Engine handle
     * @param CameraIndex Index of camera to get (0 for first/default camera)
     * @param OutTransform Pointer to receive camera transform (can be NULL)
     * @return AxCamera pointer or NULL on failure
     */
    AxCamera* (*GetSceneCamera)(AxEngineHandle* Engine, uint32_t CameraIndex, AxTransform** OutTransform);

    /**
     * Update engine systems (alias for Update - future expansion)
     * @param Engine Engine handle
     * @param DeltaTime Time since last frame in seconds
     */
    void (*UpdateSystems)(AxEngineHandle* Engine, float DeltaTime);

    /**
     * Load texture from file using STB image
     * @param Engine Engine handle
     * @param Path Path to texture file
     * @param IsSRGB true for color textures, false for data textures
     * @return Texture handle or NULL on failure
     */
    AxTexture* (*LoadTexture)(AxEngineHandle* Engine, const char* Path, bool IsSRGB);

    /**
     * Load mesh from GLTF file (geometry only)
     * @param Engine Engine handle
     * @param Path Path to GLTF/GLB file
     * @return Mesh handle or NULL on failure
     */
    AxMesh* (*LoadMesh)(AxEngineHandle* Engine, const char* Path);

    /**
     * Load complete GLTF model with materials, textures, and hierarchy
     * @param Engine Engine handle
     * @param Path Path to GLTF/GLB file
     * @param Model Pointer to AxModel structure to populate
     * @return true on success, false on failure
     */
    bool (*LoadModel)(AxEngineHandle* Engine, const char* Path, AxModel* Model);

    /**
     * Load and compile shader from files
     * @param Engine Engine handle
     * @param VertPath Path to vertex shader
     * @param FragPath Path to fragment shader
     * @return Shader program ID or 0 on failure
     */
    uint32_t (*LoadShader)(AxEngineHandle* Engine, const char* VertPath, const char* FragPath);

    /**
     * Render the loaded scene using specified camera
     * @param Engine Engine handle
     * @param Viewport Viewport for rendering
     * @param CameraIndex Index of camera to use for rendering (0 for default)
     */
    void (*RenderScene)(AxEngineHandle* Engine, const AxViewport* Viewport, uint32_t CameraIndex);
} AxEngineAPI;

#ifdef __cplusplus
}
#endif
