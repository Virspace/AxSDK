#pragma once

#include "Foundation/Types.h"

//typedef struct OpenGLData OpenGLData;
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

struct AxOpenGLAPI
{
    // Creates the OpenGL rendering backend, loads extensions, and 
    void (*Create)(AxWindow *Window);
    
    // Destroys the OpenGL rendering backend
    void (*Destroy)(void);

    // Get information about the current OpenGL Context
    struct AxOpenGLInfo (*GetInfo)(bool ModernContext);

    // Updates the viewport and clears the buffer
    void (*NewFrame)(void);

    // TODO(mdeforge): Consider moving to a pipeline API?
    // Add a drawable to the target draw data
    //void (*AddDrawable)(AxDrawData DrawData, const AxDrawVert *Vertices, const AxDrawIndex *Indices, const int32_t VertexCount, const int32_t IndexCount);

    // Pass this into the backend renderer. Valid after Render() until next call to NewFrame().
    struct AxDrawData *(*GetDrawData)(void);

    // 
    void (*Render)(AxDrawData *DrawData);

    //struct AxTexture *(*CreateTexture)(struct AxRenderContext *Context, int32_t Width, int32_t Height, void *Data);

    // Exchanges the front and back buffers in the current pixel format for the window referenced
    void (*SwapBuffers)(void);
};