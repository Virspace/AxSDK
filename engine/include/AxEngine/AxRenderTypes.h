#pragma once

/**
 * AxRenderTypes.h - Rendering Type Definitions
 *
 * Defines rendering-specific data types that are engine concerns, not
 * fundamental math/utility types. These live at the Engine layer rather
 * than Foundation because they describe rendering concepts (materials,
 * vertices, models) that are only meaningful in a rendering context.
 *
 * This header is C-compatible so it can be included by C rendering
 * plugins (AxOpenGL) as well as C++ engine code.
 */

#include "Foundation/AxTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque renderer-specific types (concrete definitions provided by renderer plugins)
typedef struct AxTexture AxTexture;
typedef struct AxMesh AxMesh;
typedef struct AxShaderData AxShaderData;
typedef struct AxViewport AxViewport;
typedef struct AxMaterial AxMaterial;

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

typedef struct AxVertex
{
    AxVec3 Position;
    AxVec3 Normal;
    AxVec2 TexCoord;
    AxVec4 Tangent; // XYZ = tangent vector, W = handedness (-1.0 or 1.0)
} AxVertex;

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
