#include <Windows.h>
#include <stdio.h>
#include <string.h>
#include "GL/gl3w.h"

#include "AxOpenGL.h"

//#include "AxWindow/AxWindow.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxMath.h"
#include "Foundation/AxPlatform.h"
#include "GL/glcorearb.h"

// Define GL constants that may not be available in core contexts
#ifndef GL_DEPTH_BITS
#define GL_DEPTH_BITS 0x0D56
#endif

#ifndef GL_TEXTURE_MAX_ANISOTROPY
#define GL_TEXTURE_MAX_ANISOTROPY 0x84FE
#endif

#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY
#define GL_MAX_TEXTURE_MAX_ANISOTROPY 0x84FF
#endif

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
AxViewport *Viewport;
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

static void SetupRenderState(int FramebufferWidth, int FramebufferHeight)
{
    AXON_ASSERT(ContextInitialized && "Context not initialized in SetupRenderState()!");

    // Enable alpha blending for transparent textures
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // Enable face culling to avoid rendering back faces
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Enable depth testing but disable depth writing for transparent objects
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    //glDisable(GL_STENCIL_TEST);
    //glEnable(GL_SCISSOR_TEST);
    //glDisable(GL_PRIMITIVE_RESTART);
    //glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // float AspectRatio = (float)WindowWidth / (float)WindowHeight;
    // AxMat4x4 ProjectionMatrix = CalcPerspectiveProjection(AspectRatio, 0.1f, 0.50f, 100.0f);

    // AxVec3 ViewDirection = { 0.0f, 0.0f, -1.0f };
    // AxVec3 Up = { 0.0f, 1.0f, 0.0f };
    // AxVec3 Position = { 0.0, 0.0f, -1.0f };
    // AxMat4x4 Transform = Identity();
    // Transform = Translate(Transform, OpenGL->Mesh.Transform.Position);

    // AxMat4x4 ViewMatrix = TransformLookAt(Position, Vec3Add(Position, ViewDirection), Up);
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

static bool CheckDSASupport()
{
    static bool DSA_Checked = false;
    static bool HasDSA = false;

    if (!DSA_Checked) {
        const char* Version = (const char*)glGetString(GL_VERSION);
        const char* Extensions = (const char*)glGetString(GL_EXTENSIONS);

        if (Version) {
            int Major = 0, Minor = 0;
            sscanf(Version, "%d.%d", &Major, &Minor);
            HasDSA = (Major > 4) || (Major == 4 && Minor >= 5);
        }

        if (!HasDSA && Extensions) {
            HasDSA = (strstr(Extensions, "ARB_direct_state_access") != NULL);
        }

        DSA_Checked = true;
        printf("Direct State Access support: %s\n", HasDSA ? "Yes" : "No");
    }

    return HasDSA;
}


///////////////////////////////////////////////////////////////
// Public API
///////////////////////////////////////////////////////////////
void AxCreateContext(uint64_t WindowHandle) // But then we'd need AX_LAST_KEY?
{
    //AXON_ASSERT(Window && "Window is NULL in CreateContext()!");

    // Trampoline the OpenGL 1.0 context to 4.6
    Win32LoadWGLExtensions();

    // Get device context for Window
    HDC DeviceContext = GetDC((HWND)WindowHandle);

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
        wglSwapIntervalEXT(1); // Disable vsync
    }

    // Load OpenGL Extensions
    gl3wInit();

    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(GLDebugMessageCallback, 0);
    GLuint UnusedIDs = 0;
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, &UnusedIDs, GL_TRUE);

    // Enable sRGB framebuffer for proper gamma correction
    // This will automatically convert linear color output to sRGB for display
    glEnable(GL_FRAMEBUFFER_SRGB);
    printf("sRGB framebuffer enabled for correct gamma correction\n");

    // Create default viewport
    Viewport = calloc(1, sizeof(AxViewport));
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

// Camera helper functions
static void RecomputeProjectionMatrix(AxCamera *Camera)
{
    if (!Camera) { return; }

    if (Camera->IsOrthographic) {
        // Calculate orthographic bounds based on zoom level and aspect ratio
        float HalfHeight = Camera->ZoomLevel * 0.5f;
        float HalfWidth = HalfHeight * Camera->AspectRatio;

        Camera->ProjectionMatrix = CalcOrthographicProjection(
            -HalfWidth, HalfWidth,    // Left, Right
            -HalfHeight, HalfHeight,  // Bottom, Top
            Camera->NearClipPlane,    // Near
            Camera->FarClipPlane      // Far
        );
    } else {
        // Perspective projection
        Camera->ProjectionMatrix = CalcPerspectiveProjection(
            Camera->FieldOfView,      // FOV in radians
            Camera->AspectRatio,      // Aspect ratio
            Camera->NearClipPlane,    // Near clip
            Camera->FarClipPlane      // Far clip
        );
    }
}

// Camera functions
static void CreateCamera(AxCamera *Camera)
{
    AXON_ASSERT(Camera);
    if (!Camera) { return; }
    memset(Camera, 0, sizeof(AxCamera));

    // Set default camera properties
    Camera->IsOrthographic = false;
    Camera->FieldOfView = AX_PI / 4.0f; // 45 degrees in radians
    Camera->ZoomLevel = 10.0f;          // Default orthographic zoom
    Camera->AspectRatio = 16.0f / 9.0f; // Default aspect ratio
    Camera->NearClipPlane = 0.01f;
    Camera->FarClipPlane = 1000.0f;

    // Initialize view matrix as identity (camera at origin looking down -Z)
    Camera->ViewMatrix = Identity();

    // Compute initial projection matrix
    RecomputeProjectionMatrix(Camera);
}

static bool CameraGetOrthographic(AxCamera *Camera)
{
    AXON_ASSERT(Camera);
    if (!Camera) { return (false); }
    return (Camera->IsOrthographic);
}

static void CameraSetOrthographic(AxCamera *Camera, bool IsOrthographic)
{
    AXON_ASSERT(Camera);
    if (!Camera) { return; }
    Camera->IsOrthographic = IsOrthographic;
    RecomputeProjectionMatrix(Camera);
}

static float CameraGetFOV(AxCamera *Camera)
{
    AXON_ASSERT(Camera);
    if (!Camera) { return (0.0f); }
    return (Camera->FieldOfView);
}

static void CameraSetFOV(AxCamera *Camera, float FOV)
{
    AXON_ASSERT(Camera);
    if (!Camera) { return; }
    Camera->FieldOfView = FOV;
    RecomputeProjectionMatrix(Camera);
}

static float CameraGetNearClipPlane(AxCamera *Camera)
{
    AXON_ASSERT(Camera);
    if (!Camera) { return (0.0f); }
    return (Camera->NearClipPlane);
}

static void CameraSetNearClipPlane(AxCamera *Camera, float NearClipPlane)
{
    AXON_ASSERT(Camera);
    if (!Camera) { return; }
    Camera->NearClipPlane = NearClipPlane;
    RecomputeProjectionMatrix(Camera);
}

static float CameraGetFarClipPlane(AxCamera *Camera)
{
    AXON_ASSERT(Camera);
    if (!Camera) { return (0.0f); }

    return (Camera->FarClipPlane);
}

static void CameraSetFarClipPlane(AxCamera *Camera, float FarClipPlane)
{
    AXON_ASSERT(Camera);
    if (!Camera) { return; }
    Camera->FarClipPlane = FarClipPlane;
    RecomputeProjectionMatrix(Camera);
}

static float CameraGetZoomLevel(AxCamera *Camera)
{
    AXON_ASSERT(Camera);
    if (!Camera) { return (0.0f); }
    return (Camera->ZoomLevel);
}

static void CameraSetZoomLevel(AxCamera *Camera, float ZoomLevel)
{
    AXON_ASSERT(Camera);
    if (!Camera) { return; }

    Camera->ZoomLevel = ZoomLevel;
    RecomputeProjectionMatrix(Camera);
}

static float CameraGetAspectRatio(AxCamera *Camera)
{
    AXON_ASSERT(Camera);
    if (!Camera) { return (0.0f); }

    return (Camera->AspectRatio);
}

static void CameraSetAspectRatio(AxCamera *Camera, float AspectRatio)
{
    AXON_ASSERT(Camera);
    if (!Camera) { return; }
    Camera->AspectRatio = AspectRatio;
    RecomputeProjectionMatrix(Camera);
}

static struct AxMat4x4 CameraGetProjectionMatrix(AxCamera *Camera)
{
    AXON_ASSERT(Camera);
    if (!Camera) { return (struct AxMat4x4) { 0 }; }

    return (Camera->ProjectionMatrix);
}

static AxViewport *AxCreateViewport(AxVec2 Position, AxVec2 Size)
{
    AxViewport *Result = calloc(1, sizeof(AxViewport));
    Result->Position = Position;
    Result->Size = Size;
    Result->Depth = (AxVec2) { 0.0f, 1.0f };
    Result->Scale = (AxVec2) { 1.0f, 1.0f };
    Result->IsActive = true;

    return(Result);
}

static void AxDestroyViewport(AxViewport *Viewport)
{
    SAFE_FREE(Viewport);
}

static void AxNewFrame(void)
{
    AXON_ASSERT(ContextInitialized && "Context not initialized in NewFrame()!");

    // Reset to default OpenGL state at start of frame
    glDisable(GL_SCISSOR_TEST);

    // Reset viewport to full window size
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glViewport(0, 0, viewport[2], viewport[3]);

    // Clear the entire window with the default background color
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

static void AxSetActiveViewport(AxViewport *Viewport)
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

static void AxRender(AxViewport *Viewport, struct AxMesh *Mesh, struct AxShaderData *ShaderData)
{
    AXON_ASSERT(ContextInitialized && "Context not initialized in Render()!");
    AXON_ASSERT(Viewport && "Viewport is NULL in Render()!");
    AXON_ASSERT(Mesh && "Mesh is NULL in Render()!");
    AXON_ASSERT(ShaderData && "ShaderData is NULL in Render()!");

    // Set up render state for alpha blending
    SetupRenderState(0, 0); // TODO(mdeforge): This needs rethinking

    glUseProgram(ShaderData->ShaderHandle);
    glBindVertexArray(Mesh->VAO);

    glDrawElements(
        GL_TRIANGLES,
        Mesh->IndexCount,
        GL_UNSIGNED_INT,
        0
    );

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

    ShaderData->ShaderHandle = ProgramID;
    glUseProgram(ProgramID);

    uint32_t Result = glGetError();
    AXON_ASSERT(!Result && "OpenGL Error in CreateProgram()!");

    // Required uniforms - return false if missing
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

    // Required vertex attributes - return false if missing
    ShaderData->AttribLocationVertexPos = glGetAttribLocation(ShaderData->ShaderHandle, "position");
    if (ShaderData->AttribLocationVertexPos < 0) {
        return (false);
    }

    ShaderData->AttribLocationVertexNormal = glGetAttribLocation(ShaderData->ShaderHandle, "normal");
    if (ShaderData->AttribLocationVertexNormal < 0) {
        return (false);
    }

    ShaderData->AttribLocationVertexUV = glGetAttribLocation(ShaderData->ShaderHandle, "texCoord");
    if (ShaderData->AttribLocationVertexUV < 0) {
        return (false);
    }

    ShaderData->AttribLocationVertexTangent = glGetAttribLocation(ShaderData->ShaderHandle, "tangent");
    if (ShaderData->AttribLocationVertexTangent < 0) {
        return (false);
    }

    // Optional legacy uniforms - silently ignore if not found (used by old non-PBR shaders)
    ShaderData->AttribLocationLightPos = glGetUniformLocation(ShaderData->ShaderHandle, "lightPos");
    ShaderData->AttribLocationLightColor = glGetUniformLocation(ShaderData->ShaderHandle, "lightColor");

    ShaderData->AttribLocationViewPos = glGetUniformLocation(ShaderData->ShaderHandle, "viewPos");
    if (ShaderData->AttribLocationViewPos < 0) {
        printf("Warning: Could not find uniform 'viewPos'\n");
    }

    ShaderData->AttribLocationMaterialColor = glGetUniformLocation(ShaderData->ShaderHandle, "materialColor");
    if (ShaderData->AttribLocationMaterialColor < 0) {
        printf("Warning: Could not find uniform 'materialColor'\n");
    }

    ShaderData->AttribLocationDiffuseTexture = glGetUniformLocation(ShaderData->ShaderHandle, "diffuseTexture");
    if (ShaderData->AttribLocationDiffuseTexture < 0) {
        printf("Warning: Could not find uniform 'diffuseTexture'\n");
    }

    ShaderData->AttribLocationNormalTexture = glGetUniformLocation(ShaderData->ShaderHandle, "normalTexture");
    if (ShaderData->AttribLocationNormalTexture < 0) {
        printf("Warning: Could not find uniform 'normalTexture'\n");
    }

    ShaderData->AttribLocationUseDiffuseTexture = glGetUniformLocation(ShaderData->ShaderHandle, "useDiffuseTexture");
    if (ShaderData->AttribLocationUseDiffuseTexture < 0) {
        printf("Warning: Could not find uniform 'useDiffuseTexture'\n");
    }

    ShaderData->AttribLocationUseNormalTexture = glGetUniformLocation(ShaderData->ShaderHandle, "useNormalTexture");
    if (ShaderData->AttribLocationUseNormalTexture < 0) {
        printf("Warning: Could not find uniform 'useNormalTexture'\n");
    }

    // Alpha handling uniforms
    ShaderData->AttribLocationAlphaMode = glGetUniformLocation(ShaderData->ShaderHandle, "alphaMode");
    if (ShaderData->AttribLocationAlphaMode < 0) {
        printf("Warning: Could not find uniform 'alphaMode'\n");
    }

    ShaderData->AttribLocationAlphaCutoff = glGetUniformLocation(ShaderData->ShaderHandle, "alphaCutoff");
    if (ShaderData->AttribLocationAlphaCutoff < 0) {
        printf("Warning: Could not find uniform 'alphaCutoff'\n");
    }

    // Legacy uniforms (for backward compatibility) - optional
    ShaderData->AttribLocationColor = glGetUniformLocation(ShaderData->ShaderHandle, "color");
    if (ShaderData->AttribLocationColor < 0) {
        printf("Info: Could not find legacy uniform 'color'\n");
    }

    ShaderData->AttribLocationMaterialAlpha = glGetUniformLocation(ShaderData->ShaderHandle, "materialAlpha");
    if (ShaderData->AttribLocationMaterialAlpha < 0) {
        printf("Info: Could not find legacy uniform 'materialAlpha'\n");
    }

    ShaderData->AttribLocationHasNormalMap = glGetUniformLocation(ShaderData->ShaderHandle, "hasNormalMap");
    if (ShaderData->AttribLocationHasNormalMap < 0) {
        printf("Info: Could not find legacy uniform 'hasNormalMap'\n");
    }

    Result = glGetError();
    AXON_ASSERT(!Result && "OpenGL Error in GetAttributeLocations()!");

    printf("Shader attribute locations loaded successfully:\n");
    printf("  Required: model=%d, view=%d, projection=%d\n", 
           ShaderData->AttribLocationModelMatrix, 
           ShaderData->AttribLocationViewMatrix, 
           ShaderData->AttribLocationProjectionMatrix);
    printf("  Textures: diffuse=%d, normal=%d, useDiffuse=%d, useNormal=%d\n",
           ShaderData->AttribLocationDiffuseTexture,
           ShaderData->AttribLocationNormalTexture,
           ShaderData->AttribLocationUseDiffuseTexture,
           ShaderData->AttribLocationUseNormalTexture);

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
    } else if (strcmp(UniformName, "materialColor") == 0) {
        if (ShaderData->AttribLocationMaterialColor != -1) {
            glProgramUniform4fv(ShaderData->ShaderHandle, ShaderData->AttribLocationMaterialColor, 1, &((AxVec4*)Value)->X);
        }
    } else if (strcmp(UniformName, "lightPos") == 0) {
        if (ShaderData->AttribLocationLightPos != -1) {
            glProgramUniform3fv(ShaderData->ShaderHandle, ShaderData->AttribLocationLightPos, 1, &((AxVec3*)Value)->X);
        }
    } else if (strcmp(UniformName, "lightColor") == 0) {
        if (ShaderData->AttribLocationLightColor != -1) {
            glProgramUniform3fv(ShaderData->ShaderHandle, ShaderData->AttribLocationLightColor, 1, &((AxVec3*)Value)->X);
        }
    } else if (strcmp(UniformName, "viewPos") == 0) {
        if (ShaderData->AttribLocationViewPos != -1) {
            glProgramUniform3fv(ShaderData->ShaderHandle, ShaderData->AttribLocationViewPos, 1, &((AxVec3*)Value)->X);
        }
    } else if (strcmp(UniformName, "diffuseTexture") == 0) {
        if (ShaderData->AttribLocationDiffuseTexture != -1) {
            glProgramUniform1i(ShaderData->ShaderHandle, ShaderData->AttribLocationDiffuseTexture, *(const int*)Value);
        }
    } else if (strcmp(UniformName, "normalTexture") == 0) {
        if (ShaderData->AttribLocationNormalTexture != -1) {
            glProgramUniform1i(ShaderData->ShaderHandle, ShaderData->AttribLocationNormalTexture, *(const int*)Value);
        }
    } else if (strcmp(UniformName, "useDiffuseTexture") == 0) {
        if (ShaderData->AttribLocationUseDiffuseTexture != -1) {
            glProgramUniform1i(ShaderData->ShaderHandle, ShaderData->AttribLocationUseDiffuseTexture, *(const int*)Value);
        }
    } else if (strcmp(UniformName, "useNormalTexture") == 0) {
        if (ShaderData->AttribLocationUseNormalTexture != -1) {
            glProgramUniform1i(ShaderData->ShaderHandle, ShaderData->AttribLocationUseNormalTexture, *(const int*)Value);
        }
    } else if (strcmp(UniformName, "alphaMode") == 0) {
        if (ShaderData->AttribLocationAlphaMode != -1) {
            glProgramUniform1i(ShaderData->ShaderHandle, ShaderData->AttribLocationAlphaMode, *(const int*)Value);
        }
    } else if (strcmp(UniformName, "alphaCutoff") == 0) {
        if (ShaderData->AttribLocationAlphaCutoff != -1) {
            glProgramUniform1f(ShaderData->ShaderHandle, ShaderData->AttribLocationAlphaCutoff, *(const float*)Value);
        }
    } else {
        printf("Warning: Unknown uniform '%s' in SetUniform()\n", UniformName);
    }

    uint32_t Result = glGetError();
    if (Result != GL_NO_ERROR) {
        printf("OpenGL Error in SetUniform('%s'): %u\n", UniformName, Result);
    }
}

// New PBR material uniform setter - sets all PBR material + lighting uniforms in one call
// This replaces individual SetUniform calls for materials, maintaining performance while
// decoupling material data from AxShaderData structure per the hybrid material system spec
static void AxSetPBRMaterialUniforms(struct AxShaderData* ShaderData, const AxPBRMaterial* Material,
                                      const AxLight* Lights, int32_t LightCount)
{
    AXON_ASSERT(ContextInitialized && "Context not initialized!");
    AXON_ASSERT(ShaderData && "ShaderData is NULL!");
    AXON_ASSERT(Material && "Material is NULL!");

    uint32_t ShaderHandle = ShaderData->ShaderHandle;

    // Helper macros to get uniform location and set if it exists
    #define GET_UNIFORM_LOC(name) glGetUniformLocation(ShaderHandle, name)
    #define SET_UNIFORM_IF_EXISTS(loc, setter) if (loc != -1) { setter; }

    // Set PBR material properties
    GLint loc = GET_UNIFORM_LOC("materialColor");
    SET_UNIFORM_IF_EXISTS(loc, glProgramUniform4fv(ShaderHandle, loc, 1, &Material->BaseColorFactor.X));

    loc = GET_UNIFORM_LOC("metallicFactor");
    SET_UNIFORM_IF_EXISTS(loc, glProgramUniform1f(ShaderHandle, loc, Material->MetallicFactor));

    loc = GET_UNIFORM_LOC("roughnessFactor");
    SET_UNIFORM_IF_EXISTS(loc, glProgramUniform1f(ShaderHandle, loc, Material->RoughnessFactor));

    loc = GET_UNIFORM_LOC("emissiveFactor");
    SET_UNIFORM_IF_EXISTS(loc, glProgramUniform3fv(ShaderHandle, loc, 1, &Material->EmissiveFactor.X));

    // Set texture usage flags
    int32_t useDiffuse = (Material->BaseColorTexture >= 0) ? 1 : 0;
    loc = GET_UNIFORM_LOC("useDiffuseTexture");
    SET_UNIFORM_IF_EXISTS(loc, glProgramUniform1i(ShaderHandle, loc, useDiffuse));

    int32_t useNormal = (Material->NormalTexture >= 0) ? 1 : 0;
    loc = GET_UNIFORM_LOC("useNormalTexture");
    SET_UNIFORM_IF_EXISTS(loc, glProgramUniform1i(ShaderHandle, loc, useNormal));

    int32_t useMetallicRoughness = (Material->MetallicRoughnessTexture >= 0) ? 1 : 0;
    loc = GET_UNIFORM_LOC("useMetallicRoughnessTexture");
    SET_UNIFORM_IF_EXISTS(loc, glProgramUniform1i(ShaderHandle, loc, useMetallicRoughness));

    int32_t useEmissive = (Material->EmissiveTexture >= 0) ? 1 : 0;
    loc = GET_UNIFORM_LOC("useEmissiveTexture");
    SET_UNIFORM_IF_EXISTS(loc, glProgramUniform1i(ShaderHandle, loc, useEmissive));

    int32_t useOcclusion = (Material->OcclusionTexture >= 0) ? 1 : 0;
    loc = GET_UNIFORM_LOC("useOcclusionTexture");
    SET_UNIFORM_IF_EXISTS(loc, glProgramUniform1i(ShaderHandle, loc, useOcclusion));

    // Set alpha mode and cutoff
    loc = GET_UNIFORM_LOC("alphaMode");
    SET_UNIFORM_IF_EXISTS(loc, glProgramUniform1i(ShaderHandle, loc, (int32_t)Material->AlphaMode));

    loc = GET_UNIFORM_LOC("alphaCutoff");
    SET_UNIFORM_IF_EXISTS(loc, glProgramUniform1f(ShaderHandle, loc, Material->AlphaCutoff));

    // Set texture sampler slots (which texture unit each sampler should read from)
    loc = GET_UNIFORM_LOC("diffuseTexture");
    SET_UNIFORM_IF_EXISTS(loc, glProgramUniform1i(ShaderHandle, loc, 0));  // Texture unit 0

    loc = GET_UNIFORM_LOC("normalTexture");
    SET_UNIFORM_IF_EXISTS(loc, glProgramUniform1i(ShaderHandle, loc, 1));  // Texture unit 1

    loc = GET_UNIFORM_LOC("metallicRoughnessTexture");
    SET_UNIFORM_IF_EXISTS(loc, glProgramUniform1i(ShaderHandle, loc, 2));  // Texture unit 2

    loc = GET_UNIFORM_LOC("emissiveTexture");
    SET_UNIFORM_IF_EXISTS(loc, glProgramUniform1i(ShaderHandle, loc, 3));  // Texture unit 3

    loc = GET_UNIFORM_LOC("occlusionTexture");
    SET_UNIFORM_IF_EXISTS(loc, glProgramUniform1i(ShaderHandle, loc, 4));  // Texture unit 4

    // Set light count
    int32_t ClampedLightCount = (LightCount > 8) ? 8 : LightCount;
    loc = GET_UNIFORM_LOC("lightCount");
    SET_UNIFORM_IF_EXISTS(loc, glProgramUniform1i(ShaderHandle, loc, ClampedLightCount));

    // Set light array uniforms only for lights that exist
    // This avoids setting uniforms that have been optimized out by the shader compiler
    if (Lights) {
        for (int32_t i = 0; i < ClampedLightCount; i++) {
            char uniformName[64];
            const AxLight* Light = &Lights[i];

            snprintf(uniformName, sizeof(uniformName), "lightPositions[%d]", i);
            loc = GET_UNIFORM_LOC(uniformName);
            SET_UNIFORM_IF_EXISTS(loc, glProgramUniform3fv(ShaderHandle, loc, 1, &Light->Position.X));

            snprintf(uniformName, sizeof(uniformName), "lightColors[%d]", i);
            loc = GET_UNIFORM_LOC(uniformName);
            SET_UNIFORM_IF_EXISTS(loc, glProgramUniform3fv(ShaderHandle, loc, 1, &Light->Color.X));

            snprintf(uniformName, sizeof(uniformName), "lightIntensities[%d]", i);
            loc = GET_UNIFORM_LOC(uniformName);
            SET_UNIFORM_IF_EXISTS(loc, glProgramUniform1f(ShaderHandle, loc, Light->Intensity));

            snprintf(uniformName, sizeof(uniformName), "lightRanges[%d]", i);
            loc = GET_UNIFORM_LOC(uniformName);
            SET_UNIFORM_IF_EXISTS(loc, glProgramUniform1f(ShaderHandle, loc, Light->Range));
        }
    }

    #undef GET_UNIFORM_LOC
    #undef SET_UNIFORM_IF_EXISTS

    uint32_t Result = glGetError();
    if (Result != GL_NO_ERROR) {
        printf("OpenGL Error in AxSetPBRMaterialUniforms: %u\n", Result);
    }
}

static void AxInitTexture(AxTexture *Texture, uint8_t *Pixels)
{
    AXON_ASSERT(ContextInitialized && "Context not initialized in InitTexture()!");
    AXON_ASSERT(Texture && "Texture is NULL in InitTexture()!");

    GLenum internalFormat = GL_RGB;
    GLenum format = GL_RGB;

    // Handle sRGB vs linear color spaces
    if (Texture->Channels == 4) {
        format = GL_RGBA;
        internalFormat = Texture->IsSRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    } else if (Texture->Channels == 3) {
        format = GL_RGB;
        internalFormat = Texture->IsSRGB ? GL_SRGB8 : GL_RGB8;
    } else if (Texture->Channels == 1) {
        format = GL_RED;
        internalFormat = GL_R8; // Single channel data is always linear
    } else {
        AXON_ASSERT(false && "Unsupported texture channel count!");
        return;
    }

    bool useDSA = CheckDSASupport();

    if (useDSA && glCreateTextures != NULL && glTextureStorage2D != NULL && glTextureSubImage2D != NULL) {
        printf("Using DSA path for texture creation\n");

        // DSA path: create texture
        glCreateTextures(GL_TEXTURE_2D, 1, &Texture->ID);
        uint32_t err = glGetError();
        if (err != GL_NO_ERROR) {
            printf("Error after glCreateTextures: 0x%X\n", err);
            // Fall back to legacy if DSA fails
            useDSA = false;
        } else {
            // Calculate number of mip levels needed
            // Formula: floor(log2(max(width, height))) + 1
            uint32_t maxDim = Texture->Width > Texture->Height ? Texture->Width : Texture->Height;
            uint32_t Levels = 1;
            while (maxDim > 1) {
                maxDim >>= 1;
                Levels++;
            }
            printf("Allocating %u mip levels for %ux%u texture\n", Levels, Texture->Width, Texture->Height);

            // Allocate immutable storage with mipmap levels
            glTextureStorage2D(Texture->ID, Levels, internalFormat, Texture->Width, Texture->Height);
            err = glGetError();
            if (err != GL_NO_ERROR) {
                printf("Error after glTextureStorage2D: 0x%X (internalFormat: 0x%X)\n", err, internalFormat);
                useDSA = false;
            }

            if (useDSA) {
                // Upload pixel data
                glTextureSubImage2D(Texture->ID, 0, 0, 0, Texture->Width, Texture->Height, format, GL_UNSIGNED_BYTE, Pixels);
                err = glGetError();
                if (err != GL_NO_ERROR) {
                    printf("Error after glTextureSubImage2D: 0x%X\n", err);
                }

                // Generate mipmaps
                glGenerateTextureMipmap(Texture->ID);
                err = glGetError();
                if (err != GL_NO_ERROR) {
                    printf("Error after glGenerateTextureMipmap: 0x%X\n", err);
                }

                // Set default texture parameters using DSA
                glTextureParameteri(Texture->ID, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTextureParameteri(Texture->ID, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glTextureParameteri(Texture->ID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTextureParameteri(Texture->ID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                // Enable anisotropic filtering for better quality at oblique angles
                GLfloat maxAnisotropy = 0.0f;
                glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAnisotropy);
                if (maxAnisotropy > 1.0f) {
                    glTextureParameterf(Texture->ID, GL_TEXTURE_MAX_ANISOTROPY, maxAnisotropy);
                    printf("Anisotropic filtering enabled: %.1fx\n", maxAnisotropy);
                }

                printf("Created texture ID %u: %dx%d, %d channels, %s (DSA with mipmaps)\n",
                       Texture->ID, Texture->Width, Texture->Height, Texture->Channels,
                       Texture->IsSRGB ? "sRGB" : "Linear");
            }
        }
    }

    if (!useDSA) {
        printf("Using legacy path for texture creation\n");

        // Legacy OpenGL path
        glGenTextures(1, &Texture->ID);
        uint32_t err = glGetError();
        if (err != GL_NO_ERROR) {
            printf("Error after glGenTextures: 0x%X\n", err);
        }

        GLint currentTexture;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &currentTexture);

        glBindTexture(GL_TEXTURE_2D, Texture->ID);
        err = glGetError();
        if (err != GL_NO_ERROR) {
            printf("Error after glBindTexture: 0x%X\n", err);
        }

        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat,
                     Texture->Width, Texture->Height, 0,
                     format, GL_UNSIGNED_BYTE, Pixels);
        err = glGetError();
        if (err != GL_NO_ERROR) {
            printf("Error after glTexImage2D: 0x%X (internalFormat: 0x%X, format: 0x%X)\n",
                   err, internalFormat, format);
        }

        // Generate mipmaps
        glGenerateMipmap(GL_TEXTURE_2D);
        err = glGetError();
        if (err != GL_NO_ERROR) {
            printf("Error after glGenerateMipmap: 0x%X\n", err);
        }

        // Set default texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Enable anisotropic filtering for better quality at oblique angles
        GLfloat maxAnisotropy = 0.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAnisotropy);
        if (maxAnisotropy > 1.0f) {
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, maxAnisotropy);
            printf("Anisotropic filtering enabled: %.1fx\n", maxAnisotropy);
        }

        // Restore previous texture binding
        glBindTexture(GL_TEXTURE_2D, currentTexture);

        printf("Created texture ID %u: %dx%d, %d channels, %s (Legacy with mipmaps)\n",
               Texture->ID, Texture->Width, Texture->Height, Texture->Channels,
               Texture->IsSRGB ? "sRGB" : "Linear");
    }

    uint32_t Result = glGetError();
    if (Result != GL_NO_ERROR) {
        printf("OpenGL Error in InitTexture(): %u\n", Result);
        AXON_ASSERT(!Result && "OpenGL Error in InitTexture()!");
    }
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

    // Determine the format based on the number of channels
    GLenum DataFormat = 0;
    switch(Texture->Channels) {
        case 1:
            DataFormat = GL_RED;
            break;
        case 3:
            DataFormat = GL_RGB;
            break;
        case 4:
            DataFormat = GL_RGBA;
            break;
        default:
            AXON_ASSERT(false && "Unsupported number of channels in SetTextureData()!");
            return;
    }

    glTextureSubImage2D(Texture->ID, 0, 0, 0, Texture->Width, Texture->Height, DataFormat, GL_UNSIGNED_BYTE, Pixels);
}

static void AxBindTexture(AxTexture *Texture, uint32_t Slot)
{
    AXON_ASSERT(Texture && "Texture is NULL in BindTexture()!");
    AXON_ASSERT(Texture->ID > 0 && "Invalid texture ID in BindTexture()!");

    if (CheckDSASupport() && glBindTextureUnit != NULL) {
        glBindTextureUnit(Slot, Texture->ID);
    } else {
        // Legacy path: activate texture unit and bind texture
        glActiveTexture(GL_TEXTURE0 + Slot);
        glBindTexture(GL_TEXTURE_2D, Texture->ID);
    }

    uint32_t err = glGetError();
    if (err != GL_NO_ERROR) {
        printf("OpenGL Error in BindTexture(): 0x%X (Texture ID: %u, Slot: %u)\n",
               err, Texture->ID, Slot);
    }
}

static void AxInitMesh(AxMesh *Mesh, struct AxVertex *Vertices, uint32_t *Indices, uint32_t VertexCount, uint32_t IndexCount)
{
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

    // Tangent attribute (vec4: XYZ = tangent vector, W = handedness)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(struct AxVertex), (void*)offsetof(struct AxVertex, Tangent));

    // Store handles in Mesh struct
    Mesh->VAO = VAO;
    Mesh->VBO = VBO;
    Mesh->EBO = EBO;
    Mesh->IndexCount = IndexCount;
}

static uint32_t AxBlendFunctionToGL(AxBlendFunction BlendFunc)
{
    switch (BlendFunc) {
        case AX_BLEND_ZERO: return GL_ZERO;
        case AX_BLEND_ONE: return GL_ONE;
        case AX_BLEND_SRC_COLOR: return GL_SRC_COLOR;
        case AX_BLEND_ONE_MINUS_SRC_COLOR: return GL_ONE_MINUS_SRC_COLOR;
        case AX_BLEND_DST_COLOR: return GL_DST_COLOR;
        case AX_BLEND_ONE_MINUS_DST_COLOR: return GL_ONE_MINUS_DST_COLOR;
        case AX_BLEND_SRC_ALPHA: return GL_SRC_ALPHA;
        case AX_BLEND_ONE_MINUS_SRC_ALPHA: return GL_ONE_MINUS_SRC_ALPHA;
        case AX_BLEND_DST_ALPHA: return GL_DST_ALPHA;
        case AX_BLEND_ONE_MINUS_DST_ALPHA: return GL_ONE_MINUS_DST_ALPHA;
        case AX_BLEND_CONSTANT_COLOR: return GL_CONSTANT_COLOR;
        case AX_BLEND_ONE_MINUS_CONSTANT_COLOR: return GL_ONE_MINUS_CONSTANT_COLOR;
        case AX_BLEND_CONSTANT_ALPHA: return GL_CONSTANT_ALPHA;
        case AX_BLEND_ONE_MINUS_CONSTANT_ALPHA: return GL_ONE_MINUS_CONSTANT_ALPHA;
        case AX_BLEND_SRC_ALPHA_SATURATE: return GL_SRC_ALPHA_SATURATE;
        default: return GL_ONE; // GL_ONE as fallback
    }
}

static void AxEnableBlending(bool Enable)
{
    AXON_ASSERT(ContextInitialized && "Context not initialized in EnableBlending()!");

    if (Enable) {
        glEnable(GL_BLEND);
    } else {
        glDisable(GL_BLEND);
    }
}

static void AxSetBlendFunction(AxBlendFunction SourceFactor, AxBlendFunction DestFactor)
{
    AXON_ASSERT(ContextInitialized && "Context not initialized in SetBlendFunction()!");

    uint32_t GLSourceFactor = AxBlendFunctionToGL(SourceFactor);
    uint32_t GLDestFactor = AxBlendFunctionToGL(DestFactor);

    glBlendFunc(GLSourceFactor, GLDestFactor);
}

static bool AxTextureHasAlpha(AxTexture *Texture)
{
    AXON_ASSERT(Texture && "Texture is NULL in TextureHasAlpha()!");
    return (Texture->Channels == 4);
}

static void AxSetDepthWrite(bool Enable)
{
    AXON_ASSERT(ContextInitialized && "Context not initialized in SetDepthWrite()!");
    glDepthMask(Enable ? GL_TRUE : GL_FALSE);
}

static void AxSetCullMode(bool Enable)
{
    AXON_ASSERT(ContextInitialized && "Context not initialized in SetCullMode()!");
    if (Enable) {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);  // Cull back faces
    } else {
        glDisable(GL_CULL_FACE);
    }
}

static uint32_t AxTextureWrapModeToGL(AxTextureWrapMode WrapMode)
{
    switch (WrapMode) {
        case AX_TEXTURE_WRAP_REPEAT: return GL_REPEAT;
        case AX_TEXTURE_WRAP_CLAMP_TO_EDGE: return GL_CLAMP_TO_EDGE;
        case AX_TEXTURE_WRAP_MIRRORED_REPEAT: return GL_MIRRORED_REPEAT;
        default: return GL_REPEAT; // GL_REPEAT as fallback
    }
}

static uint32_t AxTextureFilterToGL(AxTextureFilter Filter)
{
    switch (Filter) {
        case AX_TEXTURE_FILTER_NEAREST: return GL_NEAREST;
        case AX_TEXTURE_FILTER_LINEAR: return GL_LINEAR;
        case AX_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST: return GL_NEAREST_MIPMAP_NEAREST;
        case AX_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST: return GL_LINEAR_MIPMAP_NEAREST;
        case AX_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR: return GL_NEAREST_MIPMAP_LINEAR;
        case AX_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR: return GL_LINEAR_MIPMAP_LINEAR;
        default: return GL_LINEAR; // GL_LINEAR as fallback
    }
}

static uint32_t AxTextureParameterToGL(AxTextureParameter Parameter)
{
    switch (Parameter) {
        case AX_TEXTURE_WRAP_S: return GL_TEXTURE_WRAP_S;
        case AX_TEXTURE_WRAP_T: return GL_TEXTURE_WRAP_T;
        case AX_TEXTURE_MAG_FILTER: return GL_TEXTURE_MAG_FILTER;
        case AX_TEXTURE_MIN_FILTER: return GL_TEXTURE_MIN_FILTER;
        default: return GL_TEXTURE_WRAP_S; // GL_TEXTURE_WRAP_S as fallback
    }
}

static void AxSetTextureParameter(AxTexture *Texture, AxTextureParameter Parameter, int32_t Value)
{
    AXON_ASSERT(ContextInitialized && "Context not initialized in SetTextureParameter()!");
    AXON_ASSERT(Texture && "Texture is NULL in SetTextureParameter()!");
    AXON_ASSERT(Texture->ID > 0 && "Texture ID is invalid in SetTextureParameter()!");

    uint32_t GLParameter = AxTextureParameterToGL(Parameter);
    glTextureParameteri(Texture->ID, GLParameter, Value);

    // Update the texture struct based on parameter
    switch (Parameter) {
        case AX_TEXTURE_WRAP_S: Texture->WrapS = (uint32_t)Value; break;
        case AX_TEXTURE_WRAP_T: Texture->WrapT = (uint32_t)Value; break;
        case AX_TEXTURE_MAG_FILTER: Texture->MagFilter = (uint32_t)Value; break;
        case AX_TEXTURE_MIN_FILTER: Texture->MinFilter = (uint32_t)Value; break;
    }

    uint32_t Result = glGetError();
    AXON_ASSERT(!Result && "OpenGL Error in SetTextureParameter()!");
}

static void AxSetTextureWrapMode(AxTexture *Texture, AxTextureWrapMode WrapS, AxTextureWrapMode WrapT)
{
    AXON_ASSERT(ContextInitialized && "Context not initialized in SetTextureWrapMode()!");
    AXON_ASSERT(Texture && "Texture is NULL in SetTextureWrapMode()!");
    AXON_ASSERT(Texture->ID > 0 && "Texture ID is invalid in SetTextureWrapMode()!");

    printf("SetTextureWrapMode called: Texture ID %u, WrapS enum=%d, WrapT enum=%d\n",
           Texture->ID, WrapS, WrapT);

    uint32_t GLWrapS = AxTextureWrapModeToGL(WrapS);
    uint32_t GLWrapT = AxTextureWrapModeToGL(WrapT);

    printf("  Converted to GL: WrapS=0x%X, WrapT=0x%X\n", GLWrapS, GLWrapT);

    Texture->WrapS = GLWrapS;
    Texture->WrapT = GLWrapT;

    // Clear any previous errors
    while (glGetError() != GL_NO_ERROR);

    if (CheckDSASupport()) {
        printf("  Using DSA path\n");
        glTextureParameteri(Texture->ID, GL_TEXTURE_WRAP_S, GLWrapS);
        uint32_t err = glGetError();
        if (err != GL_NO_ERROR) {
            printf("  ERROR after glTextureParameteri(WRAP_S): 0x%X\n", err);
        }

        glTextureParameteri(Texture->ID, GL_TEXTURE_WRAP_T, GLWrapT);
        err = glGetError();
        if (err != GL_NO_ERROR) {
            printf("  ERROR after glTextureParameteri(WRAP_T): 0x%X\n", err);
        }
    } else {
        printf("  Using legacy binding path\n");
        GLint currentTexture;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &currentTexture);

        glBindTexture(GL_TEXTURE_2D, Texture->ID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GLWrapS);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GLWrapT);

        glBindTexture(GL_TEXTURE_2D, currentTexture);
    }

    uint32_t Result = glGetError();
    if (Result != GL_NO_ERROR) {
        const char* errorStr = "Unknown";
        switch (Result) {
            case GL_INVALID_ENUM: errorStr = "GL_INVALID_ENUM"; break;
            case GL_INVALID_VALUE: errorStr = "GL_INVALID_VALUE"; break;
            case GL_INVALID_OPERATION: errorStr = "GL_INVALID_OPERATION"; break;
        }
        printf("OpenGL Error in SetTextureWrapMode(): 0x%X (%s)\n", Result, errorStr);
        printf("  Texture ID: %u, WrapS: 0x%X, WrapT: 0x%X\n",
               Texture->ID, GLWrapS, GLWrapT);
        AXON_ASSERT(!Result && "OpenGL Error in SetTextureWrapMode()!");
    } else {
        printf("  SetTextureWrapMode completed successfully\n");
    }
}

static void AxSetTextureFilterMode(AxTexture *Texture, AxTextureFilter MagFilter, AxTextureFilter MinFilter)
{
    AXON_ASSERT(ContextInitialized && "Context not initialized in SetTextureFilterMode()!");
    AXON_ASSERT(Texture && "Texture is NULL in SetTextureFilterMode()!");
    AXON_ASSERT(Texture->ID > 0 && "Texture ID is invalid in SetTextureFilterMode()!");

    printf("Setting texture filter mode for texture ID %u:\n", Texture->ID);
    printf("  MagFilter: %d, MinFilter: %d\n", MagFilter, MinFilter);

    Texture->MagFilter = AxTextureFilterToGL(MagFilter);
    Texture->MinFilter = AxTextureFilterToGL(MinFilter);

    printf("  Converted to GL: Mag=%d, Min=%d\n", Texture->MagFilter, Texture->MinFilter);

    // Clear any previous errors
    while (glGetError() != GL_NO_ERROR);

    if (CheckDSASupport()) {
        printf("  Using DSA...\n");
        glTextureParameteri(Texture->ID, GL_TEXTURE_MAG_FILTER, Texture->MagFilter);
        glTextureParameteri(Texture->ID, GL_TEXTURE_MIN_FILTER, Texture->MinFilter);
    } else {
        printf("  Using traditional binding...\n");
        GLint currentTexture;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &currentTexture);

        glBindTexture(GL_TEXTURE_2D, Texture->ID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, Texture->MagFilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, Texture->MinFilter);

        glBindTexture(GL_TEXTURE_2D, currentTexture);
    }

    uint32_t Result = glGetError();
    if (Result != GL_NO_ERROR) {
        const char* errorStr = "Unknown";
        switch (Result) {
            case GL_INVALID_ENUM: errorStr = "GL_INVALID_ENUM"; break;
            case GL_INVALID_VALUE: errorStr = "GL_INVALID_VALUE"; break;
            case GL_INVALID_OPERATION: errorStr = "GL_INVALID_OPERATION"; break;
            case GL_INVALID_FRAMEBUFFER_OPERATION: errorStr = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
            case GL_OUT_OF_MEMORY: errorStr = "GL_OUT_OF_MEMORY"; break;
        }
        printf("OpenGL Error in SetTextureFilterMode(): %u (%s)\n", Result, errorStr);
        printf("  Texture ID: %u, DSA: %s\n", Texture->ID, CheckDSASupport() ? "Yes" : "No");
        printf("  MagFilter GL enum: %d, MinFilter GL enum: %d\n", Texture->MagFilter, Texture->MinFilter);
        AXON_ASSERT(!Result && "OpenGL Error in SetTextureFilterMode()!");
    } else {
        printf("  Texture filter set successfully\n");
    }
}

// Use a compound literal to construct an unnamed object of API type in-place
struct AxOpenGLAPI *AxOpenGLAPI = &(struct AxOpenGLAPI) {
    .CreateContext = AxCreateContext,
    .DestroyContext = AxDestroyContext,
    .GetInfo = AxGetInfo,
    .CreateCamera = CreateCamera,
    .CameraGetOrthographic = CameraGetOrthographic,
    .CameraSetOrthographic = CameraSetOrthographic,
    .CameraGetFOV = CameraGetFOV,
    .CameraSetFOV = CameraSetFOV,
    .CameraGetNearClipPlane = CameraGetNearClipPlane,
    .CameraSetNearClipPlane = CameraSetNearClipPlane,
    .CameraGetFarClipPlane = CameraGetFarClipPlane,
    .CameraSetFarClipPlane = CameraSetFarClipPlane,
    .CameraGetZoomLevel = CameraGetZoomLevel,
    .CameraSetZoomLevel = CameraSetZoomLevel,
    .CameraGetAspectRatio = CameraGetAspectRatio,
    .CameraSetAspectRatio = CameraSetAspectRatio,
    .CameraGetProjectionMatrix = CameraGetProjectionMatrix,
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
    .SetPBRMaterialUniforms = AxSetPBRMaterialUniforms,
    .InitTexture = AxInitTexture,
    .DestroyTexture = AxDestroyTexture,
    .SetTextureData = AxSetTextureData,
    .BindTexture = AxBindTexture,
    .SetTextureParameter = AxSetTextureParameter,
    .SetTextureWrapMode = AxSetTextureWrapMode,
    .SetTextureFilterMode = AxSetTextureFilterMode,
    .InitMesh = AxInitMesh,
    .EnableBlending = AxEnableBlending,
    .SetBlendFunction = AxSetBlendFunction,
    .TextureHasAlpha = AxTextureHasAlpha,
    .SetDepthWrite = AxSetDepthWrite,
    .SetCullMode = AxSetCullMode
};

AXON_DLL_EXPORT void LoadPlugin(struct AxAPIRegistry *APIRegistry)
{
    if (APIRegistry)
    {
        PlatformAPI = APIRegistry->Get(AXON_PLATFORM_API_NAME);

        APIRegistry->Set(AXON_OPENGL_API_NAME, AxOpenGLAPI, sizeof(struct AxOpenGLAPI));
    }
}

AXON_DLL_EXPORT void UnloadPlugin(struct AxAPIRegistry *APIRegistry)
{
    if (APIRegistry)
    {
        APIRegistry->Set(AXON_OPENGL_API_NAME, NULL, 0);
        PlatformAPI = NULL;
    }
}