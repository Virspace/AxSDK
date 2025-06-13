#pragma once

#include "Foundation/AxTypes.h"

typedef struct AxWindow AxWindow;
typedef struct AxMesh AxMesh;
typedef struct AxTexture AxTexture;
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

struct AxVertex
{
    AxVec3 Position;
    AxVec3 Normal;
    AxVec2 TexCoord;
};

struct AxShaderData
{
    uint32_t ShaderHandle;
    uint32_t IMGUIShaderHandle;
    int32_t AttribLocationTexture;
    int32_t AttribLocationVertexPos;
    int32_t AttribLocationVertexUV;
    int32_t AttribLocationVertexColor;
    int32_t AttribLocationModelMatrix;
    int32_t AttribLocationViewMatrix;
    int32_t AttribLocationProjectionMatrix;
    int32_t AttribLocationColor;
    int32_t AttribLocationLightPos;
    int32_t AttribLocationLightColor;
    bool SupportsSRGBFramebuffer;
};

struct AxCamera
{
    AxVec3 Position;          // Camera position in world space
    AxVec3 Orientation;       // Camera orientation (e.g., direction it is facing)
    float FieldOfView;       // Field of view for perspective projection
    float AspectRatio;       // Aspect ratio for the camera
    float NearPlane;         // Near clipping plane
    float FarPlane;          // Far clipping plane
    AxMat4x4 ViewMatrix;  // View matrix
    AxMat4x4Inv ProjectionMatrix; // Projection matrix
};

// TODO(mdeforge): Add texture parameters such as format and filtering modes
struct AxTexture
{
    uint32_t ID;
    uint32_t Width;
    uint32_t Height;
    uint32_t Channels;
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
};

struct AxModel
{
    AxMesh *Meshes;
    AxTexture *Textures;
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
};

// struct AxShaderData;

// struct AxShaderAPI
// {

// };