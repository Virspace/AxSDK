#include <Windows.h>
#include <stdio.h>
#include "GL/gl3w.h"

#include "AxOpenGL.h"

#include "AxWindow/AxWindow.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxMath.h"
#include "Foundation/AxPlatform.h"
#include "GL/glcorearb.h"

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
static bool ContextInitialized = false;
static HGLRC RenderContext = 0;

///////////////////////////////////////////////////////////////
// Internal Functions
///////////////////////////////////////////////////////////////
void APIENTRY GLDebugMessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam)
{
    fprintf(stderr, "----------opengl-callback-start----------\n");
    fprintf(stderr, "message: %s\n", message);
    fprintf(stderr, "type: ");

    switch (type) {
    case GL_DEBUG_TYPE_ERROR:
        fprintf(stderr, "ERROR\n");
        break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        fprintf(stderr, "DEPRECATED_BEHAVIOR\n");
        break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        fprintf(stderr, "UNDEFINED_BEHAVIOR\n");
        break;
    case GL_DEBUG_TYPE_PORTABILITY:
        fprintf(stderr, "PORTABILITY\n");
        break;
    case GL_DEBUG_TYPE_PERFORMANCE:
        fprintf(stderr, "PERFORMANCE\n");
        break;
    case GL_DEBUG_TYPE_OTHER:
        fprintf(stderr, "OTHER\n");
        break;
    }

    fprintf(stderr, "id: %u\n", id);
    fprintf(stderr, "severity: ");
    switch (severity){
    case GL_DEBUG_SEVERITY_LOW:
        fprintf(stderr, "LOW\n");
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        fprintf(stderr, "MEDIUM\n");
        break;
    case GL_DEBUG_SEVERITY_HIGH:
        fprintf(stderr, "HIGH\n");
        break;
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "----------opengl-callback-end----------\n");

    AXON_ASSERT(false && "OpenGL Error!");
}

// TODO(mdeforge): Move to Foundation
inline bool IsEndOfLine(char C)
{
    bool Result = ((C == '\n') || (C == '\r'));

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
    AXON_ASSERT(ContextInitialized && "Context not initialized in CheckProgram()!");

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
    AXON_ASSERT(ContextInitialized && "Context not initialized in SetupRenderState()!");

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

///////////////////////////////////////////////////////////////
// Public API
///////////////////////////////////////////////////////////////
void AxCreateContext(struct AxWindow *Window)
{
    AXON_ASSERT(Window && "Window is NULL in CreateContext()!");

    // Trampoline the OpenGL 1.0 context to 4.6
    Win32LoadWGLExtensions();

    // Get device context for Window
    AxWin32WindowData Win32WindowData = WindowAPI->GetPlatformData(Window).Win32;
    HDC DeviceContext = GetDC((HWND)Win32WindowData.Handle);

    // Set pixel format for the device
    Win32SetPixelFormat(/*Data,*/ DeviceContext);

    // Create modern OpenGL context
    bool ModernContext = true;
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

    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(GLDebugMessageCallback, 0);
    GLuint UnusedIDs = 0;
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, &UnusedIDs, GL_TRUE);

    // Create default viewport
    Viewport = calloc(1, sizeof(struct AxViewport));
    ContextInitialized = true;
}

static void AxDestroyContext(void)
{
    AXON_ASSERT(RenderContext && "RenderContext is NULL in DestroyContext()!");
    wglMakeCurrent(0, 0);
    wglDeleteContext(RenderContext);
}

static struct AxOpenGLInfo AxGetInfo(bool ModernContext)
{
    AXON_ASSERT(ContextInitialized && "Context not initialized in GetInfo()!");

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

static struct AxViewport *AxCreateViewport(AxVec2 Position, AxVec2 Size)
{
    struct AxViewport *Result = calloc(1, sizeof(struct AxViewport));
    Result->Position = Position;
    Result->Size = Size;
    Result->Depth = (AxVec2) { 0.0f, 1.0f };
    Result->Scale = (AxVec2) { 1.0f, 1.0f };
    Result->IsActive = true;

    return(Result);
}

static void AxDestroyViewport(struct AxViewport *Viewport)
{
    SAFE_FREE(Viewport);
}

static void AxNewFrame(void)
{
    AXON_ASSERT(ContextInitialized && "Context not initialized in NewFrame()!");

    // Reset to default OpenGL state at start of frame
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);

    // Reset viewport to full window size
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glViewport(0, 0, viewport[2], viewport[3]);

    // Clear the entire window with the default background color
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

static void AxSetActiveViewport(struct AxViewport *Viewport)
{
    AXON_ASSERT(ContextInitialized && "Context not initialized in SetActiveViewport()!");
    AXON_ASSERT(Viewport && "Viewport is NULL in SetActiveViewport()!");

    if (Viewport->IsActive) {
        // Get current window height for Y-flip
        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        int32_t WindowHeight = viewport[3];

        // Calculate flipped Y coordinate
        int32_t FlippedY = WindowHeight - (Viewport->Position.Y + Viewport->Size.Y);

        // Set viewport transform
        glViewport(
            (GLint)Viewport->Position.X,
            (GLint)FlippedY,
            (GLsizei)(Viewport->Size.X * Viewport->Scale.X),
            (GLsizei)(Viewport->Size.Y * Viewport->Scale.Y)
        );

        // Enable and set scissor to match viewport exactly
        glEnable(GL_SCISSOR_TEST);
        glScissor(
            (GLint)Viewport->Position.X,
            (GLint)FlippedY,
            (GLsizei)(Viewport->Size.X * Viewport->Scale.X),
            (GLsizei)(Viewport->Size.Y * Viewport->Scale.Y)
        );

        // Set up depth testing for this viewport
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthRange(Viewport->Depth.Min, Viewport->Depth.Max);
        glClearDepth(Viewport->Depth.Max);

        // Clear just this viewport with its color
        glClearColor(
            Viewport->ClearColor.X,
            Viewport->ClearColor.Y,
            Viewport->ClearColor.Z,
            Viewport->ClearColor.W
        );
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
}

static void AxRender(struct AxViewport *Viewport, struct AxMesh *Mesh, struct AxShaderData *ShaderData)
{
    AXON_ASSERT(ContextInitialized && "Context not initialized in Render()!");
    AXON_ASSERT(Viewport && "Viewport is NULL in Render()!");
    AXON_ASSERT(Mesh && "Mesh is NULL in Render()!");
    AXON_ASSERT(ShaderData && "ShaderData is NULL in Render()!");

    // Save current OpenGL state
    GLint lastViewport[4];
    glGetIntegerv(GL_VIEWPORT, lastViewport);
    GLboolean lastScissorTest = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean lastDepthTest = glIsEnabled(GL_DEPTH_TEST);

    // Set up viewport and render
    AxSetActiveViewport(Viewport);
    glUseProgram(ShaderData->ShaderHandle);
    glBindVertexArray(Mesh->VAO);

    glDrawElements(
        GL_TRIANGLES,
        Mesh->IndexCount,
        GL_UNSIGNED_INT,
        0
    );

    // Restore previous OpenGL state
    glViewport(lastViewport[0], lastViewport[1], lastViewport[2], lastViewport[3]);
    if (!lastScissorTest) {
        glDisable(GL_SCISSOR_TEST);
    }
    if (!lastDepthTest) {
        glDisable(GL_DEPTH_TEST);
    }

    // Check for errors
    GLenum err;
    while((err = glGetError()) != GL_NO_ERROR) {
        printf("GL Error: %d\n", err);
    }
}

static void AxSwapBuffers(void)
{
    AXON_ASSERT(ContextInitialized && "Context not initialized in SwapBuffers()!");
    SwapBuffers(wglGetCurrentDC());
}

static uint32_t AxCreateProgram(const char *HeaderCode, const char *VertexCode, const char *FragmentCode)
{
    AXON_ASSERT(ContextInitialized && "Context not initialized in CreateProgram()!");
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

static void AxDestroyProgram(uint32_t ProgramID)
{
    AXON_ASSERT(ContextInitialized && "Context not initialized in DestroyProgram()!");
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

static bool AxGetAttributeLocations(const uint32_t ProgramID, struct AxShaderData *ShaderData)
{
    AXON_ASSERT(ContextInitialized && "Context not initialized in GetAttributeLocations()!");
    AXON_ASSERT(ProgramID && "ProgramID is 0 in GetAttributeLocations()!");
    AXON_ASSERT(ShaderData && "ShaderData is NULL in GetAttributeLocations()!");

    // TODO(mdeforge): Turn this into a SetAttrib function where the user passes in a string
    // and value and, part of what it does it call this, and the calls the second part of the function
    // (not present here) to actually set it. The calling code is responsible for this.

    ShaderData->ShaderHandle = ProgramID;
    glUseProgram(ProgramID);

    uint32_t Result = glGetError();
    AXON_ASSERT(!Result && "OpenGL Error in CreateProgram()!");

    ShaderData->AttribLocationModelMatrix = glGetUniformLocation(ShaderData->ShaderHandle, "model");
    if (ShaderData->AttribLocationModelMatrix < 0) {
        return (false);
    }

    ShaderData->AttribLocationViewMatrix = glGetUniformLocation(ShaderData->ShaderHandle, "view");
    if (ShaderData->AttribLocationViewMatrix < 0) {
        return (false);
    }

    ShaderData->AttribLocationProjectionMatrix = glGetUniformLocation(ShaderData->ShaderHandle, "projection");
    if (ShaderData->AttribLocationProjectionMatrix < 0) {
        return (false);
    }

    ShaderData->AttribLocationLightPos = glGetUniformLocation(ShaderData->ShaderHandle, "LightPos");
    if (ShaderData->AttribLocationLightPos < 0) {
        printf("Warning: Could not find uniform 'LightPos'. Location: %d\n", 
               ShaderData->AttribLocationLightPos);
        // Don't return false here as this is now an expected uniform
    }

    ShaderData->AttribLocationLightColor = glGetUniformLocation(ShaderData->ShaderHandle, "LightColor");
    if (ShaderData->AttribLocationLightColor < 0) {
        printf("Warning: Could not find uniform 'LightColor'. Location: %d\n",
               ShaderData->AttribLocationLightColor);
        // Don't return false here as this is now an expected uniform
    }

    ShaderData->AttribLocationColor = glGetUniformLocation(ShaderData->ShaderHandle, "color");
    if (ShaderData->AttribLocationColor < 0) {
        return (false);
    }

    ShaderData->AttribLocationVertexPos = glGetAttribLocation(ShaderData->ShaderHandle, "Position");
    if (ShaderData->AttribLocationVertexPos < 0) {
        return (false);
    }

    ShaderData->AttribLocationVertexUV = glGetAttribLocation(ShaderData->ShaderHandle, "TexCoord");
    if (ShaderData->AttribLocationVertexUV < 0) {
        return (false);
    }

    ShaderData->AttribLocationTexture = glGetUniformLocation(ShaderData->ShaderHandle, "Texture");
    if (ShaderData->AttribLocationTexture < 0) {
        printf("Warning: Could not find uniform 'Texture'. Location: %d\n",
               ShaderData->AttribLocationTexture);
        // Don't return false here as this is now an expected uniform
    }

    // ShaderData->AttribLocationVertexColor = glGetAttribLocation(ShaderData->ShaderHandle, "Color");
    // if (ShaderData->AttribLocationVertexColor < 0) {
    //     return (false);
    // }

    Result = glGetError();
    AXON_ASSERT(!Result && "OpenGL Error in CreateProgram()!");

    return true;
}

static void AxSetUniform(struct AxShaderData *ShaderData, const char *UniformName, const void *Value)
{
    AXON_ASSERT(ContextInitialized && "Context not initialized in SetUniform()!");
    AXON_ASSERT(ShaderData && "ShaderData is NULL in SetUniform()!");

    if (strcmp(UniformName, "projection") == 0) {
        if (ShaderData->AttribLocationProjectionMatrix != -1) {
            glProgramUniformMatrix4fv(ShaderData->ShaderHandle, ShaderData->AttribLocationProjectionMatrix, 1, GL_FALSE, (const GLfloat *)Value);
        }
    } else if (strcmp(UniformName, "view") == 0) {
        if (ShaderData->AttribLocationViewMatrix != -1) {
            glProgramUniformMatrix4fv(ShaderData->ShaderHandle, ShaderData->AttribLocationViewMatrix, 1, GL_FALSE, (const GLfloat *)Value);
        }
    } else if (strcmp(UniformName, "model") == 0) {
        if (ShaderData->AttribLocationModelMatrix != -1) {
            glProgramUniformMatrix4fv(ShaderData->ShaderHandle, ShaderData->AttribLocationModelMatrix, 1, GL_FALSE, (const GLfloat *)Value);
        }
    } else if (strcmp(UniformName, "color") == 0) {
        if (ShaderData->AttribLocationColor != -1) {
            glProgramUniform3fv(ShaderData->ShaderHandle, ShaderData->AttribLocationColor, 1, &((AxVec3*)Value)->X);
        }
    } else if (strcmp(UniformName, "LightPos") == 0) {
        if (ShaderData->AttribLocationLightPos != -1) {
            glProgramUniform3fv(ShaderData->ShaderHandle, ShaderData->AttribLocationLightPos, 1, &((AxVec3*)Value)->X);
        }
    } else if (strcmp(UniformName, "LightColor") == 0) {
        if (ShaderData->AttribLocationLightColor != -1) {
            glProgramUniform3fv(ShaderData->ShaderHandle, ShaderData->AttribLocationLightColor, 1, &((AxVec3*)Value)->X);
        }
    } else if (strcmp(UniformName, "Texture") == 0) {
        if (ShaderData->AttribLocationTexture != -1) {
            glProgramUniform1i(ShaderData->ShaderHandle, ShaderData->AttribLocationTexture, 0);
        }
    } else if (strcmp(UniformName, "TexCoord") == 0) {
        if (ShaderData->AttribLocationVertexUV != -1) {
            glProgramUniform2fv(ShaderData->ShaderHandle, ShaderData->AttribLocationVertexUV, 1, &((AxVec2*)Value)->X);
        }
    }

    uint32_t Result = glGetError();
    AXON_ASSERT(!Result && "OpenGL Error in CreateProgram()!");
}

static void AxInitTexture(AxTexture *Texture, uint8_t *Pixels)
{
    AXON_ASSERT(ContextInitialized && "Context not initialized in CreateTexture()!");
    AXON_ASSERT(Texture && "Texture is NULL in CreateTexture()!");

    AXON_ASSERT(Texture->Width > 0 && Texture->Height > 0 && "Texture dimensions are invalid in CreateTexture()!");
    AXON_ASSERT(Pixels && "Pixels are NULL in CreateTexture()!");

    // Generate a new texture ID
    glCreateTextures(GL_TEXTURE_2D, 1, &Texture->ID);
    AXON_ASSERT(Texture->ID > 0 && "Texture ID is invalid in CreateTexture()!");

    // Determine the format based on the number of channels
    GLenum InternalFormat = 0;
    GLenum DataFormat = 0;
    switch(Texture->Channels) {
        case 1:
            InternalFormat = GL_R8;
            DataFormat = GL_RED;
            break;
        case 3:
            InternalFormat = GL_RGB8;
            DataFormat = GL_RGB;
            break;
        case 4:
            InternalFormat = GL_RGBA8;
            DataFormat = GL_RGBA;
            break;
        default:
            AXON_ASSERT(false && "Unsupported number of channels in CreateTexture()!");
    }

    AXON_ASSERT(InternalFormat > 0 && DataFormat > 0 && "Format not supported inCreateTexture()!");

    // Load the texture data
    const uint32_t Levels = (uint32_t)Min(5, Log2(Max(Texture->Width, Texture->Height)));
    glTextureStorage2D(Texture->ID, Levels, InternalFormat, Texture->Width, Texture->Height);
    glTextureSubImage2D(Texture->ID, 0, 0, 0, Texture->Width, Texture->Height, DataFormat, GL_UNSIGNED_BYTE, Pixels);

    // Wrap
    glTextureParameteri(Texture->ID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(Texture->ID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Filter mag
    glTextureParameteri(Texture->ID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Filter min
    glTextureParameteri(Texture->ID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    // Mip
    glTextureParameteri(Texture->ID, GL_TEXTURE_BASE_LEVEL, 0);
    glTextureParameteri(Texture->ID, GL_TEXTURE_MAX_LEVEL, Levels - 1);
    glGenerateTextureMipmap(Texture->ID);
}

static void AxDestroyTexture(AxTexture *Texture)
{
    AXON_ASSERT(ContextInitialized && "Context not initialized in DestroyTexture()!");
    AXON_ASSERT(Texture && "Texture is NULL in DestroyTexture()!");

    glDeleteTextures(1, &Texture->ID);
}

static void AxSetTextureData(AxTexture *Texture, uint8_t *Pixels)
{
    AXON_ASSERT(ContextInitialized && "Context not initialized in SetData()!");
    AXON_ASSERT(Texture && "Texture is NULL in SetData()!");
    AXON_ASSERT(Pixels && "Pixels are NULL in SetData()!");

    glTextureSubImage2D(Texture->ID, 0, 0, 0, Texture->Width, Texture->Height, Texture->Channels, GL_UNSIGNED_BYTE, Pixels);
}

static void AxBindTexture(AxTexture *Texture, uint32_t Slot)
{
    glBindTextureUnit(Slot, Texture->ID);
}

static void AxInitMesh(AxMesh *Mesh, struct AxVertex *Vertices, uint32_t *Indices, uint32_t VertexCount, uint32_t IndexCount)
{
    // Print first few vertices and indices for debugging
    for (uint32_t i = 0; i < min(VertexCount, 8); i++) {
        printf("V%u: (%f, %f, %f)\n", i,
               Vertices[i].Position.X,
               Vertices[i].Position.Y,
               Vertices[i].Position.Z);
    }

    // Create and bind VAO first
    GLuint VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // Create and bind VBO
    GLuint VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, VertexCount * sizeof(struct AxVertex), Vertices, GL_STATIC_DRAW);

    // Create and bind EBO
    GLuint EBO;
    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, IndexCount * sizeof(uint32_t), Indices, GL_STATIC_DRAW);

    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct AxVertex), (void*)offsetof(struct AxVertex, Position));

    // Normal attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(struct AxVertex), (void*)offsetof(struct AxVertex, Normal));

    // TexCoord attribute
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(struct AxVertex), (void*)offsetof(struct AxVertex, TexCoord));

    // Store handles in Mesh struct
    Mesh->VAO = VAO;
    Mesh->VBO = VBO;
    Mesh->EBO = EBO;
    Mesh->IndexCount = IndexCount;
}

// Use a compound literal to construct an unnamed object of API type in-place
struct AxOpenGLAPI *AxOpenGLAPI = &(struct AxOpenGLAPI) {
    .CreateContext = AxCreateContext,
    .DestroyContext = AxDestroyContext,
    .GetInfo = AxGetInfo,
    .CreateViewport = AxCreateViewport,
    .DestroyViewport = AxDestroyViewport,
    .SetActiveViewport = AxSetActiveViewport,
    .NewFrame = AxNewFrame,
    .Render = AxRender,
    .SwapBuffers = AxSwapBuffers,
    .CreateProgram = AxCreateProgram,
    .DestroyProgram = AxDestroyProgram,
    .GetAttributeLocations = AxGetAttributeLocations,
    .SetUniform = AxSetUniform,
    .InitTexture = AxInitTexture,
    .DestroyTexture = AxDestroyTexture,
    .SetTextureData = AxSetTextureData,
    .BindTexture = AxBindTexture,
    .InitMesh = AxInitMesh
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