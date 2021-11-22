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
    GLuint AttribLocationVertexPos;
    GLuint AttribLocationVertexUV;
    GLuint AttribLocationVertexColor;
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
    //char *VertexCode = 
    //    "#version 460 core\n\
    //    layout (location = 0) in vec3 Position;\n\
    //    //layout (location = 1) in vec3 Color;\n\
    //    //layout (location = 2) in vec2 UV;\n\
    //    \n\
    //    //out vec3 FragColor;\n\
    //    //out vec2 FragUV;\n\
    //    \n\
    //    void main()\n\
    //    {\n\
    //        gl_Position = vec4(Position, 1.0f);\n\
    //        //FragColor = Color;\n\
    //        //FragUV = UV;\n\
    //    };";

    //char *FragmentCode = 
    //    "#version 460 core\n\
    //    out vec4 OutColor;\n\
    //    \n\
    //    //in vec3 FragColor;\n\
    //    //in vec2 FragUV;\n\
    //    //\n\
    //    //uniform sampler2D texture1;\n\
    //    //\n\
    //    void main()\n\
    //    {\n\
    //        OutColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n\
    //        //OutColor = vec4(FragColor, 1.0);\n\
    //    };";

    char* VertexCode = "#version 460 core\n\
        layout (location = 0) in vec3 aPos;\n\
        void main()\n\
        {\n\
           gl_Position = vec4(aPos, 1.0);\n\
        }";

    char* FragmentCode = "#version 460 core\n\
        out vec4 FragColor;\n\
        void main()\n\
        {\n\
           FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n\
        }";

    Data->ShaderHandle = CreateProgram(Header, VertexCode, FragmentCode);

    // TODO(mdeforge): glGetUniformLocation for Texture
    Data->AttribLocationVertexPos = glGetAttribLocation(Data->ShaderHandle, "aPos");
    //Data->AttribLocationVertexColor = glGetAttribLocation(Data->ShaderHandle, "Color");
    //Data->AttribLocationVertexUV = glGetAttribLocation(Data->ShaderHandle, "UV");

    //GLint resLoc = glGetProgramResourceLocation(OpenGLData->ShaderHandle, GL_UNIFORM, "iResolution");
    //GLint mouseLoc = glGetProgramResourceLocation(OpenGLData->ShaderHandle, GL_UNIFORM, "iMouse");

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

internal void SetupRenderState(/*struct OpenGLData *Data,*/ AxDrawData *DrawData, GLuint VAO)
{
    // TODO(mdeforge): Each object is probably going to want its own render setup
    
    // Enable alpha blending and scissor test, disable face culling and depth test
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_SCISSOR_TEST);

    // TODO(mdeforge): Setup view port, orthographic projection matrix
    //glViewport(0, 0, (GLsizei)DrawData->DisplaySize.X, (GLsizei)DrawData->DisplaySize.Y);
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

    // Buffer Vertex Data
    glBindBuffer(GL_ARRAY_BUFFER, Data->VBOHandle);

    // Position Attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // UV Attribute
    //glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(AxDrawVert), (void*)(3 * sizeof(float)));
    //glEnableVertexAttribArray(1);

    // Color Attribute
    //glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(AxDrawVert), (void*)(5 * sizeof(float)));
    //glEnableVertexAttribArray(2);

    // Buffer Element Data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Data->ElementsHandle);

    // NOTE(mdeforge): Dangerous
    //glBindBuffer(GL_ARRAY_BUFFER, 0);
    //glBindVertexArray(0);
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
}

static bool IsFirstTime = true;
void RenderDrawData(AxDrawData *DrawData)
{
    // TODO(mdeforge): Avoid rendering when minimized

    // TODO(mdeforge): Maybe don't check for this every time and only setup RenderState once?
    GLuint VAO = 0;
    if (IsFirstTime)
    {
        glGenVertexArrays(1, &VAO);
        SetupRenderState(DrawData, VAO);
        glBindVertexArray(VAO);
    }

    // Render command lists
    for (int i = 0; i < ArraySize(DrawData->CommandList); i++)
    {
        AxDrawList *DrawList = DrawData->CommandList[i];
        const AxDrawVert *VertexBuffer = DrawList->VertexBuffer;
        const AxDrawIndex *IndexBuffer = DrawList->IndexBuffer;
        
        for (int j = 0; j < ArraySize(DrawList->CommandBuffer); j++)
        {
            AxDrawCommand *Command = &DrawList->CommandBuffer[j];
            if (Command)
            {
                // NOTE(mdeforge): This needs to occur AFTER SetupRenderState
                // since SetupRenderState is binding the buffers...
                
                // TODO(mdeforge): Re-evaluate given the above note
                
                // If the draw list dirty, buffer data
                if (DrawList->IsDirty)
                {
                    // Upload vertex and index buffers
                    // TODO(mdeforge): SubData???
                    // TODO(mdeforge): Better to do at DrawList level like before or at command level?
                    glBufferData(GL_ARRAY_BUFFER, ArraySizeInBytes(VertexBuffer), VertexBuffer, GL_STATIC_DRAW);
                    glBufferData(GL_ELEMENT_ARRAY_BUFFER, ArraySizeInBytes(IndexBuffer), IndexBuffer, GL_STATIC_DRAW);
                    DrawList->IsDirty = false;
                }
           
                // Project scissor/clipping rectangles into framebuffer space

                // Apply scissor/clipping rectangle (Y is inverted in OpenGL)
                //glScissor

                // Bind texture and draw
                //glBindTexture(GL_TEXTURE_2D, (GLuint)Command->TextureID);
                glUseProgram(Data->ShaderHandle);
                
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
    glViewport(0, 0, 1920, 1080);
    glClearColor(0.42f, 0.51f, 0.54f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    RenderDrawData(DrawData);
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

// static AxTexture *CreateTexture(struct OpenGL *OpenGL, int32_t Width, int32_t Height, void *Data)
// {
//     Assert(OpenGL && "CreateTexture passed NULL OpenGL Pointer!");

//     AxTexture *Texture = calloc(1, sizeof(AxTexture));
//     if (!Texture) {
//         return (NULL);
//     }

//     glGenTextures(1, &Texture->ID);
//     glBindTexture(GL_TEXTURE_2D, Texture->ID);

//     // Setup filtering parameters
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same

//     // Upload pixels into texture
//     glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Width, Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, Data);
//     glGenerateMipmap(GL_TEXTURE_2D);

//     return (Texture);
// }

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