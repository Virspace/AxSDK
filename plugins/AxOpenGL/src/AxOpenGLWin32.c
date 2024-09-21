#include <Windows.h>
#include <stdio.h>
#include "GL/gl3w.h"

#include "AxOpenGL.h"

#include "AxWindow/AxWindow.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxMath.h"
#include "Foundation/AxPlatform.h"
#include "Foundation/AxCamera.h"

#define AXARRAY_IMPLEMENTATION
#include "Foundation/AxArray.h"

#define WGL_DRAW_TO_WINDOW_ARB                  0x2001
#define WGL_ACCELERATION_ARB                    0x2003
#define WGL_SUPPORT_OPENGL_ARB                  0x2010
#define WGL_DOUBLE_BUFFER_ARB                   0x2011
#define WGL_PIXEL_TYPE_ARB                      0x2013
#define WGL_TYPE_RGBA_ARB                       0x202B
#define WGL_FULL_ACCELERATION_ARB               0x2027
#define WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB        0x20A9
#define WGL_CONTEXT_MAJOR_VERSION_ARB           0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB           0x2092
#define WGL_CONTEXT_FLAGS_ARB                   0x2094
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB  0x0002
#define WGL_CONTEXT_PROFILE_MASK_ARB            0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB        0x00000001

typedef BOOL WINAPI wgl_swap_interval_ext(int interval);
typedef const char * WINAPI wgl_get_extensions_string_ext(void);
typedef HGLRC WINAPI wgl_create_context_attribs_arb(HDC hDC, HGLRC hShareContext, const int *attribList);
typedef BOOL WINAPI wgl_choose_pixel_format_arb(HDC hdc,
    const int *piAttribIList,
    const FLOAT *pfAttribFList,
    UINT nMaxFormats,
    int *piFormats,
    UINT *nNumFormats);

static wgl_choose_pixel_format_arb *wglChoosePixelFormatARB;
static wgl_create_context_attribs_arb *wglCreateContextAttribsARB;
static wgl_swap_interval_ext *wglSwapIntervalEXT;
static wgl_get_extensions_string_ext *wglGetExtensionsStringEXT;

int32_t Win32OpenGLAttribs[] =
{
    WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
    WGL_CONTEXT_MINOR_VERSION_ARB, 6,
    WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB
    #if AXON_static
    | WGL_CONTEXT_DEBUG_BIT_ARB
    #endif
    ,
    WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
    0
};

// API Handles
struct AxWindowAPI *WindowAPI;
struct AxPlatformAPI *PlatformAPI;

// struct OpenGLData
// {
//     GLuint ShaderHandle;
//     GLuint IMGUIShaderHandle;
//     GLuint AttribLocationTexture;
//     GLuint AttribLocationVertexPos;
//     GLuint AttribLocationVertexUV;
//     GLuint AttribLocationVertexColor;
//     GLuint AttribLocationProjectMatrix;
//     bool SupportsSRGBFramebuffer;
// };

// Global data
struct OpenGLData *Data;
struct AxViewport *Viewport;

// TODO(mdeforge): Move to Foundation
inline bool IsEndOfLine(char C)
{
    bool Result = ((C == '\n') ||
                  (C == '\r'));

    return(Result);
}

// TODO(mdeforge): Move to Foundation
inline bool IsWhitespace(char C)
{
    bool Result = ((C == ' ') ||
                  (C == '\t') ||
                  (C == '\v') ||
                  (C == '\f') ||
                  IsEndOfLine(C));

    return(Result);
}

static bool CheckShader(GLuint Handle, const char *Description)
{
    GLint Status = 0, LogLength = 0;
    glGetShaderiv(Handle, GL_COMPILE_STATUS, &Status);
    glGetShaderiv(Handle, GL_INFO_LOG_LENGTH, &LogLength);
    if ((GLboolean)Status == GL_FALSE) {
        fprintf(stderr, "ERROR: CheckShader failed to compile %s!\n", Description);
    }

    if (LogLength > 1)
    {
        char *Buffer = malloc(LogLength + 1);
        glGetShaderInfoLog(Handle, LogLength, NULL, (GLchar *)Buffer);
        fprintf(stderr, "%s\n", Buffer);
        if (Buffer) {
            free(Buffer);
        }
    }

    return ((GLboolean)Status == GL_TRUE);
}

static void SetUniform(struct AxShaderData *ShaderData, const char *UniformName, const void *Value)
{
    if (ShaderData->AttribLocationProjectMatrix != -1) {
        glProgramUniformMatrix4fv(ShaderData->ShaderHandle, ShaderData->AttribLocationProjectMatrix, 1, GL_FALSE, (const GLfloat *)Value);
    }

    uint32_t Result = glGetError();
    AXON_ASSERT(!Result && "OpenGL Error in CreateProgram()!");
}

static bool CheckProgram(GLuint Handle, const char *Description)
{
    GLint Status = 0, LogLength = 0;
    glGetProgramiv(Handle, GL_LINK_STATUS, &Status);
    glGetProgramiv(Handle, GL_INFO_LOG_LENGTH, &LogLength);
    if ((GLboolean)Status == GL_FALSE) {
        fprintf(stderr, "ERROR: CheckProgram failed to link %s!\n", Description);
    }

    if (LogLength > 1)
    {
        char *Buffer = malloc(LogLength + 1);
        glGetProgramInfoLog(Handle, LogLength, NULL, (GLchar *)Buffer);
        fprintf(stderr, "%s\n", Buffer);
        if (Buffer) {
            free(Buffer);
        }
    }

    return ((GLboolean)Status == GL_TRUE);
}

static uint32_t CreateProgram(const char *HeaderCode, const char *VertexCode, const char *FragmentCode)
{
    AXON_ASSERT(HeaderCode && "HeaderCode is NULL in CreateProgram()!");
    AXON_ASSERT(VertexCode && "VertexCode is NULL in CreatePrgoram()!");
    AXON_ASSERT(FragmentCode && "FragmentCode is NULL in CreateProgram()!");

    // Create Shaders
    GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
    const GLchar *VertexShaderCode[2] = { HeaderCode, VertexCode };
    glShaderSource(VertexShaderID, ArrayCount(VertexShaderCode), VertexShaderCode, 0);
    glCompileShader(VertexShaderID);
    if (!CheckShader(VertexShaderID, "Vertex Shader")) {
        return 0;
    }

    GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
    const GLchar *FragmentShaderCode[2] = { HeaderCode, FragmentCode };
    glShaderSource(FragmentShaderID, ArrayCount(FragmentShaderCode), FragmentShaderCode, 0);
    glCompileShader(FragmentShaderID);
    if (!CheckShader(FragmentShaderID, "Fragment Shader")) {
        return 0;
    }

    // Link
    GLuint ProgramID = glCreateProgram();
    glAttachShader(ProgramID, VertexShaderID);
    glAttachShader(ProgramID, FragmentShaderID);
    glLinkProgram(ProgramID);
    if (!CheckProgram(ProgramID, "Shader Program")) {
        return 0;
    }

    glDetachShader(ProgramID, VertexShaderID);
    glDetachShader(ProgramID, FragmentShaderID);
    glDeleteShader(VertexShaderID);
    glDeleteShader(FragmentShaderID);

    uint32_t Result = glGetError();
    AXON_ASSERT(!Result && "OpenGL Error in CreateProgram()!");

    return((uint32_t)ProgramID);
}

static void DestroyProgram(uint32_t ProgramID)
{
    AXON_ASSERT(ProgramID && "ProgramID is 0 in DestroyProgram()!");

    if (ProgramID != 0)
    {
        // Get the attached shaders
        GLint shaderCount = 0;
        GLuint shaders[5]; // TODO(mdeforge): Can we not hardcode this?
        glGetAttachedShaders(ProgramID, 5, &shaderCount, shaders);

        // Detach and delete each shader
        for (GLint i = 0; i < shaderCount; ++i)
        {
            glDetachShader(ProgramID, shaders[i]);
            glDeleteShader(shaders[i]);
        }

        // Delete the shader program
        glDeleteProgram(ProgramID);
    }
}

static void DrawableDestroy(struct AxDrawable *Drawable)
{
    AXON_ASSERT(Drawable && "Drawable is NULL in DrawableDestroy()!");


}

static bool IsValidArray(GLuint Index)
{
    bool Result = (Index != -1);
    return(Result);
}

static void Win32SetPixelFormat(/*struct OpenGLData *Data,*/ HDC WindowDC)
{
    int SuggestedPixelFormatIndex = 0;
    GLuint ExtendedPick = 0;
    if(wglChoosePixelFormatARB)
    {
        int IntAttribList[] = {
            WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
            WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
            WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
            WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
            WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
            WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, GL_TRUE,
            0,
        };

        // if(!Data->SupportsSRGBFramebuffer) {
        //     IntAttribList[10] = 0;
        // }

        wglChoosePixelFormatARB(WindowDC, IntAttribList, 0, 1, &SuggestedPixelFormatIndex, &ExtendedPick);
    }

    if (!ExtendedPick)
    {
        PIXELFORMATDESCRIPTOR DesiredPixelFormat = { 0 };
        DesiredPixelFormat.nSize = sizeof(DesiredPixelFormat);
        DesiredPixelFormat.nVersion = 1;
        DesiredPixelFormat.iPixelType = PFD_TYPE_RGBA;
        DesiredPixelFormat.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
        DesiredPixelFormat.cColorBits = 24;
        DesiredPixelFormat.cAlphaBits = 8;
        DesiredPixelFormat.cDepthBits = 24;
        DesiredPixelFormat.iLayerType = PFD_MAIN_PLANE;

        SuggestedPixelFormatIndex = ChoosePixelFormat(WindowDC, &DesiredPixelFormat);
    }

    PIXELFORMATDESCRIPTOR SuggestedPixelFormat;
    DescribePixelFormat(WindowDC, SuggestedPixelFormatIndex, sizeof(SuggestedPixelFormat), &SuggestedPixelFormat);
    SetPixelFormat(WindowDC, SuggestedPixelFormatIndex, &SuggestedPixelFormat);
}

static void SetupRenderState(struct AxDrawData *DrawData, int FramebufferWidth, int FramebufferHeight)
{
    // Enable alpha blending and scissor test, disable face culling and depth test
	// TODO(mdeforge): This is currently causing an issue but probably because I have to put back in textures
    // glEnable(GL_BLEND);
    // glBlendEquation(GL_FUNC_ADD);
    // glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    uint32_t Result = glGetError();
    AXON_ASSERT(!Result && "OpenGL Error in CreateProgram()!");

    //glDisable(GL_CULL_FACE);
    //glDisable(GL_DEPTH_TEST);
    //glDisable(GL_STENCIL_TEST);
    //glEnable(GL_SCISSOR_TEST);
    //glDisable(GL_PRIMITIVE_RESTART);
    //glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    Result = glGetError();
    AXON_ASSERT(!Result && "OpenGL Error in CreateProgram()!");
    // float AspectRatio = (float)WindowWidth / (float)WindowHeight;
    // AxMat4x4Inv ProjectionMatrix = PerspectiveProjection(AspectRatio, 0.1f, 0.50f, 100.0f);

    // AxVec3 ViewDirection = { 0.0f, 0.0f, -1.0f };
    // AxVec3 Up = { 0.0f, 1.0f, 0.0f };
    // AxVec3 Position = { 0.0, 0.0f, -1.0f };
    // AxMat4x4 Transform = Identity();
    // Transform = Translate(Transform, OpenGL->Mesh.Transform.Position);

    // AxMat4x4 ViewMatrix = LookAt(Position, Vec3Add(Position, ViewDirection), Up);
    // AxMat4x4 ModelMatrix = Transform;
    // AxMat4x4 ModelView = Mat4x4Mul(ViewMatrix, ModelMatrix);
    // AxMat4x4 MVP = Mat4x4Mul(ProjectionMatrix.Forward, ModelView);
    //glProgramUniformMatrix4fv(Program->ProgramHandle, FullTransformMatrixLocation, 1, GL_FALSE, &MVP.E[0][0]);
}

static void Win32LoadWGLExtensions(/*struct OpenGLData *Data*/)
{
    // Before we can load extensions we need a dummy OpenGL context which is
    // created used a dummy window. We do this because you can only set a
    // pixel format for a window once. For the real window, we want to use
    // wglChoosePixelFormatARB (so we can potentially specify options that
    // aren't available in PIXELFORMATDESCRIPTOR), but we can't load and use
    // that before we have a context.

    WNDCLASSA WindowClass = {
        .style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
        .lpfnWndProc = DefWindowProcA,
        .hInstance = GetModuleHandle(0),
        .lpszClassName = "AxonWGLLoader"
    };

    if(!RegisterClassA(&WindowClass)) {
        return;
    }

    HWND DummyWindow = CreateWindowExA(
        0,
        WindowClass.lpszClassName,
        "AxonDummyWindow",
        0,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        0,
        0,
        WindowClass.hInstance,
        0);

    if (!DummyWindow) {
        return;
    }

    HDC DummyWindowDC = GetDC(DummyWindow);

    Win32SetPixelFormat(/*OpenGLData,*/ DummyWindowDC);

    HGLRC DummyContext = wglCreateContext(DummyWindowDC);

    if(wglMakeCurrent(DummyWindowDC, DummyContext))
    {
        wglChoosePixelFormatARB = (wgl_choose_pixel_format_arb *)wglGetProcAddress("wglChoosePixelFormatARB");
        wglCreateContextAttribsARB = (wgl_create_context_attribs_arb *)wglGetProcAddress("wglCreateContextAttribsARB");
        wglGetExtensionsStringEXT = (wgl_get_extensions_string_ext *)wglGetProcAddress("wglGetExtensionStringEXT");
        wglSwapIntervalEXT = (wgl_swap_interval_ext *)wglGetProcAddress("wglSwapIntervalEXT");

        wglMakeCurrent(DummyWindowDC, 0);
    }

    wglDeleteContext(DummyContext);
    ReleaseDC(DummyWindow, DummyWindowDC);
    DestroyWindow(DummyWindow);
}

void Create(struct AxWindow *Window)
{
    AXON_ASSERT(Window && "Window is NULL in Create()!");

    // Trampoline the OpenGL 1.0 context to 4.6
    Win32LoadWGLExtensions(Data);

    // Get device context for Window
    AxWin32WindowData Win32WindowData = WindowAPI->GetPlatformData(Window).Win32;
    HDC DeviceContext = GetDC((HWND)Win32WindowData.Handle);

    // Set pixel format for the device
    Win32SetPixelFormat(/*Data,*/ DeviceContext);

    // Create modern OpenGL context
    bool ModernContext = true;
    HGLRC RenderContext = 0;
    if (wglCreateContextAttribsARB)  {
        RenderContext = wglCreateContextAttribsARB(DeviceContext, 0, Win32OpenGLAttribs);
    }

    if (!RenderContext)
    {
        ModernContext = false;
        RenderContext = wglCreateContext((HDC)DeviceContext);
    }

    if (wglMakeCurrent((HDC)DeviceContext, RenderContext)) {
        wglSwapIntervalEXT(0); // Disable vsync
    }

    // Load OpenGL Extensions
    gl3wInit();

    // Create default viewport
    Viewport = calloc(1, sizeof(struct AxViewport));
}

void DrawableInit(struct AxDrawable *Drawable, struct AxMesh *Mesh, struct AxMaterial *Material, AxMat4x4 Transform)
{
    AXON_ASSERT(Drawable && "Drawable is NULL in DrawableInit()!");
    AXON_ASSERT(Mesh && "Mesh is NULL in DrawableInit()!");
    AXON_ASSERT(Material && "Material is NULL in DrawableInit()!");

    Drawable->Mesh = Mesh;
    Drawable->Material = Material;
    Drawable->Transform = Transform;
}

static void DrawListInit(struct AxDrawList *DrawList)
{
    AXON_ASSERT(DrawList && "DrawList is NULL in DrawListInit()!");

    glGenVertexArrays(1, &DrawList->VAO);
    glGenBuffers(1, &DrawList->VBO);
    glGenBuffers(1, &DrawList->EBO);

    DrawList->IsDirty = true;
}

void DrawListBufferData(struct AxDrawList *DrawList, struct AxDrawVert *Vertices, AxDrawIndex *Indices, size_t NumVertices, size_t NumIndices)
{
    AXON_ASSERT(DrawList && "Drawable is NULL in DrawListBind()!");
    AXON_ASSERT(DrawList->VAO && "The VAO has not been generated in DrawListBind()!");

    glBindBuffer(GL_ARRAY_BUFFER, DrawList->VBO);
    glBufferData(GL_ARRAY_BUFFER, ArraySizeInBytes(Vertices), Vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, DrawList->EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, ArraySizeInBytes(Indices), Indices, GL_STATIC_DRAW);

    uint32_t Result = glGetError();
    AXON_ASSERT(!Result && "OpenGL Error in CreateProgram()!");
}

void DrawListAddDrawable(struct AxDrawList *DrawList, struct AxDrawable *Drawable, const AxVec4 ClipRect)
{
    AXON_ASSERT(DrawList && "DrawList is NULL in DrawListAddDrawable()!");
    AXON_ASSERT(Drawable && "Drawable is NULL in DrawListAddDrawable()!");

    ArrayPushArray(DrawList->VertexBuffer, Drawable->Mesh->Vertices);
    ArrayPushArray(DrawList->IndexBuffer, Drawable->Mesh->Indices);

    DrawList->IsDirty = true;
}

static void DrawListAddCommand(struct AxDrawList *DrawList, struct AxDrawCommand DrawCommand)
{
    ArrayPush(DrawList->CommandBuffer, DrawCommand);
}

void DrawListBind(struct AxDrawList *DrawList)
{
    glBindVertexArray(DrawList->VAO);
}

void DrawListUnbind()
{
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

static void DrawDataInit(struct AxDrawData *DrawData, const AxVec2 DisplayPos, const AxVec2 DisplaySize, const AxVec2 FrameBufferScale)
{
    DrawData->DisplayPos = DisplayPos;
    DrawData->DisplaySize = DisplaySize;
    DrawData->FramebufferScale = FrameBufferScale;
}

static void DrawDataDestroy(struct AxDrawData *DrawData)
{
    // Free DrawList ** here?
}

void DrawDataAddDrawList(struct AxDrawData *DrawData, struct AxDrawList *DrawList)
{
    AXON_ASSERT(DrawData && "DrawData is NULL in DrawDataAddDrawList()!");
    AXON_ASSERT(DrawList && "DrawList is NULL in DrawDataAddDrawList()!");

    DrawData->TotalVertexCount += ArraySize(DrawList->VertexBuffer);
    DrawData->TotalIndexCount += ArraySize(DrawList->IndexBuffer);

    ArrayPush(DrawData->DrawLists, DrawList); // ArrayPushArray? Why?
}

static void DrawListDestroy(struct AxDrawList *DrawList)
{
    AXON_ASSERT(DrawList && "DrawList is NULL in DrawListDestroy()!");
    if (DrawList->VBO)
    {
        glDeleteBuffers(1, &DrawList->VBO);
        DrawList->VBO = 0;
    }

    if (DrawList->EBO)
    {
        glDeleteBuffers(1, &DrawList->EBO);
        DrawList->EBO = 0;
    }
}

void DrawableAddShaderData(struct AxDrawable *Drawable, struct AxShaderData *ShaderData)
{
    AXON_ASSERT(Drawable && "Drawable is NULL in DrawableAddShaderData()!");
    AXON_ASSERT(ShaderData && "ShaderData is NULL in DrawableAddShaderData()!");

    uint32_t Result;

    // Position Attribute
    glVertexAttribPointer(ShaderData->AttribLocationVertexPos, 3, GL_FLOAT, GL_FALSE, sizeof(struct AxDrawVert), (GLvoid *)offsetof(struct AxDrawVert, Position));
    glEnableVertexAttribArray(ShaderData->AttribLocationVertexPos);
    Result = glGetError();
    AXON_ASSERT(!Result && "OpenGL Error in CreateProgram()!");

    // // UV Attribute
    // glVertexAttribPointer(ShaderData->AttribLocationVertexUV, 2, GL_FLOAT, GL_FALSE, sizeof(struct AxDrawVert), (GLvoid *)offsetof(struct AxDrawVert, UV));
    // glEnableVertexAttribArray(ShaderData->AttribLocationVertexUV);
    // Result = glGetError();
    // AXON_ASSERT(!Result && "OpenGL Error in CreateProgram()!");

    // Color Attribute
    glVertexAttribPointer(ShaderData->AttribLocationVertexColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(struct AxDrawVert), (GLvoid *)offsetof(struct AxDrawVert, Color));
    glEnableVertexAttribArray(ShaderData->AttribLocationVertexColor);

    Result = glGetError();
    AXON_ASSERT(!Result && "OpenGL Error in CreateProgram()!");

    if (wglGetExtensionsStringEXT)
    {
        char *Extensions = (char *)glGetString(GL_EXTENSIONS);
        char *At = Extensions;
        while(*At)
        {
            while(IsWhitespace(*At)) { ++At; }
            char *End = At;
            while(*End && !IsWhitespace(*End)) { ++End; }

            uintptr_t Count = End - At;

            if(strncmp(At, "WGL_EXT_framebuffer_sRGB", Count)) {
                ShaderData->SupportsSRGBFramebuffer = true;
            } else if(strncmp(At, "WGL_ARB_framebuffer_sRGB", Count)) {
                ShaderData->SupportsSRGBFramebuffer = true;
            }

            At = End;
        }
    }

    Result = glGetError();
    AXON_ASSERT(!Result && "OpenGL Error in CreateProgram()!");
}

void NewFrame(void)
{
    uint32_t Result = glGetError();
    AXON_ASSERT(!Result && "OpenGL Error in CreateProgram()!");

    glClearColor(0.42f, 0.51f, 0.54f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void RenderDrawData(AxDrawData *DrawData)
{
    // TODO(mdeforge): Avoid rendering when minimized

    //SetupRenderState(DrawData, FramebufferWidth, FramebufferHeight);
    glDisable(GL_FRAMEBUFFER_SRGB);

    // Will project scissor/clipping rectangles into framebuffer space
    // AxVec2 ClipOff = DrawData->DisplayPos;
    // AxVec2 ClipScale = DrawData->FramebufferScale;

    // Render command lists
    for (int i = 0; i < ArraySize(DrawData->DrawLists); i++)
    {
        struct AxDrawList *DrawList = DrawData->DrawLists[i];
        glBindVertexArray(DrawList->VAO);

        // If the draw list dirty, buffer data
        // if (DrawList->IsDirty)
        // {
        //     const struct AxDrawVert *VertexBuffer = DrawList->VertexBuffer;
        //     const AxDrawIndex *IndexBuffer = DrawList->IndexBuffer;
        //     fprintf(stdout, "GL Error: %d\n", glGetError());

        //     // Upload vertex and index buffers
        //     glBufferData(GL_ARRAY_BUFFER, ArraySizeInBytes(VertexBuffer), VertexBuffer, GL_STATIC_DRAW);
        //     glBufferData(GL_ELEMENT_ARRAY_BUFFER, ArraySizeInBytes(IndexBuffer), IndexBuffer, GL_STATIC_DRAW);
        //     fprintf(stdout, "GL Error: %d\n", glGetError());
        //     DrawList->IsDirty = false;
        // }

        size_t CommandBufferSize = ArraySize(DrawList->CommandBuffer);
        for (int j = 0; j < CommandBufferSize; j++)
        {
            const struct AxDrawCommand *Command = &DrawList->CommandBuffer[j];
            if (Command)
            {
                struct AxDrawable *Drawable = Command->Drawable;
                AXON_ASSERT(Drawable && "Drawable is NULL in RenderDrawData()!");

                struct AxShaderData *ShaderData = Drawable->Material->ShaderData;
                AXON_ASSERT(ShaderData && "ShaderData is NULL in RenderDrawData()!");

                glUseProgram(ShaderData->ShaderHandle);

                // Update projection matrix
                float L = (float)DrawData->DisplayPos.X;
                float R = (float)DrawData->DisplayPos.X + (float)DrawData->DisplaySize.X;
                float B = (float)DrawData->DisplayPos.Y;
                float T = (float)DrawData->DisplayPos.Y + (float)DrawData->DisplaySize.Y;
                AxMat4x4f OrthoProjection = CameraAPI->CalcOrthographicProjection(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
                OrthoProjection = Transpose(OrthoProjection);
                SetUniform(ShaderData, "ProjMtx", &OrthoProjection);

                // Project scissor/clipping rectangles into framebuffer space
                // AxVec2 ClipMin = {
                //     (Command->ClipRect.X - ClipOff.X) * ClipScale.X,
                //     (Command->ClipRect.Y - ClipOff.Y) * ClipScale.Y
                // };

                // AxVec2 ClipMax = {
                //     (Command->ClipRect.Z - ClipOff.X) * ClipScale.X,
                //     (Command->ClipRect.W - ClipOff.Y) * ClipScale.Y
                // };

                // if (ClipMax.X < ClipMin.X || ClipMax.Y < ClipMin.Y) {
                //     continue;
                // }

                // Apply scissor/clipping rectangle (Y is inverted in OpenGL)
                //glScissor((int)ClipMin.X, (int)(FramebufferHeight - ClipMax.Y), (int)(ClipMax.X - ClipMin.X), (int)(ClipMax.Y - ClipMin.Y));

                // Bind texture and draw
                //glBindTexture(GL_TEXTURE_2D, (GLuint)Command->TextureID);
                //fprintf(stdout, "GL Error: %d\n", glGetError());
                glDrawElementsBaseVertex(
                    GL_TRIANGLES,
                    (GLsizei)Command->ElementCount,
                    GL_UNSIGNED_INT,
                    0,
                    0
                );
            }
        }
    }
}

void Render(AxDrawData *DrawData)
{
    glViewport(0, 0, (GLsizei)DrawData->DisplaySize.X, (GLsizei)DrawData->DisplaySize.Y);

    RenderDrawData(DrawData);

    // Check for errors
    GLenum err;
    while((err = glGetError()) != GL_NO_ERROR) {
      printf("GL Error: %d\n", err);
    }
}

static struct AxOpenGLInfo GetInfo(bool ModernContext)
{
    struct AxOpenGLInfo Result = {0};

    Result.ModernContext = ModernContext;
    Result.Vendor = (char *)glGetString(GL_VENDOR);
    Result.Renderer = (char *)glGetString(GL_RENDERER);
    Result.GLVersion = (char *)glGetString(GL_VERSION);

    if(Result.ModernContext) {
        Result.GLSLVersion = (char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
    } else {
        Result.GLSLVersion = "(none)";
    }

    return (Result);
}

bool GetAttributeLocations(const uint32_t ProgramID, struct AxShaderData *ShaderData)
{
    // TODO(mdeforge): Turn this into a SetAttrib function where the user passes in a string
    // and value and, part of what it does it call this, and the calls the second part of the function
    // (not present here) to actually set it. The calling code is responsible for this.

    ShaderData->ShaderHandle = ProgramID;
    glUseProgram(ProgramID);

    uint32_t Result = glGetError();
    AXON_ASSERT(!Result && "OpenGL Error in CreateProgram()!");

    // ShaderData->AttribLocationTexture = glGetUniformLocation(ShaderData->ShaderHandle, "Texture");
    // if (ShaderData->AttribLocationTexture < 0) {
    //     return (false);
    // }

    // Result = glGetError();
    // AXON_ASSERT(!Result && "OpenGL Error in CreateProgram()!");

    ShaderData->AttribLocationProjectMatrix = glGetUniformLocation(ShaderData->ShaderHandle, "ProjMtx");
    if (ShaderData->AttribLocationProjectMatrix < 0) {
        return (false);
    }

    Result = glGetError();
    AXON_ASSERT(!Result && "OpenGL Error in CreateProgram()!");

    ShaderData->AttribLocationVertexPos = glGetAttribLocation(ShaderData->ShaderHandle, "Position");
    if (ShaderData->AttribLocationVertexPos < 0) {
        return (false);
    }

    Result = glGetError();
    AXON_ASSERT(!Result && "OpenGL Error in CreateProgram()!");

    // ShaderData->AttribLocationVertexUV = glGetAttribLocation(ShaderData->ShaderHandle, "UV");
    // if (ShaderData->AttribLocationVertexUV < 0) {
    //     return (false);
    // }

    // Result = glGetError();
    // AXON_ASSERT(!Result && "OpenGL Error in CreateProgram()!");

    ShaderData->AttribLocationVertexColor = glGetAttribLocation(ShaderData->ShaderHandle, "Color");
    if (ShaderData->AttribLocationVertexColor < 0) {
        return (false);
    }

    Result = glGetError();
    AXON_ASSERT(!Result && "OpenGL Error in CreateProgram()!");

    return true;
}

void AxSwapBuffers(void)
{
    SwapBuffers(wglGetCurrentDC());
}

static AxTexture *CreateTexture(const uint32_t Width, const uint32_t Height, const void *Pixels)
{
    AXON_ASSERT(Pixels && "Pixels is NULL in CreateTexture()!");

    AxTexture *Texture = calloc(1, sizeof(AxTexture));
    if (!Texture) {
        return (NULL);
    }

    // GLint LastTexture;
    // glGetIntegerv(GL_TEXTURE_BINDING_2D, &LastTexture);

    glGenTextures(1, &Texture->ID);
    glBindTexture(GL_TEXTURE_2D, Texture->ID);

    // Setup filtering parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same

    // Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Width, Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, Pixels);
    //glGenerateMipmap(GL_TEXTURE_2D);

    // Restore state
    //glBindTexture(GL_TEXTURE_2D, LastTexture);

    return (Texture);
}

static uint32_t TextureID(const struct AxTexture *Texture)
{
    AXON_ASSERT(Texture && "Texture is NULL in TextureID()!");

    return ((Texture) ? Texture->ID : 0);
}

static void MeshInit(struct AxMesh *Mesh)
{
    AXON_ASSERT(Mesh && "Mesh is NULL in MeshInit()!");
}

static void MeshDestroy(struct AxMesh *Mesh)
{
    AXON_ASSERT(Mesh && "Mesh is NULL in MeshDestroy()!");

    ArrayFree(Mesh->Vertices);
    ArrayFree(Mesh->Indices);
}

static void MaterialDestroy(struct AxMaterial *Material)
{
    if (Material->ShaderData->ShaderHandle)
    {
        glDeleteProgram(Material->ShaderData->ShaderHandle);
        Material->ShaderData->ShaderHandle = 0;
    }
}

static void Destroy(void)
{

}

// Use a compound literal to construct an unnamed object of API type in-place
struct AxOpenGLAPI *AxOpenGLAPI = &(struct AxOpenGLAPI) {
    .Create = Create,
    .Destroy = Destroy,
    .GetInfo = GetInfo,
    .DrawableInit = DrawableInit,
    .DrawableAddShaderData = DrawableAddShaderData,
    .DrawableDestroy = DrawableDestroy,
    .DrawListInit = DrawListInit,
    .DrawListBufferData = DrawListBufferData,
    .DrawListAddDrawable = DrawListAddDrawable,
    .DrawListAddCommand = DrawListAddCommand,
    .DrawListBind = DrawListBind,
    .DrawListUnbind = DrawListUnbind,
    .DrawListDestroy = DrawListDestroy,
    .DrawDataInit = DrawDataInit,
    .DrawDataAddDrawList = DrawDataAddDrawList,
    .DrawDataDestroy = DrawDataDestroy,
    .NewFrame = NewFrame,
    .Render = Render,
    .CreateTexture = CreateTexture,
    .TextureID = TextureID,
    .MeshInit = MeshInit,
    .MeshDestroy = MeshDestroy,
    .MaterialDestroy = MaterialDestroy,
    .CreateProgram = CreateProgram,
    .DestroyProgram = DestroyProgram,
    .GetAttributeLocations = GetAttributeLocations,
    .SwapBuffers = AxSwapBuffers
};

AXON_DLL_EXPORT void LoadPlugin(struct AxAPIRegistry *APIRegistry, bool Load)
{
    if (APIRegistry)
    {
        WindowAPI = APIRegistry->Get(AXON_WINDOW_API_NAME);
        PlatformAPI = APIRegistry->Get(AXON_PLATFORM_API_NAME);

        APIRegistry->Set(AXON_OPENGL_API_NAME, AxOpenGLAPI, sizeof(struct AxOpenGLAPI));
    }
}