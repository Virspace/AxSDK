#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(COMPILER_MSVC)
#define COMPILER_MSVC 0
#endif

#if !defined(COMPILER_LLVM)
#define COMPILER_LLVM 0
#endif

#if !COMPILER_MVC && !COMPILER_LLVM
#if _MSC_VER
#undef COMPILER_MSVC
#define COMPILER_MSVC 1
#else
// TODO(mdeforge): Add more compilers here
#undef COMPILER_LLVM
#define COMPILER_LLVM 1
#endif
#endif

#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define AXON_DLL_EXPORT __declspec(dllexport)
#define AXON_DLL_IMPORT __declspec(dllimport)

#ifdef AXENGINE_EXPORTS
  #define AXENGINE_API AXON_DLL_EXPORT
#else
  #define AXENGINE_API AXON_DLL_IMPORT
#endif

#define AX_PI 3.14159265359f

//#if AXON_SLOW
#if defined(__clang__)
#define AXON_ASSERT(Expression) if(!(Expression)) { __builtin_trap(); }
#else
#define AXON_ASSERT(Expression) if(!(Expression)) { *(volatile int *)0 = 0; }
#endif

#define AXON_UNUSED(Variable) ((void)(Variable)) // Used to silence "unused variable warnings"

#define SAFE_FREE(a) if( (a) != NULL ) free(a); (a) = NULL;

#define Kilobytes(Value) ((Value) * 1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

// TODO(mdeforge): Move to utilities
#define FOURCC(String) (((u32)(String[0]) << 0) | ((u32)(String[1]) << 8) | ((u32)(String[2]) << 16) | ((u32)(String[3]) << 24))

// Forward declarations
typedef struct AxAllocator AxAllocator;

// Opaque renderer-specific types (concrete definitions provided by renderer plugins)
typedef struct AxTexture AxTexture;
typedef struct AxMesh AxMesh;
typedef struct AxShaderData AxShaderData;
typedef struct AxViewport AxViewport;
typedef struct AxMaterial AxMaterial;

/**
 * Token Types for lexical analysis
 */
typedef enum AxTokenType {
    AX_TOKEN_INVALID = 0,
    AX_TOKEN_EOF,
    AX_TOKEN_COMMENT,
    AX_TOKEN_IDENTIFIER,
    AX_TOKEN_STRING,
    AX_TOKEN_NUMBER,
    AX_TOKEN_LBRACE,        // {
    AX_TOKEN_RBRACE,        // }
    AX_TOKEN_COLON,         // :
    AX_TOKEN_COMMA,         // ,
    AX_TOKEN_NEWLINE
} AxTokenType;

/**
 * Token structure for lexical analysis
 */
typedef struct AxToken {
    AxTokenType Type;
    const char* Start;
    size_t Length;
    int Line;
    int Column;
} AxToken;

typedef struct AxRect
{
    float Left;
    float Top;
    float Right;
    float Bottom;
} AxRect;

typedef union AxVec2
{
    struct
    {
        float X, Y;
    };

    struct
    {
        float U, V;
    };

    struct
    {
        float Width, Height;
    };

    struct
    {
        float Min, Max;
    };

    float XY[2];
} AxVec2;

typedef union AxVec3
{
    struct
    {
        float X, Y, Z;
    };

    struct
    {
        float U, V, W;
    };

    struct
    {
        float R, G, B;
    };

    float XYZ[3];
} AxVec3;

typedef union AxVec4
{
  struct
  {
    union
    {
        AxVec3 XYZ;
        struct
        {
            float X, Y, Z;
        };
    };

    float W;
  };

  struct
  {
    union
    {
        float R, G, B, A;
    };

    float RGBA[4];
  };

  float E[4];
} AxVec4;

typedef union AxQuat
{
    struct
    {
        float X, Y, Z, W;
    };

    float XYZW[4];
} AxQuat;

// A column major 4x4 matrix
typedef struct AxMat4x4
{
    float E[4][4]; // E[column][row]
} AxMat4x4;


// Transform representation using Translation, Rotation, Scale
typedef struct AxTransform
{
    // TRS Components
    AxVec3 Translation;
    AxQuat Rotation;
    AxVec3 Scale;
    AxVec3 Up;

    // Hierarchy
    struct AxTransform *Parent;  // Parent transform (NULL for root)

    // Lazy computation cache
    AxMat4x4 CachedForwardMatrix;
    AxMat4x4 CachedInverseMatrix;
    uint64_t LastComputedFrame;      // Frame number when matrices were computed
    bool ForwardMatrixDirty;         // True if forward matrix needs recomputation
    bool InverseMatrixDirty;         // True if inverse matrix needs recomputation
    bool IsIdentity;                 // Optimization flag for identity transforms
} AxTransform;

// Total number of ticks that have occurred since the OS started
typedef struct AxWallClock
{
    int64_t Ticks;
} AxWallClock;

// Represents a time from the system clock.
typedef struct AxClock
{
    int64_t TimeStamp;
} AxClock;

typedef enum AxTupleType { AX_TUPLE_INT, AX_TUPLE_FLOAT, AX_TUPLE_STRING } AxTupleType;

typedef struct AxTupleItem
{
    AxTupleType Type;
    union
    {
        int ValInt;
        float ValFloat;
        char *ValString;
    };
} AxTupleItem;

// TODO(mdeforge): Consider moving this outside of Types.h so that we can use an AxArray to back this
typedef struct AxTuple
{
    size_t NumItems;
    AxTupleItem *Items;
} AxTuple;

// Scene //////////////////////////////////////////////////////

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
    AxVec3 Position;                    // Position in world space (for point/spot lights)
    AxVec3 Direction;                   // Direction vector (for directional/spot lights)
    AxVec3 Color;                       // Light color (RGB)
    float Intensity;                    // Light intensity multiplier
    float Range;                        // Range for point/spot lights (0 = infinite)
    float InnerConeAngle;               // Inner cone angle for spot lights (radians)
    float OuterConeAngle;               // Outer cone angle for spot lights (radians)
} AxLight;

typedef struct AxCamera
{
    AxTransform Transform;      // Camera transform (position, rotation, scale)
    bool IsOrthographic;        // Orthographic or projection
    float FieldOfView;          // Field of view for perspective projection
    float ZoomLevel;            // Zoom level for orthographic projection
    float AspectRatio;          // Aspect ratio for the camera
    float NearClipPlane;        // Near clipping plane
    float FarClipPlane;         // Far clipping plane
    AxMat4x4 ViewMatrix;        // View matrix
    AxMat4x4 ProjectionMatrix;  // Projection matrix
} AxCamera;

/**
 * Scene Object represents a single object in a scene hierarchy.
 * Objects can have transforms, asset references, and parent-child relationships.
 */
typedef struct AxSceneObject {
    char Name[64];                      // Object identifier
    AxTransform Transform;              // Local transform (relative to parent)
    char MeshPath[256];                 // Path to mesh asset (empty if no mesh)
    char MaterialPath[256];             // Path to material asset (empty if no material)

    // Hierarchy relationships
    struct AxSceneObject* Parent;       // Parent object (NULL for root objects)
    struct AxSceneObject* FirstChild;   // First child in hierarchy (NULL if no children)
    struct AxSceneObject* NextSibling;  // Next sibling at same level (NULL if last)

    uint32_t ObjectID;                  // Unique object identifier within scene

    // Runtime loaded resource handle (Index=0 means not loaded)
    uint32_t LoadedModelIndex;          // Resource handle index
    uint32_t LoadedModelGeneration;     // Resource handle generation
} AxSceneObject;

/**
 * Scene container holds a hierarchy of scene objects and manages their memory.
 * Uses a unified allocator interface for efficient memory management.
 */

typedef struct AxScene {
    char Name[64];                      // Scene identifier
    AxSceneObject* RootObject;          // Root of scene hierarchy (NULL for empty scenes)
    AxMaterial* Materials;              // Dynamic array of materials defined in scene
    AxLight* Lights;                    // Dynamic array of lights defined in scene
    AxCamera* Cameras;                  // Dynamic array of cameras defined in scene
    AxTransform* CameraTransforms;      // Dynamic array of camera transforms (parallel to Cameras)
    uint32_t ObjectCount;               // Total number of objects in scene
    uint32_t MaterialCount;             // Total number of materials in scene
    uint32_t LightCount;                // Total number of lights in scene
    uint32_t CameraCount;               // Total number of cameras in scene
    uint32_t NextObjectID;              // Next available object ID

    // Memory management
    struct AxAllocator* Allocator;      // Scene memory allocator (unified interface)
} AxScene;

///////////////////////////////////////////////////////////////
// Rendering //////////////////////////////////////////////////
///////////////////////////////////////////////////////////////

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

// AxTexture is opaque - concrete definition provided by renderer plugins

typedef struct AxVertex
{
    AxVec3 Position;
    AxVec3 Normal;
    AxVec2 TexCoord;
    AxVec4 Tangent; // XYZ = tangent vector, W = handedness (-1.0 or 1.0)
} AxVertex;

// AxMesh is opaque - concrete definition provided by renderer plugins

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

// AxMaterial is opaque - concrete definition provided by renderer plugins

// AxShaderData is opaque - concrete definition provided by renderer plugins

// AxViewport is opaque - concrete definition provided by renderer plugins

#ifdef __cplusplus
}
#endif
