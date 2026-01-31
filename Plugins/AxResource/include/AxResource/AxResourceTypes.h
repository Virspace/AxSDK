#pragma once

/**
 * AxResourceTypes.h - Resource Handle Types and Options
 *
 * This file contains the handle types, enums, and option structs for the
 * Resource API. All resource types use generation-based handles to detect
 * stale references and prevent use-after-free bugs.
 *
 * Handle Design:
 *   - 8 bytes total (pass by value)
 *   - Index 0 is reserved as invalid (zero-initialization = invalid)
 *   - Generation increments on slot reuse
 *   - IsValid() checks both index bounds and generation match
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "Foundation/AxMath.h"

//=============================================================================
// Handle Types
//=============================================================================

/**
 * Base handle structure for all resource types.
 * Designed to be small (8 bytes) and passed by value.
 */
struct AxResourceHandle {
    uint32_t Index;      // Slot index (0 = invalid)
    uint32_t Generation; // Generation counter for stale detection
};

// Type-safe handle aliases
typedef struct AxResourceHandle AxTextureHandle;
typedef struct AxResourceHandle AxMeshHandle;
typedef struct AxResourceHandle AxShaderHandle;
typedef struct AxResourceHandle AxMaterialHandle;
typedef struct AxResourceHandle AxModelHandle;
typedef struct AxResourceHandle AxSceneHandle;

// Invalid handle constant (zero-initialized = invalid)
#define AX_INVALID_HANDLE ((struct AxResourceHandle){0, 0})

// Handle utility macros
#define AX_HANDLE_EQUALS(a, b) ((a).Index == (b).Index && (a).Generation == (b).Generation)
#define AX_HANDLE_IS_VALID(h)  ((h).Index != 0)

//=============================================================================
// Initialization Options
//=============================================================================

/**
 * Options for initializing the resource system.
 * Pass NULL to use defaults.
 */
struct AxResourceInitOptions {
    uint32_t InitialTextureCapacity;   // Initial texture slots (default: 64)
    uint32_t InitialMeshCapacity;      // Initial mesh slots (default: 64)
    uint32_t InitialShaderCapacity;    // Initial shader slots (default: 32)
    uint32_t InitialMaterialCapacity;  // Initial material slots (default: 64)
    uint32_t InitialModelCapacity;     // Initial model slots (default: 32)
    uint32_t InitialSceneCapacity;     // Initial scene slots (default: 8)
};

#define AX_RESOURCE_DEFAULT_TEXTURE_CAPACITY  64
#define AX_RESOURCE_DEFAULT_MESH_CAPACITY     64
#define AX_RESOURCE_DEFAULT_SHADER_CAPACITY   32
#define AX_RESOURCE_DEFAULT_MATERIAL_CAPACITY 64
#define AX_RESOURCE_DEFAULT_MODEL_CAPACITY    32
#define AX_RESOURCE_DEFAULT_SCENE_CAPACITY    8

//=============================================================================
// Loading Options
//=============================================================================

/**
 * Options for loading textures.
 */
struct AxTextureLoadOptions {
    bool IsSRGB;           // Interpret as sRGB color space (default: false)
    bool GenerateMipmaps;  // Generate mipmaps on load (default: true)
};

/**
 * Options for loading meshes.
 */
struct AxMeshLoadOptions {
    bool CalculateTangents;  // Calculate tangents if missing (default: true)
    bool FlipWindingOrder;   // Flip triangle winding (default: false)
    float Scale;             // Uniform scale factor (default: 1.0)
};

/**
 * Options for loading shaders.
 */
struct AxShaderLoadOptions {
    const char* Defines;      // Preprocessor defines, semicolon-separated
    const char* IncludePath;  // Include search path
};

//=============================================================================
// Resource Information
//=============================================================================

/**
 * Information about a loaded texture.
 */
struct AxTextureInfo {
    uint32_t Width;
    uint32_t Height;
    uint32_t Channels;
    uint32_t MipLevels;
    uint32_t GPUHandle;
    bool HasAlpha;
};

/**
 * Information about a loaded mesh.
 */
struct AxMeshInfo {
    uint32_t VertexCount;
    uint32_t IndexCount;
    uint32_t GPUVertexBuffer;
    uint32_t GPUIndexBuffer;
    uint32_t GPUVertexArray;
};

/**
 * Information about a loaded shader.
 */
struct AxShaderInfo {
    uint32_t ProgramID;
    const char* VertexPath;
    const char* FragmentPath;
};

/**
 * Aggregate statistics about the resource system.
 */
struct AxResourceStats {
    uint32_t TexturesLoaded;
    uint32_t MeshesLoaded;
    uint32_t ShadersLoaded;
    uint32_t MaterialsLoaded;
    uint32_t ModelsLoaded;
    uint32_t ScenesLoaded;
    uint32_t PendingDeletions;
    size_t TotalMemoryUsed;
};

//=============================================================================
// Model Data (Handle-based)
//=============================================================================

/** Maximum number of meshes/textures/materials/transforms per model */
#define AX_MODEL_MAX_MESHES     512
#define AX_MODEL_MAX_TEXTURES   256
#define AX_MODEL_MAX_MATERIALS  128
#define AX_MODEL_MAX_TRANSFORMS 512

/**
 * Handle-based model data for use with ResourceAPI.
 * Contains handles to all loaded resources that make up a model.
 * Use ReleaseModel() to release all resources when done.
 */
struct AxModelData {
    char Name[64];                              // Model name (from filename)
    char BasePath[256];                         // Base path for resolving relative texture paths

    // Mesh handles
    AxMeshHandle Meshes[AX_MODEL_MAX_MESHES];   // Handles to loaded meshes
    uint32_t MeshCount;                         // Number of meshes loaded

    // Texture handles
    AxTextureHandle Textures[AX_MODEL_MAX_TEXTURES]; // Handles to loaded textures
    uint32_t TextureCount;                      // Number of textures loaded

    // Material handles
    AxMaterialHandle Materials[AX_MODEL_MAX_MATERIALS]; // Handles to created materials
    uint32_t MaterialCount;                     // Number of materials created

    // Mesh-to-material mapping (MeshMaterials[i] is material index for Meshes[i])
    int32_t MeshMaterials[AX_MODEL_MAX_MESHES]; // Material index for each mesh (-1 = no material)

    // Per-mesh transform indices (MeshTransformIndices[i] is transform index for Meshes[i])
    uint32_t MeshTransformIndices[AX_MODEL_MAX_MESHES]; // Transform index for each mesh

    // Node transforms (computed from GLTF node hierarchy)
    AxMat4x4 Transforms[AX_MODEL_MAX_TRANSFORMS]; // Pre-computed world transforms for nodes
    uint32_t TransformCount;                      // Number of transforms
};

#ifdef __cplusplus
}
#endif
