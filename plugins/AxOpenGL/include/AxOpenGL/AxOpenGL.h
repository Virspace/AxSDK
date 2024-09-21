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

struct AxDrawVert
{
    AxVert Position;
    //AxUV UV;
    uint32_t Color;
};

struct AxShaderData
{
    uint32_t ShaderHandle;
    uint32_t IMGUIShaderHandle;
    int32_t AttribLocationTexture;
    int32_t AttribLocationVertexPos;
    int32_t AttribLocationVertexUV;
    int32_t AttribLocationVertexColor;
    int32_t AttribLocationProjectMatrix;
    bool SupportsSRGBFramebuffer;
};

// TODO(mdeforge): Add texture parameters such as format and filtering modes
struct AxTexture
{
    uint32_t ID;
    uint32_t Width;
    uint32_t Height;
};

struct AxMesh
{
    struct AxDrawVert *Vertices;
    AxDrawIndex *Indices;
};

struct AxMaterial
{
    struct AxShaderData *ShaderData;
    struct AxTexture *Texture;
};

struct AxDrawable
{
    struct AxMesh *Mesh;
    struct AxMaterial *Material;
    AxMat4x4 Transform;
};

struct AxDrawCommand
{
    // Buffer offsets
    size_t VertexOffset; // Start offset in the vertex buffer
    size_t IndexOffset;  // Start offset in the index buffer
    size_t ElementCount; // Number of indices to be rendered as triangles

    // Rendering state
    AxVec4 ClipRect;
    struct AxDrawable *Drawable;
};

struct AxDrawList
{
    // Buffer objects
    uint32_t VAO;
    uint32_t VBO;
    uint32_t EBO;

    // CPU-side buffers
    struct AxDrawVert *VertexBuffer;      // Vertex Buffer
    AxDrawIndex *IndexBuffer;             // Index Buffer

    // Draw commands
    struct AxDrawCommand *CommandBuffer;  // Draw commands

    // Dirty flag
    bool IsDirty;                         // Triggers upload of the buffers to GPU
};

struct AxViewport
{
    struct AxDrawData *DrawData;
};

struct AxDrawData
{
    bool Valid;
    size_t TotalIndexCount;
    size_t TotalVertexCount;
    struct AxDrawList **DrawLists;
    AxVec2 DisplayPos;
    AxVec2 DisplaySize;
    AxVec2 FramebufferScale;
};

struct AxOpenGLAPI
{
    // Initializes the OpenGL rendering backend, creates an OpenGL context
    // associated with the given window, and loads necessary extensions.
    void (*Create)(AxWindow *Window);

    // Destroys the OpenGL rendering backend
    void (*Destroy)(void);

    // Get information about the current OpenGL Context
    struct AxOpenGLInfo (*GetInfo)(bool ModernContext);

    void (*DrawableInit)(struct AxDrawable *Drawable, struct AxMesh *Mesh, struct AxMaterial *Material, AxMat4x4 Transform);
    void (*DrawableAddShaderData)(struct AxDrawable *Drawable, struct AxShaderData *Data);
    void (*DrawableDestroy)(struct AxDrawable *Drawable);


    void (*DrawListInit)(struct AxDrawList *DrawList);
    void (*DrawListBufferData)(struct AxDrawList *DrawList, struct AxDrawVert *Vertices, AxDrawIndex *Indices, size_t NumVertices, size_t NumIndices);
    void (*DrawListAddDrawable)(struct AxDrawList *DrawList, struct AxDrawable *Drawable, const AxVec4 ClipRect);
    void (*DrawListAddCommand)(struct AxDrawList *DrawList, struct AxDrawCommand Command);
    void (*DrawListBind)(struct AxDrawList *DrawList);
    void (*DrawListUnbind)();
    void (*DrawListDestroy)(struct AxDrawList *DrawList);

    void (*DrawDataInit)(struct AxDrawData *DrawData, const AxVec2 DisplayPos, const AxVec2 DisplaySize, const AxVec2 FrameBufferScale);
    void (*DrawDataAddDrawList)(struct AxDrawData *DrawData, struct AxDrawList *DrawList);
    void (*DrawDataDestroy)(struct AxDrawData *DrawData);

    // Updates the viewport and clears the buffer
    void (*NewFrame)(void);

    // Render DrawData
    void (*Render)(AxDrawData *DrawData);

    struct AxTexture *(*CreateTexture)(const uint32_t Width, const uint32_t Height, const void *Pixels);
    //void (*TextureDestroy)(struct AxTexture *Texture);
    uint32_t (*TextureID)(const struct AxTexture *Texture);

    void (*MeshInit)(struct AxMesh *Mesh);
    void (*MeshDestroy)(struct AxMesh *Mesh);

    void (*MaterialDestroy)(struct AxMaterial *Material);

    // Create a shader program
    uint32_t (*CreateProgram)(const char* HeaderCode, const char* VertexCode, const char *FragmentCode);
    void (*DestroyProgram)(uint32_t ProgramID);
    bool (*GetAttributeLocations)(const uint32_t ProgramID, struct AxShaderData *Data);

    // Exchanges the front and back buffers in the current pixel format for the window referenced
    void (*SwapBuffers)(void);
};

// struct AxShaderData;

// struct AxShaderAPI
// {

// };