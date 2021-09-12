#include <Windows.h>
#include <stdio.h>
//#include "glad/glad.h"
#include "GL/gl3w.h"
#include "Win32OpenGL/RendererOpenGL.h"
#include "Foundation/APIRegistry.h"
#include "AxWindow/AxWindow.h"
#include "Foundation/Math.h"
#include "Foundation/Platform.h"

//#define _CRTDBG_MAP_ALLOC
//#include <stdlib.h>
//#include <crtdbg.h>

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

typedef HGLRC WINAPI wgl_create_context_attribs_arb(HDC hDC, HGLRC hShareContext,
    const int *attribList);

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

// TODO(mdeforge): Huge hack, need to pass this in somewhere
static float time;

struct AxWindowAPI *WindowAPI;
struct AxPlatformAPI *PlatformAPI;

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

static GLuint Win32OpenGLCreateProgram(char *HeaderCode, char *VertexCode, char *FragmentCode, struct ShaderToy *Result)
{
    GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
    GLchar *VertexShaderCode[] = {
        HeaderCode,
        VertexCode
    };

    glShaderSource(VertexShaderID, ArrayCount(VertexShaderCode), VertexShaderCode, 0);
    glCompileShader(VertexShaderID);

    GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
    GLchar *FragmentShaderCode[] = {
        HeaderCode,
        FragmentCode
    };

    glShaderSource(FragmentShaderID, ArrayCount(FragmentShaderCode), FragmentShaderCode, 0);
    glCompileShader(FragmentShaderID);

    GLuint ProgramID = glCreateProgram();
    glAttachShader(ProgramID, VertexShaderID);
    glAttachShader(ProgramID, FragmentShaderID);
    glLinkProgram(ProgramID);
    glValidateProgram(ProgramID);

    GLint Linked = false;
    glGetProgramiv(ProgramID, GL_VALIDATE_STATUS, &Linked);

    if (!Linked)
    {
        GLsizei Ignored;
        char VertexErrors[4096];
        char FragmentErrors[4096];
        char ProgramErrors[4096];
        glGetShaderInfoLog(VertexShaderID, sizeof(VertexErrors), &Ignored, VertexErrors);
        glGetShaderInfoLog(FragmentShaderID, sizeof(FragmentErrors), &Ignored, FragmentErrors);
        glGetShaderInfoLog(ProgramID, sizeof(ProgramErrors), &Ignored, ProgramErrors);
        int i = 0;
        GLenum error4 = glGetError();
        Assert(!"Shader validation failed");
    }

    glDeleteShader(VertexShaderID);
    glDeleteShader(FragmentShaderID);

    Result->ProgramHandle = ProgramID;
    Result->iResolution = glGetAttribLocation(ProgramID, "iResolution");
    Result->iMouse = glGetAttribLocation(ProgramID, "iMouse");
    Result->iTime = glGetAttribLocation(ProgramID, "iTime");

    return(ProgramID);
}

// TODO(mdeforge): Maybe move this to a script file since not being able to do multiline string literals in C sucks
// TODO(mdeforge): Need more generic functions for compiling shaders
static void CompileShaderToy(struct OpenGL *OpenGL, struct ShaderToy *Result)
{
    char *Header = "";
    char *VertexCode = 
    "#version 460 core\n\
    layout (location = 0) in vec3 aPos;\n\
    layout (location = 1) in vec3 aColor;\n\
    layout (location = 2) in vec2 aTexCoord;\n\
    \n\
    out vec3 Color;\n\
    out vec2 TexCoord;\n\
    \n\
    void main()\n\
    {\n\
        gl_Position =  vec4(aPos, 1.0f);\n\
        Color = aColor;\n\
        TexCoord = aTexCoord;\n\
    };";

    char *FragmentCode = 
    "#version 460 core\n\
    out vec4 FragColor;\n\
    \n\
    in vec3 Color;\n\
    in vec2 TexCoord;\n\
    \n\
    uniform sampler2D texture1;\n\
    \n\
    void main()\n\
    {\n\
        FragColor = vec4(Color, 1.0);\n\
    };";

    GLuint Prog = Win32OpenGLCreateProgram(Header, VertexCode, FragmentCode, Result);
}

static bool IsValidArray(GLuint Index)
{
    bool Result = (Index != -1);
    return(Result);
}

static void UseProgramBegin(struct ShaderToy *Program)
{
    if (Program) {
        glUseProgram(Program->ProgramHandle);
    }
}

static void UseProgramEnd()
{
    glUseProgram(0);
}

static void Win32SetPixelFormat(struct OpenGL *OpenGL, HDC WindowDC)
{
    int SuggestedPixelFormatIndex = 0;
    GLuint ExtendedPick = 0;
    if(wglChoosePixelFormatARB)
    {
        int IntAttribList[] =
            {
                WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
                WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
                WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
                WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
                WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
                WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, GL_TRUE,
                0,
            };

        if(!OpenGL->SupportsSRGBFramebuffer)
        {
            IntAttribList[10] = 0;
        }

        wglChoosePixelFormatARB(WindowDC, IntAttribList, 0, 1,
                                &SuggestedPixelFormatIndex, &ExtendedPick);
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
    DescribePixelFormat(WindowDC, SuggestedPixelFormatIndex,
        sizeof(SuggestedPixelFormat), &SuggestedPixelFormat);
    SetPixelFormat(WindowDC, SuggestedPixelFormatIndex, &SuggestedPixelFormat);
}

/**
 * Gets called by Win32InitOpenGL to trampoline the OpenGL 1.0 context to 4.6
 */
static void Win32LoadWGLExtensions(struct OpenGL *OpenGL)
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

    Win32SetPixelFormat(OpenGL, DummyWindowDC); // .09

    HGLRC DummyContext = wglCreateContext(DummyWindowDC);

    if(wglMakeCurrent(DummyWindowDC, DummyContext)) // .06
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
                    OpenGL->SupportsSRGBFramebuffer = true;
                } else if(strncmp(At, "WGL_ARB_framebuffer_sRGB", Count)) {
                    OpenGL->SupportsSRGBFramebuffer = true;
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

struct OpenGLInfo Win32OpenGLGetInfo(bool ModernContext)
{
    struct OpenGLInfo Result = {0};

    Result.ModernContext = ModernContext;
    Result.Vendor = (char *)glGetString(GL_VENDOR);
    Result.Renderer = (char *)glGetString(GL_RENDERER);
    Result.Version = (char *)glGetString(GL_VERSION);

    if(Result.ModernContext) {
        Result.ShadingLanguageVersion = (char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
    } else {
        Result.ShadingLanguageVersion = "(none)";
    }

    return(Result);
}

// Because there is nothing Win32 specific here there's nothing much to see
static struct AxRenderCommands *Win32OpenGLBeginFrame(struct OpenGL *OpenGL, uint32_t WindowWidth, uint32_t WindowHeight, AxRect DrawRegion)
{
    struct AxRenderCommands *Commands = &OpenGL->RenderCommands;

    Commands->WindowWidth = WindowWidth;
    Commands->WindowHeight = WindowHeight;
    Commands->DrawRegion = DrawRegion;

    return(Commands);
}

// Here there is a Win32 specific call to SwapBuffers. It's nice to have control, per platform,
// of when that platform will swap buffers.
// TODO(mdeforge): Remove OpenGL parameter, huge hack for now
static void Win32OpenGLEndFrame(struct OpenGL *OpenGL, struct AxRenderCommands *Commands)
{
    Assert(OpenGL && "Win32OpenGLEndFrame passed NULL OpenGL Pointer!");
    if (OpenGL == NULL) {
        return;
    }

    int32_t WindowWidth = Commands->WindowWidth;
    int32_t WindowHeight = Commands->WindowHeight;
    AxRect DrawRegion = Commands->DrawRegion;

    glViewport(0, 0, WindowWidth, WindowHeight);
    glClearColor(0.42f, 0.51f, 0.54f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // TODO(mdeforge): This is all a huge hack for now...
    // Get the shader program
    struct ShaderToy *Program = &OpenGL->ShaderProgram;
    UseProgramBegin(Program);

    glBindVertexArray(OpenGL->Mesh.VertexArray);

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

    GLint resLoc = glGetProgramResourceLocation(Program->ProgramHandle, GL_UNIFORM, "iResolution");
    GLint mouseLoc = glGetProgramResourceLocation(Program->ProgramHandle, GL_UNIFORM, "iMouse");
    GLint timeLoc = glGetProgramResourceLocation(Program->ProgramHandle, GL_UNIFORM, "iTime");

    //Assert(FullTransformMatrixLocation != -1);

    //glProgramUniformMatrix4fv(Program->ProgramHandle, FullTransformMatrixLocation, 1, GL_FALSE, &MVP.E[0][0]);

    float Mouse[2] = { 0.3f, 0.3f };
    glProgramUniform3f(Program->ProgramHandle, resLoc, 
        (GLfloat) Commands->WindowWidth, (GLfloat) Commands->WindowHeight, 0);
    glProgramUniform2fv(Program->ProgramHandle, mouseLoc, 1, &Mouse[0]);
    glProgramUniform1f(Program->ProgramHandle, timeLoc, time);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, NULL);
    //glBindVertexArray(0);

    UseProgramEnd();
    time += 0.01667f;

    SwapBuffers(wglGetCurrentDC());
}

static struct OpenGL *Win32OpenGLCreate(void)
{
    struct OpenGL *OpenGL = (struct OpenGL *)malloc(sizeof(struct OpenGL));

    Win32LoadWGLExtensions(OpenGL); // .17

    return (OpenGL);
}

static void Win32OpenGLInit(struct OpenGL *OpenGL, AxWindow *Window)
{
     // Get device context for Window
    AxWin32WindowData Win32WindowData = WindowAPI->PlatformData(Window).Win32;
    HDC DeviceContext = GetDC((HWND)Win32WindowData.Handle);

    Win32SetPixelFormat(OpenGL, DeviceContext);

    bool ModernContext = true;
    HGLRC OpenGLRC = 0;

    if (wglCreateContextAttribsARB)  {
        OpenGLRC = wglCreateContextAttribsARB(DeviceContext, 0, Win32OpenGLAttribs);
    }

    if (!OpenGLRC)
    {
        ModernContext = false;
        OpenGLRC = wglCreateContext((HDC)DeviceContext);
    }

    if (wglMakeCurrent((HDC)DeviceContext, OpenGLRC)) {
        wglSwapIntervalEXT(-1); // Disable vsync
    }

    // Load OpenGL Extensions
    //gladLoadGL();
    gl3wInit();

    time = 0.0f; // A hack, for now...

    // Compile shader
    OpenGL->ShaderProgram = (struct ShaderToy){ 0 };
    CompileShaderToy(OpenGL, &OpenGL->ShaderProgram);

    // NOTE(mdeforge): My previous shader setup I actually had a texture being loaded
    float vertices[] = {
         // positions       // colors         // texture coords
         0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,   // top right
         0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,   // bottom right
        -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,   // bottom left
        -0.5f,  0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f    // top left 
    };

    uint32_t indices[] = {
        0, 1, 3, // first triangle
        1, 2, 3  // second triangle
    };

    glGenVertexArrays(1, &OpenGL->Mesh.VertexArray);
    glGenBuffers(1, &OpenGL->Mesh.VertexBuffer);
    glGenBuffers(1, &OpenGL->Mesh.ElementBuffer);

    // Bind the VAO first
    glBindVertexArray(OpenGL->Mesh.VertexArray);

    // Buffer Vertex Data
    glBindBuffer(GL_ARRAY_BUFFER, OpenGL->Mesh.VertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Buffer Element Data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OpenGL->Mesh.ElementBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Position Attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Color Attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Texture Attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // glGenTextures(1, &Mesh.Texture);
    // glBindTexture(GL_TEXTURE_2D, Mesh.Texture);

    // Set the wrapping parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // Set the filtering parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, OpenGL->Sprites[i].Image.Width, OpenGL->Sprites[i].Image.Height,
    //                 0, GL_RGBA, GL_UNSIGNED_BYTE, OpenGL->Sprites[i].Image.Data);

    // The call to glVertexAttribPointer registered VBO as the vertex attribute's
    // bound vertex buffer object so afterwards we can safely unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Enable alpha blending
    //glEnable(GL_BLEND);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //UseProgramBegin(&OpenGL->ShaderProgram);
}

static void Win32OpenGLDestroy(struct OpenGL *OpenGL)
{
    if (OpenGL)
    {
        free(OpenGL);
        OpenGL = NULL;
    }
}

struct AxOpenGLAPI *AxOpenGLAPI = &(struct AxOpenGLAPI) {
    .Create = Win32OpenGLCreate,
    .Init = Win32OpenGLInit,
    .Destroy = Win32OpenGLDestroy,
    .GetInfo = Win32OpenGLGetInfo,
    .BeginFrame = Win32OpenGLBeginFrame,
    .EndFrame = Win32OpenGLEndFrame
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