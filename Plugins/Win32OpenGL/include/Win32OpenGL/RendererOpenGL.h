#pragma once

#include "Foundation/Types.h"

// TODO(mdeforge): Create an agnostic renderer interface, render.h and move
//                 non-OpenGL stuff there.

typedef struct AxWindow AxWindow;
typedef struct AxMesh AxMesh;

struct OpenGL;

typedef struct AxMesh
{
    uint32_t VertexBuffer;
    uint32_t ElementBuffer;
    uint32_t VertexArray;
    uint32_t Texture;
    AxTransform Transform; // TODO(mdeforge): Want to eventually keep this separate
} AxMesh;

struct AxRenderCommands
{
    int32_t WindowWidth;
    int32_t WindowHeight;
    AxRect DrawRegion;
};

struct OpenGLInfo
{
    bool ModernContext;

    char *Vendor;
    char *Renderer;
    char *Version;
    char *ShadingLanguageVersion;
};

struct OpenGLProgramCommon
{
    uint32_t ProgramHandle;

    uint32_t VertexPID;
    uint32_t VertexNID;
    uint32_t VertexUVID;
    uint32_t VertexColorID;
    uint32_t VertexLightIndex;
    uint32_t VertexTextureIndex;

    uint32_t SamplerCount;
    uint32_t Samples[16];
};

struct ShaderToy
{
    uint32_t ProgramHandle;
    uint32_t iMouse;
    uint32_t iResolution;           // viewport resolution (in pixels)
    uint32_t iTime;                 // shader playback time (in seconds)
};

struct OpenGL
{
    AxMesh Mesh;
    //struct ZBiasShaderProgram ZBiasProgram; // TODO(mdeforge): Big hack for now
    struct ShaderToy ShaderProgram; // TODO(mdeforge): Big hack for now
    struct AxRenderCommands RenderCommands;
    bool SupportsSRGBFramebuffer;
};

#define AXON_OPENGL_API_NAME "AxonOpenGLAPI"

struct AxOpenGLAPI
{
    struct OpenGL *(*Create)(void);
    void (*Init)(struct OpenGL *OpenGL, AxWindow *Window);
    void (*Destroy)(struct OpenGL *OpenGL);
    struct OpenGLInfo (*GetInfo)(bool ModernContext);
    struct AxRenderCommands *(*BeginFrame)(struct OpenGL *OpenGL, uint32_t WindowWidth, uint32_t WindowHeight, AxRect DrawRegion);
    void (*EndFrame)(struct OpenGL *OpenGL, struct AxRenderCommands *Commands);
    //AxMesh *(CreateMesh)(void);
};