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
typedef struct AxLinearAllocator AxLinearAllocator;
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
} AxSceneObject;

/**
 * Scene container holds a hierarchy of scene objects and manages their memory.
 * Each scene uses a linear allocator for efficient bulk allocation/deallocation.
 */

typedef struct AxScene {
    char Name[64];                      // Scene identifier
    AxSceneObject* RootObject;          // Root of scene hierarchy (NULL for empty scenes)
    AxMaterial* Materials;              // Dynamic array of materials defined in scene
    AxLight* Lights;                    // Dynamic array of lights defined in scene
    uint32_t ObjectCount;               // Total number of objects in scene
    uint32_t MaterialCount;             // Total number of materials in scene
    uint32_t LightCount;                // Total number of lights in scene
    uint32_t NextObjectID;              // Next available object ID

    // Memory management
    struct AxLinearAllocator* Allocator; // Scene memory allocator
} AxScene;

///////////////////////////////////////////////////////////////
// Rendering //////////////////////////////////////////////////
///////////////////////////////////////////////////////////////

typedef enum AxAlphaMode {
    AX_ALPHA_MODE_OPAQUE = 0,
    AX_ALPHA_MODE_MASK = 1,
    AX_ALPHA_MODE_BLEND = 2
} AxAlphaMode;

typedef struct AxTexture
{
    uint32_t ID;
    uint32_t Width;
    uint32_t Height;
    uint32_t Channels;
    bool IsSRGB;             // true for color textures (baseColor, emissive), false for data textures (normal, metallic, etc.)

    // Sampler properties
    uint32_t WrapS;          // GL_REPEAT, GL_CLAMP_TO_EDGE, GL_MIRRORED_REPEAT
    uint32_t WrapT;          // GL_REPEAT, GL_CLAMP_TO_EDGE, GL_MIRRORED_REPEAT
    uint32_t MagFilter;      // GL_NEAREST, GL_LINEAR
    uint32_t MinFilter;      // GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, etc.
} AxTexture;

typedef struct AxVertex
{
    AxVec3 Position;
    AxVec3 Normal;
    AxVec2 TexCoord;
    AxVec4 Tangent; // XYZ = tangent vector, W = handedness (-1.0 or 1.0)
} AxVertex;

typedef struct AxMesh
{
    uint32_t VAO;
    uint32_t VBO;
    uint32_t EBO;
    uint32_t IndexCount;
    int32_t VertexOffset;
    uint32_t IndexOffset;
    // NOT OpenGL handles, just indices
    uint32_t TransformIndex;
    uint32_t BaseColorTexture;
    uint32_t NormalTexture;
    uint32_t MaterialIndex;
    // Mesh name from GLTF (for debugging)
    char Name[64];
} AxMesh;

typedef struct AxModel
{
    AxMesh *Meshes;
    AxTexture *Textures;
    AxMaterial *Materials;
    AxMat4x4 *Transforms;
    uint32_t InputLayout;
    uint32_t VertexBuffer;
    uint32_t IndexBuffer;
    uint32_t *Commands;
    uint32_t *ObjectData;
    uint32_t TransformData;
} AxModel;

// TODO(mdeforge): I wish I didn't need the AxAlphaMode enum in here
// Makes me wonder if this should be an opaque type...
typedef struct AxMaterial
{
    float BaseColorFactor[4]; // RGBA
    uint32_t BaseColorTexture;
    uint32_t NormalTexture;
    uint32_t MetallicRoughnessTexture;
    char Name[64];
    char VertexShaderPath[256];   // Path to vertex shader
    char FragmentShaderPath[256]; // Path to fragment shader

    // Alpha handling
    AxAlphaMode AlphaMode;        // Alpha rendering mode (opaque, mask, blend)
    float AlphaCutoff;            // Alpha cutoff value for mask mode (default 0.5)

    // Shader program storage (render API agnostic)
    uint32_t ShaderProgram;       // Render API handle (0 = not loaded)
    void* ShaderData;             // API-specific shader data (e.g., AxShaderData for OpenGL)
} AxMaterial;

typedef struct AxShaderData {
    uint32_t ShaderHandle;

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

typedef struct AxViewport
{
    AxVec2 Position;
    AxVec2 Size;
    AxVec2 Depth;
    AxVec2 Scale;
    bool IsActive;
    AxVec4 ClearColor;
} AxViewport;

#ifdef __cplusplus
}
#endif
