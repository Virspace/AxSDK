#pragma once

#include "Foundation/AxTypes.h"

typedef struct AxWindow AxWindow;
typedef struct AxMesh AxMesh;
typedef struct AxTexture AxTexture;
typedef struct AxMaterial AxMaterial;
typedef struct AxDrawData AxDrawData;

struct AxRenderCommands
{
    int32_t WindowWidth;
    int32_t WindowHeight;
    AxRect DrawRegion;
};

typedef struct AxOpenGLInfo
{
    bool ModernContext;

    char *Vendor;
    char *Renderer;
    char *GLVersion;
    char *GLSLVersion;
} AxOpenGLData;

#define AXON_OPENGL_API_NAME "AxonOpenGLAPI"

typedef uint32_t AxDrawIndex;

// Alpha blending constants
#ifndef GL_SRC_ALPHA
#define GL_SRC_ALPHA 0x0302
#endif
#ifndef GL_ONE_MINUS_SRC_ALPHA
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#endif
#ifndef GL_ONE
#define GL_ONE 1
#endif
#ifndef GL_ZERO
#define GL_ZERO 0
#endif

struct AxVertex
{
    AxVec3 Position;
    AxVec3 Normal;
    AxVec2 TexCoord;
    AxVec4 Tangent; // XYZ = tangent vector, W = handedness (-1.0 or 1.0)
};

struct AxShaderData
{
    uint32_t ShaderHandle;
    uint32_t IMGUIShaderHandle;
    int32_t AttribLocationTexture;
    int32_t AttribLocationNormalTexture;
    int32_t AttribLocationVertexPos;
    int32_t AttribLocationVertexUV;
    int32_t AttribLocationVertexColor;
    int32_t AttribLocationVertexTangent;
    int32_t AttribLocationVertexBitangent;
    int32_t AttribLocationModelMatrix;
    int32_t AttribLocationViewMatrix;
    int32_t AttribLocationProjectionMatrix;
    int32_t AttribLocationColor;
    int32_t AttribLocationMaterialAlpha;
    int32_t AttribLocationLightPos;
    int32_t AttribLocationLightColor;
    int32_t AttribLocationViewPos;
    int32_t AttribLocationHasNormalMap;
    bool SupportsSRGBFramebuffer;
};

struct AxCamera
{
    bool IsOrthographic;        // Orthographic or projection
    float FieldOfView;          // Field of view for perspective projection
    float ZoomLevel;            // Zoom level for orthographic projection
    float AspectRatio;          // Aspect ratio for the camera
    float NearClipPlane;        // Near clipping plane
    float FarClipPlane;         // Far clipping plane
    AxMat4x4 ViewMatrix;        // View matrix
    AxMat4x4 ProjectionMatrix;  // Projection matrix
};

// TODO(mdeforge): Add texture parameters such as format and filtering modes
struct AxTexture
{
    uint32_t ID;
    uint32_t Width;
    uint32_t Height;
    uint32_t Channels;

    // Sampler properties
    uint32_t WrapS;          // GL_REPEAT, GL_CLAMP_TO_EDGE, GL_MIRRORED_REPEAT
    uint32_t WrapT;          // GL_REPEAT, GL_CLAMP_TO_EDGE, GL_MIRRORED_REPEAT
    uint32_t MagFilter;      // GL_NEAREST, GL_LINEAR
    uint32_t MinFilter;      // GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, etc.
};

struct AxMeshCreateInfo
{
    struct AxVertex *Vertices;
    uint32_t *Indices;
    uint32_t TransformIndex;
    uint32_t BaseColorTexture;
    uint32_t NormalTexture;
    size_t VertexOffset;
    size_t IndexOffset;
    uint32_t VertexBuffer;
    uint32_t IndexBuffer;
};

struct AxMesh
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
};

struct AxMaterial
{
    float BaseColorFactor[4]; // RGBA
    uint32_t BaseColorTexture;
    uint32_t NormalTexture;
    char Name[64];
};

struct AxModel
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
};

struct AxViewport
{
    AxVec2 Position;
    AxVec2 Size;
    AxVec2 Depth;
    AxVec2 Scale;
    bool IsActive;
    AxVec4 ClearColor;
};

struct AxDrawData
{
    bool Valid;
    size_t TotalIndexCount;
    size_t TotalVertexCount;
    //struct AxDrawList **DrawLists;
};

struct AxOpenGLAPI
{
    // Initializes the OpenGL rendering backend, creates an OpenGL context
    // associated with the given window, and loads necessary extensions.
    void (*CreateContext)(AxWindow *Window);

    // Destroys the OpenGL rendering backend
    void (*DestroyContext)(void);

    // Get information about the current OpenGL Context
    struct AxOpenGLInfo (*GetInfo)(bool ModernContext);

    void (*CreateCamera)(struct AxCamera *Camera);
    bool (*CameraGetOrthographic)(struct AxCamera *Camera);
    void (*CameraSetOrthographic)(struct AxCamera *Camera, bool IsOrthographic);
    float (*CameraGetFOV)(struct AxCamera *Camera);
    void (*CameraSetFOV)(struct AxCamera *Camera, float FOV);
    float (*CameraGetNearClipPlane)(struct AxCamera *Camera);
    void (*CameraSetNearClipPlane)(struct AxCamera *Camera, float NearClipPlane);
    float (*CameraGetFarClipPlane)(struct AxCamera *Camera);
    void (*CameraSetFarClipPlane)(struct AxCamera *Camera, float FarClipPlane);
    float (*CameraGetZoomLevel)(struct AxCamera *Camera);
    void (*CameraSetZoomLevel)(struct AxCamera *Camera, float ZoomLevel);
    float (*CameraGetAspectRatio)(struct AxCamera *Camera);
    void (*CameraSetAspectRatio)(struct AxCamera *Camera, float AspectRatio);
    struct AxMat4x4 (*CameraGetProjectionMatrix)(struct AxCamera *Camera);

    // Get the current viewport
    struct AxViewport* (*CreateViewport)(AxVec2 Position, AxVec2 Size);
    void (*DestroyViewport)(struct AxViewport *Viewport);
    void (*SetActiveViewport)(struct AxViewport *Viewport);

    // Updates the viewport and clears the buffer
    void (*NewFrame)(void);

    // Render DrawData
    void (*Render)(struct AxViewport *Viewport, struct AxMesh *Mesh, struct AxShaderData *ShaderData);

    // Exchanges the front and back buffers in the current pixel format for the window referenced
    void (*SwapBuffers)(void);

    // Create a shader program
    uint32_t (*CreateProgram)(const char* HeaderCode, const char* VertexCode, const char *FragmentCode);
    void (*DestroyProgram)(uint32_t ProgramID);
    bool (*GetAttributeLocations)(const uint32_t ProgramID, struct AxShaderData *Data);
    void (*SetUniform)(struct AxShaderData *ShaderData, const char *Name, const void *Value);

    void (*InitTexture)(AxTexture *Texture, uint8_t *Pixels);
    void (*DestroyTexture)(AxTexture *Texture);
    void (*SetTextureData)(AxTexture *Texture, uint8_t *Pixels);
    void (*BindTexture)(AxTexture *Texture, uint32_t Slot);

    void (*InitMesh)(AxMesh *Mesh, struct AxVertex *Vertices, uint32_t *Indices, uint32_t VertexCount, uint32_t IndexCount);
    //void (*DestroyMesh)(AxMesh *Mesh);

    // Alpha blending control
    void (*EnableAlphaBlending)(bool Enable);
    void (*SetAlphaBlendMode)(uint32_t SourceFactor, uint32_t DestFactor);
    bool (*TextureHasAlpha)(AxTexture *Texture);
    void (*SetDepthWrite)(bool Enable);
};

// struct AxShaderData;

// struct AxShaderAPI
// {

// };