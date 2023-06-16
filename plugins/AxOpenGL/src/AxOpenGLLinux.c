#include <stdio.h>
#include "GL/gl3w.h"

#include "AxOpenGL.h"
#include "Drawable/AxDrawData.h"
#include "Drawable/AxDrawCommand.h"
#include "Drawable/AxDrawList.h"

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

static bool CheckProgram(GLuint Handle, const char *Description)
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

static GLuint CreateProgram(char *HeaderCode, char *VertexCode, char *FragmentCode)
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
static bool CreateDeviceObjects(/*struct OpenGLData* Data*/)
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

static void DestroyDeviceObjects(/*struct OpenGLData *Data*/)
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

static void SetupRenderState(AxDrawData *DrawData, int FramebufferWidth, int FramebufferHeight, GLuint VAO)
{
    // NOTE(mdeforge): This is currently setup for orthographic sprite drawing since the immediate focus is 2D sprites.

    // TODO(mdeforge): Instead of each object going through it's own render setup, batch like objects as a part of the
    // same AxDrawData. The editor should help achieve this. All objects with the same VAO should be together.

    // TODO(mdeforge): Create the concept of a camera and organize ortho vs perp based on camera

    // Enable alpha blending and scissor test, disable face culling and depth test
	// TODO(mdeforge): This is currently causing an issue but probably because I have to put back in textures
    // glEnable(GL_BLEND);
    // glBlendEquation(GL_FUNC_ADD);
    // glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
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

    AxMat4x4f OrthoProjection = CameraAPI->CalcOrthographicProjection(L, R, B, T, 0.1f, 100.0f);
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

    // Bind the VAO first
    glBindVertexArray(VAO);

    // Buffer Element Data
    glBindBuffer(GL_ARRAY_BUFFER, Data->VBOHandle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Data->ElementsHandle);

    // Position Attribute
    glEnableVertexAttribArray(Data->AttribLocationVertexPos);
    glVertexAttribPointer(Data->AttribLocationVertexPos, 3, GL_FLOAT, GL_FALSE, sizeof(struct AxDrawVert), (GLvoid *)offsetof(struct AxDrawVert, Position));

    // UV Attribute
    glEnableVertexAttribArray(Data->AttribLocationVertexUV);
    glVertexAttribPointer(Data->AttribLocationVertexUV, 2, GL_FLOAT, GL_FALSE, sizeof(struct AxDrawVert), (GLvoid *)offsetof(struct AxDrawVert, UV));

    // Color Attribute
    glEnableVertexAttribArray(Data->AttribLocationVertexColor);
    glVertexAttribPointer(Data->AttribLocationVertexColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(struct AxDrawVert), (GLvoid *)offsetof(struct AxDrawVert, Color));
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

    SetupRenderState(DrawData, FramebufferWidth, FramebufferHeight, VAO);

    // Will project scissor/clipping rectangles into framebuffer space
    AxVec2 ClipOff = DrawData->DisplayPos;
    AxVec2 ClipScale = DrawData->FramebufferScale;

    // Render command lists
    size_t a = ArraySize(DrawData->CommandList);
    for (int i = 0; i < ArraySize(DrawData->CommandList); i++)
    {
        struct AxDrawList *DrawList = &DrawData->CommandList[i];

        // If the draw list dirty, buffer data
        if (DrawList->IsDirty)
        {
            const struct AxDrawVert *VertexBuffer = DrawList->VertexBuffer;
            const AxDrawIndex *IndexBuffer = DrawList->IndexBuffer;
            // TODO(mdeforge): SubData???
            // TODO(mdeforge): Better to do at DrawList level like before or at command level?
            // Upload vertex and index buffers
            glBufferData(GL_ARRAY_BUFFER, ArraySizeInBytes(VertexBuffer), VertexBuffer, GL_STATIC_DRAW);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, ArraySizeInBytes(IndexBuffer), IndexBuffer, GL_STATIC_DRAW);
            DrawList->IsDirty = false;
        }

        size_t CommandBufferSize = ArraySize(DrawList->CommandBuffer);
        for (int j = 0; j < ArraySize(DrawList->CommandBuffer); j++)
        {
            const struct AxDrawCommand *Command = &DrawList->CommandBuffer[j];
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
                //glScissor((int)ClipMin.X, (int)(FramebufferHeight - ClipMax.Y), (int)(ClipMax.X - ClipMin.X), (int)(ClipMax.Y - ClipMin.Y));

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
}

void Render(AxDrawData *DrawData)
{
    //glViewport(0, 0, (GLsizei)DrawData->DisplaySize.X, (GLsizei)DrawData->DisplaySize.Y);
    //glClearColor(0.42f, 0.51f, 0.54f, 0.0f);
    //glClear(GL_COLOR_BUFFER_BIT);

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

// Use a compound literal to construct an unnamed object of API type in-place
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