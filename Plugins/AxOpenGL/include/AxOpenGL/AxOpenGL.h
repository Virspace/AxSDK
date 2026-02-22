#pragma once

#include "Foundation/AxTypes.h"
#include "AxEngine/AxSceneTypes.h"
#include "AxEngine/AxRenderTypes.h"
#include "AxOpenGL/AxOpenGLTypes.h"

// struct AxRenderCommands
// {
//     int32_t WindowWidth;
//     int32_t WindowHeight;
//     AxRect DrawRegion;
// };

// Blend function enums
typedef enum AxBlendFunction {
    AX_BLEND_ZERO,
    AX_BLEND_ONE,
    AX_BLEND_SRC_COLOR,
    AX_BLEND_ONE_MINUS_SRC_COLOR,
    AX_BLEND_DST_COLOR,
    AX_BLEND_ONE_MINUS_DST_COLOR,
    AX_BLEND_SRC_ALPHA,
    AX_BLEND_ONE_MINUS_SRC_ALPHA,
    AX_BLEND_DST_ALPHA,
    AX_BLEND_ONE_MINUS_DST_ALPHA,
    AX_BLEND_CONSTANT_COLOR,
    AX_BLEND_ONE_MINUS_CONSTANT_COLOR,
    AX_BLEND_CONSTANT_ALPHA,
    AX_BLEND_ONE_MINUS_CONSTANT_ALPHA,
    AX_BLEND_SRC_ALPHA_SATURATE
} AxBlendFunction;

// Texture parameter enums
typedef enum AxTextureParameter {
    AX_TEXTURE_WRAP_S,
    AX_TEXTURE_WRAP_T,
    AX_TEXTURE_MAG_FILTER,
    AX_TEXTURE_MIN_FILTER
} AxTextureParameter;

typedef enum AxTextureWrapMode {
    AX_TEXTURE_WRAP_REPEAT,
    AX_TEXTURE_WRAP_CLAMP_TO_EDGE,
    AX_TEXTURE_WRAP_MIRRORED_REPEAT
} AxTextureWrapMode;

typedef enum AxTextureFilter {
    AX_TEXTURE_FILTER_NEAREST,
    AX_TEXTURE_FILTER_LINEAR,
    AX_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST,
    AX_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST,
    AX_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR,
    AX_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR
} AxTextureFilter;

typedef struct AxOpenGLInfo
{
    bool ModernContext;

    char *Vendor;
    char *Renderer;
    char *GLVersion;
    char *GLSLVersion;
} AxOpenGLData;

#define AXON_OPENGL_API_NAME "AxonOpenGLAPI"

//typedef uint32_t AxDrawIndex;

// struct AxMeshCreateInfo
// {
//     AxVertex *Vertices;
//     uint32_t *Indices;
//     uint32_t TransformIndex;
//     uint32_t BaseColorTexture;
//     uint32_t NormalTexture;
//     size_t VertexOffset;
//     size_t IndexOffset;
//     uint32_t VertexBuffer;
//     uint32_t IndexBuffer;
// };

// struct AxDrawData
// {
//     bool Valid;
//     size_t TotalIndexCount;
//     size_t TotalVertexCount;
//     //struct AxDrawList **DrawLists;
// };

struct AxOpenGLAPI
{
    // Initializes the OpenGL rendering backend, creates an OpenGL context
    // associated with the given window, and loads necessary extensions.
    void (*CreateContext)(uint64_t WindowHandle);

    // Destroys the OpenGL rendering backend
    void (*DestroyContext)(void);

    // Get information about the current OpenGL Context
    struct AxOpenGLInfo (*GetInfo)(bool ModernContext);

    void (*CreateCamera)(AxCamera *Camera);
    bool (*CameraGetOrthographic)(AxCamera *Camera);
    void (*CameraSetOrthographic)(AxCamera *Camera, bool IsOrthographic);
    float (*CameraGetFOV)(AxCamera *Camera);
    void (*CameraSetFOV)(AxCamera *Camera, float FOV);
    float (*CameraGetNearClipPlane)(AxCamera *Camera);
    void (*CameraSetNearClipPlane)(AxCamera *Camera, float NearClipPlane);
    float (*CameraGetFarClipPlane)(AxCamera *Camera);
    void (*CameraSetFarClipPlane)(AxCamera *Camera, float FarClipPlane);
    float (*CameraGetZoomLevel)(AxCamera *Camera);
    void (*CameraSetZoomLevel)(AxCamera *Camera, float ZoomLevel);
    float (*CameraGetAspectRatio)(AxCamera *Camera);
    void (*CameraSetAspectRatio)(AxCamera *Camera, float AspectRatio);
    struct AxMat4x4 (*CameraGetProjectionMatrix)(AxCamera *Camera);

    // Get the current viewport
    AxViewport* (*CreateViewport)(AxVec2 Position, AxVec2 Size);
    void (*DestroyViewport)(AxViewport *Viewport);
    void (*SetActiveViewport)(AxViewport *Viewport);

    // Updates the viewport and clears the buffer
    void (*NewFrame)(void);

    // Render DrawData
    void (*Render)(AxViewport *Viewport, struct AxMesh *Mesh, struct AxShaderData *ShaderData);

    // Exchanges the front and back buffers in the current pixel format for the window referenced
    void (*SwapBuffers)(void);

    // Create a shader program
    uint32_t (*CreateProgram)(const char* HeaderCode, const char* VertexCode, const char *FragmentCode);
    void (*DestroyProgram)(uint32_t ProgramID);
    bool (*GetAttributeLocations)(const uint32_t ProgramID, struct AxShaderData *Data);
    void (*SetUniform)(struct AxShaderData *ShaderData, const char *Name, const void *Value);

    // PBR material uniform setter - efficiently sets all PBR material + lighting uniforms
    void (*SetPBRMaterialUniforms)(struct AxShaderData* ShaderData, const AxPBRMaterial* Material,
                                    const AxLight* Lights, int32_t LightCount);

    // Scene lighting uniform setter - sets all light array uniforms from scene lights
    void (*SetSceneLights)(struct AxShaderData* ShaderData, const AxLight* Lights, int32_t LightCount);

    void (*InitTexture)(AxTexture *Texture, uint8_t *Pixels);
    void (*DestroyTexture)(AxTexture *Texture);
    void (*SetTextureData)(AxTexture *Texture, uint8_t *Pixels);
    void (*BindTexture)(AxTexture *Texture, uint32_t Slot);
    void (*SetTextureParameter)(AxTexture *Texture, AxTextureParameter Parameter, int32_t Value);
    void (*SetTextureWrapMode)(AxTexture *Texture, AxTextureWrapMode WrapS, AxTextureWrapMode WrapT);
    void (*SetTextureFilterMode)(AxTexture *Texture, AxTextureFilter MagFilter, AxTextureFilter MinFilter);

    void (*InitMesh)(AxMesh *Mesh, struct AxVertex *Vertices, uint32_t *Indices, uint32_t VertexCount, uint32_t IndexCount);
    //void (*DestroyMesh)(AxMesh *Mesh);

    // Alpha blending control
    void (*EnableBlending)(bool Enable);
    void (*SetBlendFunction)(AxBlendFunction SourceFactor, AxBlendFunction DestFactor);
    bool (*TextureHasAlpha)(AxTexture *Texture);
    void (*SetDepthWrite)(bool Enable);

    // Face culling control
    void (*SetCullMode)(bool Enable);  // Enable/disable backface culling
};