#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <unordered_map>

#include "AxApplication.h"

#define AXARRAY_IMPLEMENTATION
#include "Foundation/AxArray.h"
#include "Foundation/AxPlugin.h"
#include "Foundation/AxEditorPlugin.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxApplication.h"
#include "Foundation/AxPlatform.h"
#include "Foundation/AxCamera.h"
#include "Foundation/AxMath.h"

#include "AxWindow/AxWindow.h"
#include "AxOpenGL/AxOpenGL.h"

static constexpr AxVec2 DISPLAY_SIZE = { 1920, 1080 };

// NOTE(mdeforge): These get assigned to in LoadPlugin().
struct AxAPIRegistry *RegistryAPI;
struct AxPluginAPI *PluginAPI;
struct AxWindowAPI *WindowAPI;
struct AxOpenGLAPI *RenderAPI;
struct AxPlatformAPI *PlatformAPI;
struct AxPlatformFileAPI *FileAPI;
struct AxMesh *Mesh;

static AxDrawVert *VertexBuffer;
static AxDrawIndex *IndexBuffer;

static AxApplication *TheApp;

// TODO(mdeforge): In lieu of having a scene to manage...
static AxDrawData *DrawData;
static AxDrawList *DrawList;
static AxDrawable *Drawable;
static AxShaderData *ShaderData;
static AxTexture *Texture;
static AxMaterial *Material;
static uint32_t ShaderProgramID;

static void ResetState()
{
    TheApp->Viewer.CameraTransform = {
        .Position = { 0, 0, 0 },
        .Rotation = { 0, 0, 0 },
        .Scale = { 1, 1, 1 }
    };
}

static AxWindow *CreateWindow()
{
    int32_t StyleFlags = AX_WINDOW_STYLE_VISIBLE | AX_WINDOW_STYLE_DECORATED | AX_WINDOW_STYLE_RESIZABLE;
    TheApp->Window = WindowAPI->CreateWindow(
        "Axon Engine",
        100,
        100,
        static_cast<int32_t>(DISPLAY_SIZE.Width),
        static_cast<int32_t>(DISPLAY_SIZE.Height),
        (AxWindowStyle)StyleFlags
    );

    enum AxCursorMode CursorMode = AX_CURSOR_NORMAL;
    WindowAPI->SetCursorMode(TheApp->Window, CursorMode);

    return (TheApp->Window);
}

static bool SetupInitialWindow()
{
    // TODO(mdeforge): Clamp window size based on display once display API is done
    WindowAPI->Init();

    return (CreateWindow());
}

static void InitializeRenderer()
{
    RenderAPI->Create(TheApp->Window);
}

static void ConstructShader()
{
    // Load shaders
    AxFile VertShader = FileAPI->OpenForRead("shaders/vert.glsl");
    AxFile FragShader = FileAPI->OpenForRead("shaders/frag.glsl");

    uint64_t VertFileSize = FileAPI->Size(VertShader);
    uint64_t FragFileSize = FileAPI->Size(FragShader);

    // Allocate text buffers
    char *VertexShaderBuffer = (char *)malloc(VertFileSize + 1);
    char *FragShaderBuffer = (char *)malloc(FragFileSize + 1);

    // Read shader text
    FileAPI->Read(VertShader, VertexShaderBuffer, (uint32_t)VertFileSize);
    FileAPI->Read(FragShader, FragShaderBuffer, (uint32_t)FragFileSize);

    // Null-terminate
    VertexShaderBuffer[VertFileSize] = '\0';
    FragShaderBuffer[FragFileSize] = '\0';

    // Create shader program
    ShaderProgramID = RenderAPI->CreateProgram("", (char *)VertexShaderBuffer, (char *)FragShaderBuffer);
    AXON_ASSERT(ShaderProgramID && "ShaderProgram is NULL!");

    // Free text buffers
    free(VertexShaderBuffer);
    free(FragShaderBuffer);

    // Setup the shader data
    ShaderData = (struct AxShaderData *)calloc(1, sizeof(AxShaderData));
    ShaderData->ShaderHandle = ShaderProgramID;

    // This will go away after I make the changes found in the TODO in GetAttribLocations
    bool Result = RenderAPI->GetAttributeLocations(ShaderProgramID, ShaderData);
    AXON_ASSERT(Result && "Failed to get shader attribute locations!");
}

static void ConstructObject()
{
    // Define vertices for a square (two triangles)
    AxDrawVert Vertices[] = {
        // Position            // Color (ABGR)
        {{-0.5f,  0.5f, 1.0f}, {0x000000FF}}, // Bottom-left
        {{ 0.5f,  0.5f, 0.0f}, {0x0000FF00}}, // Bottom-right
        {{ 0.5f, -0.5f, 0.0f}, {0x00FF0000}}, // Top-right
        {{-0.5f, -0.5f, 1.0f}, {0x00FFFFFF}}  // Top-left
    };

    AxDrawIndex Indices[] = {
        0, 1, 2, // First triangle
        2, 3, 0  // Second triangle
    };

    for (size_t i = 0; i < sizeof(Vertices) / sizeof(Vertices[0]); ++i) {
        ArrayPush(VertexBuffer, Vertices[i]);
    }

    for (size_t i = 0; i < sizeof(Indices) / sizeof(Indices[0]); ++i) {
        ArrayPush(IndexBuffer, Indices[i]);
    }

    // Create mesh
    Mesh = (struct AxMesh *)calloc(1, sizeof(AxMesh));
    for (size_t i = 0; i < sizeof(Vertices) / sizeof(Vertices[0]); ++i) {
        ArrayPush(Mesh->Vertices, Vertices[i]);
    }

    for (size_t i = 0; i < sizeof(Indices) / sizeof(Indices[0]); ++i) {
        ArrayPush(Mesh->Indices, Indices[i]);
    }
}

void ConstructMaterial()
{
    // Material
    Material = (struct AxMaterial *)calloc(1, sizeof(AxMaterial));
    Texture = (struct AxTexture *)calloc(1, sizeof(AxTexture));
    Material->ShaderData = ShaderData;
    Material->Texture = Texture;
}

void ConstructSceneObject()
{
    // Create and initialize the drawable
    AxMat4x4 Mat = {0};
    Drawable = (struct AxDrawable *)calloc(1, sizeof(AxDrawable));
    RenderAPI->DrawableInit(Drawable, Mesh, Material, Mat);
}

static void ConstructScene()
{
    // Create and initialize the draw data
    AxVec2 DisplayPos = { 0.0f, 0.0f };
    AxVec2 DisplaySize = DISPLAY_SIZE;
    AxVec2 FramebufferScale = { 1.0f, 1.0f };
    DrawData = (struct AxDrawData *)calloc(1, sizeof(AxDrawData));
    RenderAPI->DrawDataInit(DrawData, DisplayPos, DisplaySize, FramebufferScale);

    ConstructObject();
    ConstructShader();
    ConstructMaterial();
    ConstructSceneObject();

    // Create and initialize the draw list
    DrawList = (struct AxDrawList *)calloc(1, sizeof(AxDrawList));
    RenderAPI->DrawListInit(DrawList);

    // Bind vertex and index data to drawable
    RenderAPI->DrawListBind(DrawList);
    RenderAPI->DrawListBufferData(DrawList, VertexBuffer, IndexBuffer, ArraySize(VertexBuffer), ArraySize(IndexBuffer));
    RenderAPI->DrawableAddShaderData(Drawable, ShaderData);

    // Add the drawable to the draw list
    AxVec4 ClipRect = { 0.0f, 0.0f, DrawData->DisplaySize.X, DrawData->DisplaySize.Y };
    struct AxDrawCommand DrawCommand = {
        .VertexOffset = ArraySize(DrawList->VertexBuffer), // Size of the buffer up until this point
        .IndexOffset = ArraySize(DrawList->IndexBuffer),   // Size of the buffer up until this point
        .ElementCount = ArraySize(Mesh->Indices),
        .ClipRect = ClipRect,
        .Drawable = Drawable
    };

    RenderAPI->DrawListAddCommand(DrawList, DrawCommand);
    RenderAPI->DrawListAddDrawable(DrawList, Drawable, ClipRect);

    // Create draw data
    RenderAPI->DrawDataAddDrawList(DrawData, DrawList);
    RenderAPI->DrawListUnbind();
}

static AxApplication *Create(int argc, char **argv)
{
    // bool ShouldPrintUsage = false;
    // for (int32_t i = 1; i < argc; ++i)
    // {
    //     if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
    //         ShouldPrintUsage = true;
    //     }
    // }

    // Attempt to load plugins
    AxWallClock DLLLoadStartTime = PlatformAPI->TimeAPI->WallTime();
    PluginAPI->Load("Z:/AxSDK/build/Plugins/AxWindow/Debug/AxWindow.dll", false);
    PluginAPI->Load("Z:/AxSDK/build/Plugins/AxOpenGL/Debug/AxOpenGL.dll", false);
    AxWallClock DLLLoadEndTime = PlatformAPI->TimeAPI->WallTime();

    float DLLLoadElapsedTime = PlatformAPI->TimeAPI->ElapsedWallTime(DLLLoadStartTime, DLLLoadEndTime);
    printf("DLL Load %f\n", DLLLoadElapsedTime);

    // Create application
    TheApp = new AxApplication();
    TheApp->APIRegistry = RegistryAPI;
    TheApp->WindowAPI = WindowAPI;
    TheApp->PluginAPI = PluginAPI;

    ResetState();

    // Create default window
    if (!SetupInitialWindow()) {
        return (nullptr);
    }

    // Initialize the renderer and setup backend
    InitializeRenderer();

    // Create scene
    ConstructScene();

    WindowAPI->SetWindowVisible(TheApp->Window, true);

    return (TheApp);
}

static bool Tick(struct AxApplication *App)
{
    // Update frame time
    // NOTE(mdeforge): VSync is currently on

    AxWallClock Previous = PlatformAPI->TimeAPI->WallTime();
    float DeltaT = PlatformAPI->TimeAPI->ElapsedWallTime(TheApp->Timing.WallClock, Previous);

    TheApp->Timing.DeltaT = DeltaT;
    TheApp->Timing.WallClock = Previous;
    TheApp->Timing.ElapsedWallTime += DeltaT;

    // Run messsage pump for window
    WindowAPI->PollEvents(TheApp->Window);

    // Check for close
    if (WindowAPI->HasRequestedClose(TheApp->Window)) {
        return (false);
    }

    // Start the frame
    RenderAPI->NewFrame();
    RenderAPI->Render(DrawData);
    RenderAPI->SwapBuffers();

    return(true);
}

static bool Destroy(struct AxApplication *App)
{
    // Clean up resources
    RenderAPI->DestroyProgram(ShaderProgramID);

    RenderAPI->MaterialDestroy(Material);
    RenderAPI->MeshDestroy(Mesh);

    RenderAPI->DrawableDestroy(Drawable);

    RenderAPI->DrawListDestroy(DrawList);
    RenderAPI->DrawDataDestroy(DrawData);

    // Shutdown renderer
    RenderAPI->Destroy();
    WindowAPI->DestroyWindow(TheApp->Window);

    free(TheApp);

    return (true);
}

static struct AxApplicationAPI AxApplicationAPI_ {
    .Create = Create,
    .Tick = Tick,
    .Destroy = Destroy,
};

static struct AxApplicationAPI *AxApplicationAPI = &AxApplicationAPI_;

extern "C" AXON_DLL_EXPORT void LoadPlugin(struct AxAPIRegistry *APIRegistry, bool Load)
{
    if (APIRegistry)
    {
        PluginAPI = (struct AxPluginAPI *)APIRegistry->Get(AXON_PLUGIN_API_NAME);
        WindowAPI = (struct AxWindowAPI *)APIRegistry->Get(AXON_WINDOW_API_NAME);
        RenderAPI = (struct AxOpenGLAPI *)APIRegistry->Get(AXON_OPENGL_API_NAME);
        PlatformAPI = (struct AxPlatformAPI *)APIRegistry->Get(AXON_PLATFORM_API_NAME);
        FileAPI = PlatformAPI->FileAPI;
        RegistryAPI = APIRegistry;

        // TODO(mdeforge): Can we do this elsewhere? Because if not, we may need a list of plugins to load
        // prior to getting here so we can do this here. I think we may just need to cache the APIRegistry
        // and then we should be good to maintain a list of EnginePlugins *
        //DialogEditorAPI = (struct EditorPlugin *)APIRegistry->Get("AxDialogEditorAPI");

        APIRegistry->Set(AXON_APPLICATION_API_NAME, AxApplicationAPI, sizeof(struct AxApplicationAPI));
    }
}