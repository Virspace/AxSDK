#include "Foundation/AxApplication.h"
#include "Foundation/AxPlugin.h"
#include "Foundation/AxAPIRegistry.h"
#include "AxWind/AxWindow.h"
#include "Foundation/AxOpenGL.h"
#include "Foundation/AxPlatform.h"
#include "Foundation/AxTime.h"

#include "OpenGLGame.h"

static Game *Create(int argc, char **argv)
{
    // Attempt to load plugins
    AxWallClock DLLLoadStartTime = PlatformAPI->TimeAPI->WallTime();
    if (!PluginAPI->Load("libAxWindow.dll", false)) {
        fprintf(stderr, "Failed to load AxWindow!\n");
        return (nullptr);
    }

    if (!PluginAPI->Load("libAxOpenGL.dll", false)) {
        fprintf(stderr, "Failed to load AxOpenGL!\n");
        return (nullptr);
    }

    AxWallClock DLLLoadEndTime = PlatformAPI->TimeAPI->WallTime();
    float DLLLoadElapsedTime = PlatformAPI->TimeAPI->ElapsedWallTime(DLLLoadStartTime, DLLLoadEndTime);
    printf("DLL Load %f\n", DLLLoadElapsedTime);

    // Create application
    TheApp = new Game();
    TheApp->APIRegistry = RegistryAPI;
    TheApp->WindowAPI = WindowAPI;
    TheApp->PluginAPI = PluginAPI;

    // Create default window
    WindowAPI->Init();

        // Validate platform before proceeding
    enum AxWindowError Error = AX_WINDOW_ERROR_NONE;
    if (!WindowAPI->ValidatePlatform(&Error)) {
        fprintf(stderr, "Platform validation failed: %s\n", WindowAPI->GetErrorString(Error));
        return (nullptr);
    }

    // Get platform info
    AxPlatformInfo PlatformInfo;
    memset(&PlatformInfo, 0, sizeof(PlatformInfo));
    if (WindowAPI->GetPlatformInfo(&PlatformInfo, &Error)) {
        printf("Platform: %s %s\n", PlatformInfo.Name, PlatformInfo.Version);
        printf("Features: 0x%08X\n", PlatformInfo.Features);
    }

    // Set platform hints for better performance
    AxPlatformHints Hints = {0};
    Hints.Windows.EnableCompositor = true;
    Hints.Windows.UseImmersiveDarkMode = false;

    if (!WindowAPI->SetPlatformHints(&Hints, &Error)) {
        fprintf(stderr, "Failed to set platform hints: %s\n", WindowAPI->GetErrorString(Error));
        // Continue anyway, this is not critical
    }

    TheApp->Window = CreateWindow();
    AXON_ASSERT(TheApp->Window && "Window is NULL!");

    RegisterCallbacks();

    // Initialize the renderer and setup backend
    RenderAPI->CreateContext(TheApp->Window);

    ConstructViewport();

    auto Info = RenderAPI->GetInfo(true);
    printf("Vendor: %s\n", Info.Vendor);
    printf("Renderer: %s\n", Info.Renderer);
    printf("GL Version: %s\n", Info.GLVersion);
    printf("GLSL Version: %s\n", Info.GLSLVersion);

    ModelShaderData = ConstructShader("shaders/vert.glsl", "shaders/frag.glsl");
    DiffuseShaderData = ConstructShader("shaders/ortho.glsl", "shaders/frag.glsl");

    ConstructScene();

    AxVec3 CameraPos = { 0.0f, 0.0f, 8.0f };
    AxVec3 CameraTarget = { 0.0f, 0.0f, 0.0f };
    AxVec3 Up = { 0.0f, 1.0f, 0.0f };
    CreatePerspectiveCamera(CameraPos, CameraTarget, Up);

    // Position the orthographic camera directly above looking straight down
    AxVec3 OrthoCameraPos = { 0.0f, 1.0f, 0.0f };
    AxVec3 OrthoCameraTarget = { 0.0f, 0.0f, 0.0f };
    AxVec3 OrthoUp = { 0.0f, 0.0f, -1.0f };  // Up vector points towards negative Z for proper top-down orientation
    CreateOrthographicCamera(OrthoCameraPos, OrthoCameraTarget, OrthoUp, 1.0f);  // Start with default 1.0 zoom

    // Create scene
    if (!ConstructObject()) {
        fprintf(stderr, "Failed to construct object!\n");
        return (nullptr);
    }

    enum AxWindowError Error2 = AX_WINDOW_ERROR_NONE;
    WindowAPI->SetWindowVisible(TheApp->Window, true, &Error2);
    if (Error2 != AX_WINDOW_ERROR_NONE) {
        fprintf(stderr, "Failed to set window visible: %s\n", WindowAPI->GetErrorString(Error2));
        return (nullptr);
    }

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
    if (WindowAPI->HasRequestedClose(TheApp->Window) || App->IsRequestingExit) {
        return(true);
    }

    // Demonstrate new input state querying features
    enum AxWindowError Error = AX_WINDOW_ERROR_NONE;
    AxInputState InputState = {0};
    if (WindowAPI->GetWindowInputState(TheApp->Window, &InputState, &Error)) {
        // Check for specific key combinations
        if (InputState.Keys[AX_KEY_S] && InputState.Modifiers & AX_MOD_CONTROL) {
            printf("Ctrl+S pressed - could trigger save\n");
        }

        if (InputState.Keys[AX_KEY_F11]) {
            printf("F11 pressed - could toggle fullscreen\n");
        }

        // Check mouse state
        if (InputState.MouseButtons[AX_MOUSE_BUTTON_LEFT]) {
            printf("Left mouse button is pressed at (%.2f, %.2f)\n", 
                   InputState.MousePosition.X, InputState.MousePosition.Y);
        }

        // Check window focus
        if (!InputState.WindowFocused) {
            printf("Window lost focus - could pause game\n");
        }
    }

    // Start the frame
    RenderAPI->NewFrame();

    // We're not rending the other viewport here yet, questions about ordering -- textures then render?
    // What to do with update()?
    auto MonkeyModel = Models["Monkey"];
    auto DiffuseModel = Models["Diffuse"];

    RenderModel(&MonkeyModel, &MonkeyModel.Meshes[0], &PerspCamera, ModelShaderData, &Viewport);
    RenderModel(&DiffuseModel, &DiffuseModel.Meshes[0], &DiffuseCamera, DiffuseShaderData, &DiffuseViewport);

    RenderAPI->SwapBuffers();

    return(App->IsRequestingExit);
}

static bool Destroy(struct AxApplication *App)
{
    // Clean up resources
    RenderAPI->DestroyProgram(ModelShaderData->ShaderHandle);
    RenderAPI->DestroyProgram(DiffuseShaderData->ShaderHandle);

    // Shutdown renderer
    RenderAPI->DestroyContext();
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

        APIRegistry->Set(AXON_APPLICATION_API_NAME, AxApplicationAPI, sizeof(struct AxApplicationAPI));
    }
}