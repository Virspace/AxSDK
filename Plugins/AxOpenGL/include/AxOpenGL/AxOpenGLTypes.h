#pragma once

/**
 * AxOpenGLTypes.h - Rendering and Scene Type Definitions
 *
 * All rendering-related types in one header: scene types (lights, cameras),
 * renderer-agnostic descriptions (materials, vertices, models), and concrete
 * OpenGL implementations (textures, meshes, shaders, viewports).
 *
 * This header is C-compatible so it can be included by C rendering
 * plugins as well as C++ engine code.
 */

#include "Foundation/AxTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Scene Types (lights, cameras)
//=============================================================================

/**
 * Light types supported by the scene system
 */
typedef enum AxLightType {
    AX_LIGHT_TYPE_DIRECTIONAL = 0,
    AX_LIGHT_TYPE_POINT,
    AX_LIGHT_TYPE_SPOT
} AxLightType;

/**
 * Light represents a light source in the scene.
 * Lights can be directional, point, or spot lights.
 */
typedef struct AxLight {
    char Name[64];                      // Light identifier
    AxLightType Type;                   // Type of light
    AxVec3 Position;                    // Staging: filled by renderer from node world transform before GPU submission
    AxVec3 Color;                       // Light color (RGB)
    float Intensity;                    // Light intensity multiplier
    float Range;                        // Range for point/spot lights (0 = infinite)
    float InnerConeAngle;               // Inner cone angle for spot lights (radians)
    float OuterConeAngle;               // Outer cone angle for spot lights (radians)
} AxLight;

typedef struct AxCamera
{
    bool IsOrthographic;        // Orthographic or projection
    float FieldOfView;          // Field of view for perspective projection
    float ZoomLevel;            // Zoom level for orthographic projection
    float AspectRatio;          // Aspect ratio for the camera
    float NearClipPlane;        // Near clipping plane
    float FarClipPlane;         // Far clipping plane
    AxMat4x4 ViewMatrix;        // View matrix
    AxMat4x4 ProjectionMatrix;  // Projection matrix
} AxCamera;

//=============================================================================
// Material Enums and Descriptions (renderer-agnostic)
//=============================================================================

typedef enum AxAlphaMode {
    AX_ALPHA_MODE_OPAQUE = 0,
    AX_ALPHA_MODE_MASK = 1,
    AX_ALPHA_MODE_BLEND = 2
} AxAlphaMode;

/**
 * Material type discriminator for AxMaterialDesc.
 * Determines whether the material uses standard PBR or custom shaders.
 */
typedef enum AxMaterialType {
    AX_MATERIAL_TYPE_PBR = 0,    // Standard PBR material
    AX_MATERIAL_TYPE_CUSTOM = 1  // Custom shader material
} AxMaterialType;

/**
 * Standard PBR material description.
 * Contains renderer-agnostic PBR properties following the glTF 2.0 specification.
 * All texture references use indices into a texture array (-1 = no texture).
 */
typedef struct AxPBRMaterial {
    AxVec4 BaseColorFactor;           // RGBA base color (default: 1,1,1,1)
    AxVec3 EmissiveFactor;            // RGB emissive color (default: 0,0,0)
    float MetallicFactor;             // Metallic value [0-1] (default: 1)
    float RoughnessFactor;            // Roughness value [0-1] (default: 1)
    int32_t BaseColorTexture;         // Index into texture array (-1 = none)
    int32_t MetallicRoughnessTexture; // Index into texture array (-1 = none)
    int32_t NormalTexture;            // Index into texture array (-1 = none)
    int32_t EmissiveTexture;          // Index into texture array (-1 = none)
    int32_t OcclusionTexture;         // Index into texture array (-1 = none)
    AxAlphaMode AlphaMode;            // Alpha rendering mode
    float AlphaCutoff;                // Alpha cutoff for mask mode [0-1] (default: 0.5)
    bool DoubleSided;                 // Render both faces (default: false)
} AxPBRMaterial;

/**
 * Custom shader material description.
 * Allows renderer-specific custom shader implementations.
 */
typedef struct AxCustomMaterial {
    char VertexShaderPath[256];   // Path to vertex shader
    char FragmentShaderPath[256]; // Path to fragment shader
    void* CustomData;             // Renderer-specific data
    uint32_t CustomDataSize;      // Size of custom data
} AxCustomMaterial;

/**
 * Material description with type discriminator.
 * This is a renderer-agnostic material representation that can be interpreted
 * by any rendering plugin (OpenGL, Vulkan, DirectX, etc.).
 */
typedef struct AxMaterialDesc {
    char Name[64];        // Material name
    AxMaterialType Type;  // Material type discriminator
    union {
        AxPBRMaterial PBR;      // Standard PBR material
        AxCustomMaterial Custom; // Custom shader material
    };
} AxMaterialDesc;

//=============================================================================
// Vertex and Model (renderer-agnostic layout, OpenGL buffer handles)
//=============================================================================

typedef struct AxVertex
{
    AxVec3 Position;
    AxVec3 Normal;
    AxVec2 TexCoord;
    AxVec4 Tangent; // XYZ = tangent vector, W = handedness (-1.0 or 1.0)
} AxVertex;

//=============================================================================
// OpenGL Concrete Types
//=============================================================================

/**
 * OpenGL Texture
 * Represents a texture with OpenGL-specific handles and sampler properties.
 */
typedef struct AxTexture
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
} AxTexture;

/**
 * OpenGL Mesh
 * Represents a mesh with OpenGL buffer objects and metadata.
 */
typedef struct AxMesh
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
} AxMesh;

/**
 * OpenGL Shader Data
 * Contains shader program handle and attribute/uniform locations.
 */
typedef struct AxShaderData
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
} AxShaderData;

/**
 * OpenGL Viewport
 * Represents a rendering viewport with OpenGL-specific properties.
 */
typedef struct AxViewport
{
    AxVec2 Position;      // Viewport position
    AxVec2 Size;          // Viewport size
    AxVec2 Depth;         // Depth range (min, max)
    AxVec2 Scale;         // Viewport scale
    bool IsActive;        // Whether viewport is active
    AxVec4 ClearColor;    // Clear color (RGBA)
} AxViewport;

/**
 * OpenGL Material
 * Legacy material system with OpenGL-specific shader data.
 * This will be deprecated in favor of AxMaterialDesc.
 */
typedef struct AxMaterial
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
} AxMaterial;

/**
 * Model - aggregates meshes, textures, materials, and transforms for a loaded asset.
 */
typedef struct AxModel
{
    AxMesh *Meshes;
    AxTexture *Textures;
    AxMaterial *Materials;          // Legacy material system (will be deprecated)
    AxMaterialDesc *MaterialDescs;  // New material description system
    AxMat4x4 *Transforms;
    uint32_t InputLayout;
    uint32_t VertexBuffer;
    uint32_t IndexBuffer;
    uint32_t *Commands;
    uint32_t *ObjectData;
    uint32_t TransformData;
} AxModel;

#ifdef __cplusplus
}
#endif
