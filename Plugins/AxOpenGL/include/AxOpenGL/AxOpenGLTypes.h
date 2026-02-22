#pragma once

#include <AxEngine/AxRenderTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * AxOpenGL Type Definitions
 *
 * This file contains the concrete OpenGL-specific definitions for types
 * declared as opaque in Foundation/AxTypes.h. These types should only be
 * used by code that directly interfaces with the OpenGL renderer.
 */

/**
 * OpenGL Texture
 * Represents a texture with OpenGL-specific handles and sampler properties.
 */
struct AxTexture
{
    uint32_t ID;             // OpenGL texture handle
    uint32_t Width;          // Texture width in pixels
    uint32_t Height;         // Texture height in pixels
    uint32_t Channels;       // Number of color channels
    bool IsSRGB;             // true for color textures (baseColor, emissive), false for data textures (normal, metallic, etc.)

    // Sampler properties
    uint32_t WrapS;          // GL_REPEAT, GL_CLAMP_TO_EDGE, GL_MIRRORED_REPEAT
    uint32_t WrapT;          // GL_REPEAT, GL_CLAMP_TO_EDGE, GL_MIRRORED_REPEAT
    uint32_t MagFilter;      // GL_NEAREST, GL_LINEAR
    uint32_t MinFilter;      // GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, etc.
};

/**
 * OpenGL Mesh
 * Represents a mesh with OpenGL buffer objects and metadata.
 */
struct AxMesh
{
    uint32_t VAO;            // OpenGL Vertex Array Object
    uint32_t VBO;            // OpenGL Vertex Buffer Object
    uint32_t EBO;            // OpenGL Element Buffer Object
    uint32_t IndexCount;     // Number of indices
    int32_t VertexOffset;    // Offset into vertex buffer
    uint32_t IndexOffset;    // Offset into index buffer

    // Indices (NOT OpenGL handles, just array indices)
    uint32_t TransformIndex;    // Index into transform array
    uint32_t BaseColorTexture;  // Index into texture array
    uint32_t NormalTexture;     // Index into texture array
    uint32_t MaterialIndex;     // Index into material array

    // Mesh name from GLTF (for debugging)
    char Name[64];
};

/**
 * OpenGL Shader Data
 * Contains shader program handle and attribute/uniform locations.
 */
struct AxShaderData
{
    uint32_t ShaderHandle;   // OpenGL shader program handle

    // Required uniforms
    int32_t AttribLocationModelMatrix;
    int32_t AttribLocationViewMatrix;
    int32_t AttribLocationProjectionMatrix;

    // Required vertex attributes
    int32_t AttribLocationVertexPos;
    int32_t AttribLocationVertexNormal;
    int32_t AttribLocationVertexUV;
    int32_t AttribLocationVertexTangent;

    // Optional uniforms
    int32_t AttribLocationLightPos;
    int32_t AttribLocationLightColor;
    int32_t AttribLocationViewPos;
    int32_t AttribLocationMaterialColor;
    int32_t AttribLocationDiffuseTexture;
    int32_t AttribLocationNormalTexture;
    int32_t AttribLocationUseDiffuseTexture;
    int32_t AttribLocationUseNormalTexture;

    // Alpha handling uniforms
    int32_t AttribLocationAlphaMode;
    int32_t AttribLocationAlphaCutoff;

    // Legacy (for backward compatibility)
    int32_t AttribLocationColor;
    int32_t AttribLocationMaterialAlpha;
    int32_t AttribLocationHasNormalMap;
};

/**
 * OpenGL Viewport
 * Represents a rendering viewport with OpenGL-specific properties.
 */
struct AxViewport
{
    AxVec2 Position;      // Viewport position
    AxVec2 Size;          // Viewport size
    AxVec2 Depth;         // Depth range (min, max)
    AxVec2 Scale;         // Viewport scale
    bool IsActive;        // Whether viewport is active
    AxVec4 ClearColor;    // Clear color (RGBA)
};

/**
 * OpenGL Material
 * Legacy material system with OpenGL-specific shader data.
 * This will be deprecated in favor of AxMaterialDesc.
 */
struct AxMaterial
{
    float BaseColorFactor[4];          // RGBA base color
    uint32_t BaseColorTexture;         // Texture index
    uint32_t NormalTexture;            // Texture index
    uint32_t MetallicRoughnessTexture; // Texture index
    char Name[64];                     // Material name
    char VertexShaderPath[256];        // Path to vertex shader
    char FragmentShaderPath[256];      // Path to fragment shader

    // Alpha handling
    AxAlphaMode AlphaMode;             // Alpha rendering mode (opaque, mask, blend)
    float AlphaCutoff;                 // Alpha cutoff value for mask mode (default 0.5)

    // Shader program storage
    uint32_t ShaderProgram;            // OpenGL shader program handle (0 = not loaded)
    void* ShaderData;                  // Pointer to AxShaderData structure
};

#ifdef __cplusplus
}
#endif
