#pragma once

#include "Foundation/AxTypes.h"

class AxResource
{
    /**
     * Load and compile shader from files
     * @param Engine Engine handle
     * @param VertPath Path to vertex shader
     * @param FragPath Path to fragment shader
     * @return Shader program ID or 0 on failure
     */
    uint32_t LoadShader(const char* VertPath, const char* FragPath);

        /**
     * Get scene camera by index or create default if none exist
     * @param Engine Engine handle
     * @param CameraIndex Index of camera to get (0 for first/default camera)
     * @param OutTransform Pointer to receive camera transform (can be NULL)
     * @return AxCamera pointer or NULL on failure
     */
    AxCamera* GetSceneCamera(uint32_t CameraIndex, AxTransform** OutTransform);

    /**
     * Load texture from file using STB image
     * @param Engine Engine handle
     * @param Path Path to texture file
     * @param IsSRGB true for color textures, false for data textures
     * @return Texture handle or NULL on failure
     */
    AxTexture* LoadTexture(const char* Path, bool IsSRGB);

    /**
     * Load mesh from GLTF file (geometry only)
     * @param Engine Engine handle
     * @param Path Path to GLTF/GLB file
     * @return Mesh handle or NULL on failure
     */
    AxMesh* LoadMesh(const char* Path);

    /**
     * Load complete GLTF model with materials, textures, and hierarchy
     * @param Engine Engine handle
     * @param Path Path to GLTF/GLB file
     * @param Model Pointer to AxModel structure to populate
     * @return true on success, false on failure
     */
    bool LoadModel(const char* Path, AxModel* Model);

    /**
     * Load scene from file using AxScene plugin
     * @param Engine Engine handle
     * @param ScenePath Path to scene file
     * @return AxScene pointer on success, NULL on failure
     */
    AxScene* LoadScene(const char* ScenePath);
};