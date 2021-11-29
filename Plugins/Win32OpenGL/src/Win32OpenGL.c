#include <Windows.h>
#include <stdio.h>
#include "GL/gl3w.h"

#include "RendererOpenGL.h"
#include "Drawable/AxDrawData.h"
#include "Drawable/AxDrawCommand.h"
#include "Drawable/AxDrawList.h"

#include "AxWindow/AxWindow.h"
#include "Foundation/APIRegistry.h"
#include "Foundation/Math.h"
#include "Foundation/Platform.h"
#include "Foundation/Camera.h"

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
    #if AXON_INTERNAL
    | WGL_CONTEXT_DEBUG_BIT_ARB
    #endif
    ,
    WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
    0
};

// API Handles
struct AxWindowAPI *WindowAPI;
struct AxPlatformAPI *PlatformAPI;

typedef struct AxTexture
{
    GLuint ID;
    uint32_t Width;
    uint32_t Height;
} AxTexture;

// typedef struct AxMesh
// {
//     uint32_t VertexBuffer;
//     uint32_t ElementBuffer;
//     uint32_t VertexArray;
//     uint32_t Texture;
//     AxTransform Transform; // TODO(mdeforge): Want to eventually keep this separate
// } AxMesh;

typedef struct AxViewport
{
    struct AxDrawData *DrawData;
} AxViewport;

struct OpenGLData
{
    GLuint ShaderHandle;
    GLuint IMGUIShaderHandle;
    GLuint AttribLocationTexture;
    GLuint AttribLocationVertexPos;
    GLuint AttribLocationVertexUV;
    GLuint AttribLocationVertexColor;
    GLuint AttribLocationProjectMatrix;
    uint32_t VBOHandle;
    uint32_t ElementsHandle;
    bool SupportsSRGBFramebuffer;
};

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

internal bool CheckShader(GLuint Handle, const char *Description)
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

internal bool CheckProgram(GLuint Handle, const char *Description)
{
    GLint Status = 0, LogLength = 0;
    glGetProgramiv(Handle, GL_LINK_STATUS, &Status);
    glGetProgramiv(Handle, GL_INFO_LOG_LENGTH, &LogLength);
    if ((GLboolean)Status == GL_FALSE) {
        fprintf(stderr, "ERROR: CreateDeviceObjects failed to link %s!\n", Description);
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

internal GLuint CreateProgram(char *HeaderCode, char *VertexCode, char *FragmentCode)
{
    // Create Shaders
    GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
    GLchar *VertexShaderCode[2] = { HeaderCode, VertexCode };
    glShaderSource(VertexShaderID, ArrayCount(VertexShaderCode), VertexShaderCode, 0);
    glCompileShader(VertexShaderID);
    CheckShader(VertexShaderID, "Vertex Shader");

    GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
    GLchar *FragmentShaderCode[2] = { HeaderCode, FragmentCode };
    glShaderSource(FragmentShaderID, ArrayCount(FragmentShaderCode), FragmentShaderCode, 0);
    glCompileShader(FragmentShaderID);
    CheckShader(FragmentShaderID, "Fragment Shader");

    // Link
    GLuint ProgramID = glCreateProgram();
    glAttachShader(ProgramID, VertexShaderID);
    glAttachShader(ProgramID, FragmentShaderID);
    glLinkProgram(ProgramID);
    CheckProgram(ProgramID, "Shader Program");

    glDetachShader(ProgramID, VertexShaderID);
    glDetachShader(ProgramID, FragmentShaderID);
    glDeleteShader(VertexShaderID);
    glDeleteShader(FragmentShaderID);

    return(ProgramID);
}

// TODO(mdeforge): Maybe move this to a script file since not being able to do multiline string literals in C sucks
internal bool CreateDeviceObjects(/*struct OpenGLData* Data*/)
{
    char *Header = "";

    char* VertexCode = "#version 460 core\n\
        layout (location = 0) in vec3 Position;\n\
        layout (location = 1) in vec2 UV;\n\
        layout (location = 2) in vec4 Color;\n\
        uniform mat4 ProjMtx;\n\
        out vec2 Frag_UV;\n\
        out vec4 Frag_Color;\n\
        void main()\n\
        {\n\
            Frag_UV = UV;\n\
            Frag_Color = Color;\n\
            //gl_Position = vec4(Position, 1.0);\n\
            gl_Position = ProjMtx * vec4(Position.xy, 0, 1);\n\
        }";

    char* FragmentCode = "#version 460 core\n\
        in vec2 Frag_UV;\n\
        in vec4 Frag_Color;\n\
        uniform sampler2D Texture;\n\
        layout (location = 0) out vec4 Out_Color;\n\
        void main()\n\
        {\n\
           Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n\
        }";

    Data->ShaderHandle = CreateProgram(Header, VertexCode, FragmentCode);

    // TODO(mdeforge): glGetUniformLocation for Texture
    Data->AttribLocationTexture = glGetUniformLocation(Data->ShaderHandle, "Texture");
    Data->AttribLocationProjectMatrix = glGetUniformLocation(Data->ShaderHandle, "ProjMtx");
    Data->AttribLocationVertexPos = glGetAttribLocation(Data->ShaderHandle, "Position");
    Data->AttribLocationVertexUV = glGetAttribLocation(Data->ShaderHandle, "UV");
    Data->AttribLocationVertexColor = glGetAttribLocation(Data->ShaderHandle, "Color");

    // Create buffers
    glGenBuffers(1, &Data->VBOHandle);
    glGenBuffers(1, &Data->ElementsHandle);

    return (true);
}

internal void DestroyDeviceObjects(/*struct OpenGLData *Data*/)
{
    if (Data->VBOHandle)
    {
        glDeleteBuffers(1, &Data->VBOHandle);
        Data->VBOHandle = 0;
    }

    if (Data->ElementsHandle)
    {
        glDeleteBuffers(1, &Data->ElementsHandle);
        Data->ElementsHandle = 0;
    }

    if (Data->ShaderHandle)
    {
        glDeleteProgram(Data->ShaderHandle);
        Data->ShaderHandle = 0;
    }
}

internal bool IsValidArray(GLuint Index)
{
    bool Result = (Index != -1);
    return(Result);
}

internal void Win32SetPixelFormat(/*struct OpenGLData *Data,*/ HDC WindowDC)
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

        if(!Data->SupportsSRGBFramebuffer) {
            IntAttribList[10] = 0;
        }

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

internal void SetupRenderState(AxDrawData *DrawData, int FramebufferWidth, int FramebufferHeight, GLuint VAO)
{
    // TODO(mdeforge): Each object is probably going to want its own render setup

    // Enable alpha blending and scissor test, disable face culling and depth test
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_SCISSOR_TEST);
    glDisable(GL_PRIMITIVE_RESTART);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // Setup view port, orthographic projection matrix
    glViewport(0, 0, (GLsizei)FramebufferWidth, (GLsizei)FramebufferHeight);

    float L = (float)DrawData->DisplayPos.X;
    float R = (float)DrawData->DisplayPos.X + (float)DrawData->DisplaySize.X;
    float B = (float)DrawData->DisplayPos.Y;
    float T = (float)DrawData->DisplayPos.Y + (float)DrawData->DisplaySize.Y;

    AxMat4x4f OrthoProjection = AxCameraAPI->CalcOrthographicProjection(L, R, B, T, 0.1f, 100.0f);
    OrthoProjection = Transpose(OrthoProjection);

    glUseProgram(Data->ShaderHandle);
    glUniform1i(Data->AttribLocationTexture, 0);
    glUniformMatrix4fv(Data->AttribLocationProjectMatrix, 1, GL_FALSE, &OrthoProjection.E[0][0]);

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

    // TODO(mdeforge): Everything below here pretty much just needs to happen once...

    // Bind the VAO first
    glBindVertexArray(VAO);

    // Buffer Element Data
    glBindBuffer(GL_ARRAY_BUFFER, Data->VBOHandle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Data->ElementsHandle);

    // Position Attribute
    glEnableVertexAttribArray(Data->AttribLocationVertexPos);
    glVertexAttribPointer(Data->AttribLocationVertexPos, 3, GL_FLOAT, GL_FALSE, sizeof(AxDrawVert), (GLvoid *)offsetof(AxDrawVert, Position));

    // UV Attribute
    glEnableVertexAttribArray(Data->AttribLocationVertexUV);
    glVertexAttribPointer(Data->AttribLocationVertexUV, 2, GL_FLOAT, GL_FALSE, sizeof(AxDrawVert), (GLvoid *)offsetof(AxDrawVert, UV));

    // Color Attribute
    glEnableVertexAttribArray(Data->AttribLocationVertexColor);
    glVertexAttribPointer(Data->AttribLocationVertexColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(AxDrawVert), (GLvoid *)offsetof(AxDrawVert, Color));
}

internal void Win32LoadWGLExtensions(/*struct OpenGLData *Data*/)
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
        wglChoosePixelFormatARB =
            (wgl_choose_pixel_format_arb *)wglGetProcAddress("wglChoosePixelFormatARB");
        wglCreateContextAttribsARB =
            (wgl_create_context_attribs_arb *)wglGetProcAddress("wglCreateContextAttribsARB");
        wglGetExtensionsStringEXT =
            (wgl_get_extensions_string_ext *)wglGetProcAddress("wglGetExtensionStringEXT");
        wglSwapIntervalEXT =
            (wgl_swap_interval_ext *)wglGetProcAddress("wglSwapIntervalEXT");

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
                    Data->SupportsSRGBFramebuffer = true;
                } else if(strncmp(At, "WGL_ARB_framebuffer_sRGB", Count)) {
                    Data->SupportsSRGBFramebuffer = true;
                }

                At = End;
            }
        }

        wglMakeCurrent(DummyWindowDC, 0);
    }

    wglDeleteContext(DummyContext);
    ReleaseDC(DummyWindow, DummyWindowDC);
    DestroyWindow(DummyWindow);
}

static GLuint VAO = 0;
void Create(AxWindow *Window)
{
    Assert(Window && "Create passed NULL Window pointer!");

    // Initialize global data
    Data = calloc(1, sizeof(struct OpenGLData));

    // Trampoline the OpenGL 1.0 context to 4.6
    Win32LoadWGLExtensions(Data);

    // Get device context for Window
    AxWin32WindowData Win32WindowData = WindowAPI->PlatformData(Window).Win32;
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
        wglSwapIntervalEXT(-1); // Disable vsync
    }

    // Load OpenGL Extensions
    gl3wInit();

    // The call to glVertexAttribPointer registered VBO as the vertex attribute's
    // bound vertex buffer object so afterwards we can safely unbind
    //glBindBuffer(GL_ARRAY_BUFFER, 0);
    //glBindVertexArray(0);

    // Create default viewport
    Viewport = calloc(1, sizeof(struct AxViewport));

    // Create default vertex array object
    glGenVertexArrays(1, &VAO);
}

void NewFrame(void)
{
    Assert(Data && "NewFrame passed NULL OpenGLData pointer!");

    if (Data)
    {
        if (!Data->ShaderHandle) {
            CreateDeviceObjects(Data);
        }
    }

    //glViewport(0, 0, (GLsizei)DrawData->DisplaySize.X, (GLsizei)DrawData->DisplaySize.Y);
    glClearColor(0.42f, 0.51f, 0.54f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void RenderDrawData(AxDrawData *DrawData)
{
    // TODO(mdeforge): Avoid rendering when minimized

    int FramebufferWidth = (int)(DrawData->DisplaySize.X * DrawData->FramebufferScale.X);
    int FramebufferHeight = (int)(DrawData->DisplaySize.Y * DrawData->FramebufferScale.Y);
    if (FramebufferWidth <= 0 || FramebufferHeight <= 0) {
        return;
    }

    // Backup GL state
    GLenum last_active_texture; glGetIntegerv(GL_ACTIVE_TEXTURE, (GLint*)&last_active_texture);
    glActiveTexture(GL_TEXTURE0);
    GLuint last_program; glGetIntegerv(GL_CURRENT_PROGRAM, (GLint*)&last_program);
    GLuint last_texture; glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint*)&last_texture);
#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_BIND_SAMPLER
    GLuint last_sampler; if (bd->GlVersion >= 330) { glGetIntegerv(GL_SAMPLER_BINDING, (GLint*)&last_sampler); } else { last_sampler = 0; }
#endif
    GLuint last_array_buffer; glGetIntegerv(GL_ARRAY_BUFFER_BINDING, (GLint*)&last_array_buffer);
#ifdef IMGUI_IMPL_OPENGL_USE_VERTEX_ARRAY
    GLuint last_vertex_array_object; glGetIntegerv(GL_VERTEX_ARRAY_BINDING, (GLint*)&last_vertex_array_object);
#endif
#ifdef IMGUI_IMPL_HAS_POLYGON_MODE
    GLint last_polygon_mode[2]; glGetIntegerv(GL_POLYGON_MODE, last_polygon_mode);
#endif
    GLint last_viewport[4]; glGetIntegerv(GL_VIEWPORT, last_viewport);
    GLint last_scissor_box[4]; glGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);
    GLenum last_blend_src_rgb; glGetIntegerv(GL_BLEND_SRC_RGB, (GLint*)&last_blend_src_rgb);
    GLenum last_blend_dst_rgb; glGetIntegerv(GL_BLEND_DST_RGB, (GLint*)&last_blend_dst_rgb);
    GLenum last_blend_src_alpha; glGetIntegerv(GL_BLEND_SRC_ALPHA, (GLint*)&last_blend_src_alpha);
    GLenum last_blend_dst_alpha; glGetIntegerv(GL_BLEND_DST_ALPHA, (GLint*)&last_blend_dst_alpha);
    GLenum last_blend_equation_rgb; glGetIntegerv(GL_BLEND_EQUATION_RGB, (GLint*)&last_blend_equation_rgb);
    GLenum last_blend_equation_alpha; glGetIntegerv(GL_BLEND_EQUATION_ALPHA, (GLint*)&last_blend_equation_alpha);
    GLboolean last_enable_blend = glIsEnabled(GL_BLEND);
    GLboolean last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
    GLboolean last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
    GLboolean last_enable_stencil_test = glIsEnabled(GL_STENCIL_TEST);
    GLboolean last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);

    // TODO(mdeforge): Maybe don't check for this every time and only setup RenderState once?
    SetupRenderState(DrawData, FramebufferWidth, FramebufferHeight, VAO);

    // Will project scissor/clipping rectangles into framebuffer space
    AxVec2 ClipOff = DrawData->DisplayPos;
    AxVec2 ClipScale = DrawData->FramebufferScale;

    // Render command lists
    size_t a = ArraySize(DrawData->CommandList);
    for (int i = 0; i < ArraySize(DrawData->CommandList); i++)
    {
        const AxDrawList *DrawList = &DrawData->CommandList[i];

        // If the draw list dirty, buffer data
        //if (DrawList->IsDirty)
        //{
            const AxDrawVert *VertexBuffer = DrawList->VertexBuffer;
            const AxDrawIndex *IndexBuffer = DrawList->IndexBuffer;
            // TODO(mdeforge): SubData???
            // TODO(mdeforge): Better to do at DrawList level like before or at command level?
            // Upload vertex and index buffers
            glBufferData(GL_ARRAY_BUFFER, ArraySizeInBytes(VertexBuffer), VertexBuffer, GL_STREAM_DRAW);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, ArraySizeInBytes(IndexBuffer), IndexBuffer, GL_STREAM_DRAW);
            //DrawList->IsDirty = false;
        //}

        size_t CommandBufferSize = ArraySize(DrawList->CommandBuffer);
        for (int j = 0; j < ArraySize(DrawList->CommandBuffer); j++)
        {
            const AxDrawCommand *Command = &DrawList->CommandBuffer[j];
            if (Command)
            {
                // Project scissor/clipping rectangles into framebuffer space
                AxVec2 ClipMin = {
                    (Command->ClipRect.X - ClipOff.X) * ClipScale.X,
                    (Command->ClipRect.Y - ClipOff.Y) * ClipScale.Y
                };

                AxVec2 ClipMax = {
                    (Command->ClipRect.Z - ClipOff.X) * ClipScale.X,
                    (Command->ClipRect.W - ClipOff.Y) * ClipScale.Y
                };

                if (ClipMax.X < ClipMin.X || ClipMax.Y < ClipMin.Y) {
                    continue;
                }

                // Apply scissor/clipping rectangle (Y is inverted in OpenGL)
                glScissor((int)ClipMin.X, (int)(FramebufferHeight - ClipMax.Y), (int)(ClipMax.X - ClipMin.X), (int)(ClipMax.Y - ClipMin.Y));

                // Bind texture and draw
                glBindTexture(GL_TEXTURE_2D, (GLuint)Command->TextureID);
                glDrawElementsBaseVertex(
                    GL_TRIANGLES,
                    (GLsizei)Command->ElementCount,
                    GL_UNSIGNED_INT,
                    0,
                    (GLint)Command->VertexOffset
                );
            }
        }
    }

    // TODO(mdeforge): Oh boy...
    // Clean up this save/restore state business. How IMGUI specific is this? Should it be brought to the surface in AxEditor?
    // Fix the render glitch (test on my computer as well)
    // Add OpenGL version ifdefs
    // Create render paths (get ready for non immediate mode stuff)

    // Restore modified GL state
    glUseProgram(last_program);
    glBindTexture(GL_TEXTURE_2D, last_texture);
#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_BIND_SAMPLER
    if (bd->GlVersion >= 330)
        glBindSampler(0, last_sampler);
#endif
    glActiveTexture(last_active_texture);
#ifdef IMGUI_IMPL_OPENGL_USE_VERTEX_ARRAY
    glBindVertexArray(last_vertex_array_object);
#endif
    glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
    glBlendEquationSeparate(last_blend_equation_rgb, last_blend_equation_alpha);
    glBlendFuncSeparate(last_blend_src_rgb, last_blend_dst_rgb, last_blend_src_alpha, last_blend_dst_alpha);
    if (last_enable_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (last_enable_cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (last_enable_depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (last_enable_stencil_test) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
    if (last_enable_scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_PRIMITIVE_RESTART
    if (bd->GlVersion >= 310) { if (last_enable_primitive_restart) glEnable(GL_PRIMITIVE_RESTART); else glDisable(GL_PRIMITIVE_RESTART); }
#endif

#ifdef IMGUI_IMPL_HAS_POLYGON_MODE
    glPolygonMode(GL_FRONT_AND_BACK, (GLenum)last_polygon_mode[0]);
#endif
    glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
    glScissor(last_scissor_box[0], last_scissor_box[1], (GLsizei)last_scissor_box[2], (GLsizei)last_scissor_box[3]);
}

void Render(AxDrawData *DrawData)
{
    glViewport(0, 0, (GLsizei)DrawData->DisplaySize.X, (GLsizei)DrawData->DisplaySize.Y);
    // glClearColor(0.42f, 0.51f, 0.54f, 0.0f);
    // glClear(GL_COLOR_BUFFER_BIT);

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

struct AxDrawData *GetDrawData(void)
{
    return (Viewport->DrawData->Valid ? Viewport->DrawData : NULL);
}

void AxSwapBuffers(void)
{
    SwapBuffers(wglGetCurrentDC());
}

static AxTexture *CreateTexture(const uint32_t Width, const uint32_t Height, const void *Pixels)
{
    //Assert(OpenGL && "CreateTexture passed NULL OpenGL Pointer!");

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
    Assert(Texture);

    return ((Texture) ? Texture->ID : 0);
}

static void Destroy(void)
{
    SAFE_FREE(Data);
}

struct AxOpenGLAPI *AxOpenGLAPI = &(struct AxOpenGLAPI) {
    .Create = Create,
    .Destroy = Destroy,
    .GetInfo = GetInfo,
    .NewFrame = NewFrame,
    .GetDrawData = GetDrawData,
    .Render = Render,
    .CreateTexture = CreateTexture,
    .TextureID = TextureID,
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