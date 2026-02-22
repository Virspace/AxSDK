/**
 * AxEngine.cpp - Core Engine Implementation
 *
 * Engine orchestrates plugins, window, input, scripting, and rendering.
 * Rendering is delegated to AxRender. Input is managed by AxInput singleton.
 */

#include "AxEngine/AxEngine.h"
#include "AxEngine/AxInput.h"
#include "AxEngine/AxRenderer.h"
#include "AxEngine/AxSceneTree.h"
#include "AxEngine/AxScriptBase.h"
#include "AxEngine/AxNode.h"

#include "AxResource/AxResource.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxAllocatorAPI.h"
#include "Foundation/AxPlatform.h"
#include "Foundation/AxPlugin.h"
#include "AxWindow/AxWindow.h"

#include <string>
#include <string_view>

// Forward declaration of global API registry (defined at end of file)
static AxAPIRegistry* gAPIRegistry = nullptr;

static const int32_t DefaultWindowWidth = 1280;
static const int32_t DefaultWindowHeight = 720;

// Resource allocator for the engine (created during initialization)
static struct AxAllocator* gResourceAllocator = nullptr;

// Global engine pointer (for static API function implementations)
static AxEngine* gEngine = nullptr;

//=============================================================================
// Plugin Loading
//=============================================================================

bool AxEngine::LoadPlugins()
{
    AXON_ASSERT(PluginAPI_ && "PluginAPI is NULL in AxEngine::LoadPlugins!");
    if (!WindowAPI_ || !Window_) {
        fprintf(stderr, "WindowAPI or Window is NULL in AxEngine::LoadPlugins!\n");
        return false;
    }

    auto ShowPluginLoadError = [&](const char* PluginName, const char* ErrorMessage) {
        fprintf(stderr, "Failed to load %s plugin\n", PluginName);
        WindowAPI_->CreateMessageBox(
            Window_,
            "Error",
            ErrorMessage,
            static_cast<AxMessageBoxFlags>(AX_MESSAGE_BOX_ICON_ERROR | AX_MESSAGE_BOX_TYPE_OK)
        );
    };

    auto LoadPluginOrFail = [&](const char* pluginName, const char* errorMessage) {
        return PluginAPI_->Load(pluginName, false)
            ? true
            : (ShowPluginLoadError(pluginName, errorMessage), false);
    };

    if (!LoadPluginOrFail("libAxOpenGL.dll", "Failed to load AxOpenGL plugin.") ||
        !LoadPluginOrFail("libAxResource.dll", "Failed to load AxResource plugin.")) {
        return false;
    }

    // Get core APIs
    PlatformAPI_ = static_cast<AxPlatformAPI*>(APIRegistry_->Get(AXON_PLATFORM_API_NAME));
    ResourceAPI_ = static_cast<AxResourceAPI*>(APIRegistry_->Get(AXON_RESOURCE_API_NAME));

    if (!WindowAPI_ || !PlatformAPI_ || !ResourceAPI_) {
        fprintf(stderr, "Failed to get required APIs from registry\n");
        return (false);
    }

    // Initialize ResourceAPI with an allocator
    AxAllocatorAPI* AllocatorAPI = static_cast<AxAllocatorAPI*>(APIRegistry_->Get(AXON_ALLOCATOR_API_NAME));
    if (!AllocatorAPI) {
        fprintf(stderr, "Failed to get AllocatorAPI from registry\n");
        return (false);
    }

    // Create a heap allocator for engine resources (64MB initial, 256MB max)
    gResourceAllocator = AllocatorAPI->CreateHeap("EngineResources", 64 * 1024 * 1024, 256 * 1024 * 1024);
    if (!gResourceAllocator) {
        fprintf(stderr, "Failed to create resource allocator\n");
        return (false);
    }

    // Initialize ResourceAPI with our allocator
    fprintf(stderr, "[INIT] Calling ResourceAPI->Initialize...\n");
    ResourceAPI_->Initialize(APIRegistry_, gResourceAllocator, nullptr);
    fprintf(stderr, "[INIT] ResourceAPI initialized\n");

    return (true);
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

AxEngine::AxEngine() = default;

bool AxEngine::Initialize(const AxEngineConfig* config)
{
    if (!config) {
        return (false);
    }

    gEngine = this;

    // Use global APIRegistry from LoadPlugin
    APIRegistry_ = gAPIRegistry;
    if (!APIRegistry_) {
        fprintf(stderr, "[INIT] Failed to get API registry\n");
        return (false);
    }

    // Get PluginAPI from registry
    PluginAPI_ = static_cast<AxPluginAPI*>(APIRegistry_->Get(AXON_PLUGIN_API_NAME));
    if (!PluginAPI_) {
        fprintf(stderr, "[INIT] Failed to get PluginAPI from registry\n");
        return (false);
    }

    // Load Window plugin
    fprintf(stderr, "[INIT] Loading AxWindow\n");
    if (!PluginAPI_->Load("libAxWindow.dll", false)) {
        fprintf(stderr, "Failed to load AxWindow plugin\n");
        return (false);
    }

    // Initialize Window API
    WindowAPI_ = static_cast<AxWindowAPI*>(APIRegistry_->Get(AXON_WINDOW_API_NAME));
    fprintf(stderr, "[INIT] Window API: %s\n", WindowAPI_ ? "OK" : "NULL");
    WindowAPI_->Init();

    // Create window
    AxWindowConfig WindowConfig = {
        "AxonSDK Game",
        100, 100,
        DefaultWindowWidth, DefaultWindowHeight,
        static_cast<AxWindowStyle>(AX_WINDOW_STYLE_DECORATED | AX_WINDOW_STYLE_RESIZABLE)
    };

    AxWindowError Error = AX_WINDOW_ERROR_NONE;
    fprintf(stderr, "[INIT] Creating window...\n");
    Window_ = WindowAPI_->CreateWindowWithConfig(&WindowConfig, &Error);
    if (Error != AX_WINDOW_ERROR_NONE) {
        fprintf(stderr, "[INIT] Window creation failed (error %d)\n", (int)Error);
        return (false);
    }
    fprintf(stderr, "[INIT] Window created\n");

    // Load plugins (OpenGL, Scene, Resource)
    fprintf(stderr, "[INIT] Loading plugins...\n");
    if (!LoadPlugins()) {
        return (false);
    }
    fprintf(stderr, "[INIT] Plugins loaded\n");

    // Initialize renderer
    AxWindowPlatformData WindowPlatformData = WindowAPI_->GetPlatformData(Window_);
    fprintf(stderr, "[INIT] Initializing renderer...\n");
    Renderer_ = new AxRenderer();
    if (!Renderer_->Initialize(APIRegistry_, WindowPlatformData.Win32.Handle, DefaultWindowWidth, DefaultWindowHeight)) {
        fprintf(stderr, "AxEngine: Failed to initialize renderer\n");
        return (false);
    }
    fprintf(stderr, "[INIT] Renderer initialized\n");

    // Set cursor mode to disabled for FPS-style controls
    WindowAPI_->SetCursorMode(Window_, AX_CURSOR_DISABLED, &Error);

    // Initialize input system
    fprintf(stderr, "[INIT] Initializing input...\n");
    AxInput::Get().Initialize(APIRegistry_, Window_);

    // Load the default scene (ResourceAPI creates and owns the SceneTree)
    fprintf(stderr, "[INIT] Loading scene...\n");
    SceneHandle_ = ResourceAPI_->LoadScene("examples/graphics/scenes/sponza_atrium.ats");
    if (!AX_HANDLE_IS_VALID(SceneHandle_)) {
        fprintf(stderr, "AxEngine: Failed to load scene\n");
        return (false);
    }
    SceneTree_ = ResourceAPI_->GetScene(SceneHandle_);

    // Provide engine state to SceneTree for script propagation
    SceneTree_->SetMainCamera(Renderer_->GetMainCamera());

    // Load Game DLL and attach script to root node
    fprintf(stderr, "[INIT] Loading Game DLL...\n");
    AxPlatformDLLAPI* DLLAPI = PlatformAPI_->DLLAPI;
    GameDLL_ = DLLAPI->Load("libGame.dll");
    if (DLLAPI->IsValid(GameDLL_)) {
        typedef ScriptBase* (*CreateNodeScriptFn)();
        auto CreateNodeScript = reinterpret_cast<CreateNodeScriptFn>(
            DLLAPI->Symbol(GameDLL_, "CreateNodeScript"));
        if (CreateNodeScript) {
            ScriptBase* GameScript = CreateNodeScript();
            if (GameScript) {
                SceneTree_->GetRootNode()->AttachScript(GameScript);
                fprintf(stderr, "[INIT] Game script attached to root node\n");
            }
        } else {
            fprintf(stderr, "[INIT] Warning: CreateNodeScript symbol not found in Game DLL\n");
        }
    } else {
        fprintf(stderr, "[INIT] Warning: Failed to load Game DLL\n");
    }

    fprintf(stderr, "[INIT] Showing window...\n");
    WindowAPI_->SetWindowVisible(Window_, true, NULL);
    isRunning_ = true;

    fprintf(stderr, "[INIT] Done\n");
    printf("AxEngine: Initialization complete\n");
    return (true);
}

bool AxEngine::Tick()
{
    // Calculate frame time
    AxWallClock CurrentTime = PlatformAPI_->TimeAPI->WallTime();
    float DeltaT = PlatformAPI_->TimeAPI->ElapsedWallTime(LastFrameTime_, CurrentTime);
    LastFrameTime_ = CurrentTime;

    // Poll window events
    WindowAPI_->PollEvents(Window_);
    if (WindowAPI_->HasRequestedClose(Window_)) {
        isRunning_ = false;
        return (false);
    }

    // Update input
    AxInput::Get().Update();

    // Check for ESC to exit
    if (AxInput::Get().IsKeyPressed(AX_KEY_ESCAPE)) {
        isRunning_ = false;
        return (false);
    }

    // Propagate mouse delta to SceneTree for script access
    SceneTree_->UpdateMouseDelta(AxInput::Get().GetMouseDelta());

    // Fixed-timestep update
    FixedAccumulator_ += DeltaT;
    while (FixedAccumulator_ >= FixedTimestep_) {
        // PhysicsServer::Tick — stub placeholder (runs BEFORE scripts, Godot ordering)
        SceneTree_->FixedUpdate(FixedTimestep_);
        FixedAccumulator_ -= FixedTimestep_;
    }

    // Variable-rate update
    SceneTree_->Update(DeltaT);
    SceneTree_->LateUpdate(DeltaT);

    // Render
    Renderer_->BeginFrame();
    Renderer_->RenderScene(SceneTree_);
    Renderer_->EndFrame();

    // Process pending resource releases
    if (ResourceAPI_ && ResourceAPI_->IsInitialized()) {
        ResourceAPI_->ProcessPendingReleases();
    }

    return (true);
}

int AxEngine::Run()
{
    LastFrameTime_ = PlatformAPI_->TimeAPI->WallTime();

    while (isRunning_) {
        if (!Tick()) {
            break;
        }
    }

    return (0);
}

void AxEngine::Shutdown()
{
    isRunning_ = false;

    // Shutdown input
    AxInput::Get().Shutdown();

    // Detach and destroy the game script while Game DLL is still loaded
    // (the script's vtable lives in Game.dll, so it must be deleted before unload)
    if (SceneTree_ && SceneTree_->GetRootNode()) {
        ScriptBase* Script = SceneTree_->GetRootNode()->DetachScript();
        delete Script;
    }

    // Unload Game DLL (script is already destroyed)
    if (PlatformAPI_ && PlatformAPI_->DLLAPI && PlatformAPI_->DLLAPI->IsValid(GameDLL_)) {
        PlatformAPI_->DLLAPI->Unload(GameDLL_);
        GameDLL_ = {};
    }

    // Release scene handle (queues SceneTree for deferred destruction by ResourceAPI)
    if (ResourceAPI_ && AX_HANDLE_IS_VALID(SceneHandle_)) {
        ResourceAPI_->ReleaseScene(SceneHandle_);
        SceneHandle_ = AX_INVALID_HANDLE;
    }
    SceneTree_ = nullptr;

    // Shutdown renderer
    if (Renderer_) {
        Renderer_->Shutdown();
        delete Renderer_;
        Renderer_ = nullptr;
    }

    // Shutdown ResourceAPI (cleans up all loaded resources)
    if (ResourceAPI_ && ResourceAPI_->IsInitialized()) {
        ResourceAPI_->Shutdown();
    }

    // Destroy resource allocator
    if (gResourceAllocator) {
        gResourceAllocator->Destroy(gResourceAllocator);
        gResourceAllocator = nullptr;
    }

    // Destroy window
    if (Window_ && WindowAPI_) {
        WindowAPI_->DestroyWindow(Window_);
        Window_ = nullptr;
    }

    gEngine = nullptr;
}

AxEngine::~AxEngine()
{
    if (isRunning_) {
        Shutdown();
    }
}

//=============================================================================
// Static API Functions
//=============================================================================

static bool Initialize(const AxEngineConfig* cfg)
{
    if (!gEngine) {
        gEngine = new AxEngine();
    }
    return (gEngine->Initialize(cfg));
}

static int Run()
{
    if (!gEngine) {
        return (1);
    }
    return (gEngine->Run());
}

static void Shutdown()
{
    if (gEngine) {
        gEngine->Shutdown();
        delete gEngine;
        gEngine = nullptr;
    }
}

static bool IsRunning()
{
    return (gEngine && gEngine->IsRunning());
}

static AxEngineAPI gEngineAPI = {
    .Initialize = Initialize,
    .Run = Run,
    .Shutdown = Shutdown,
    .IsRunning = IsRunning
};

extern "C" AXENGINE_API void LoadPlugin(AxAPIRegistry* Registry, bool Load)
{
    if (Load) {
        gAPIRegistry = Registry;
        Registry->Set(AX_ENGINE_API_NAME, &gEngineAPI, sizeof(AxEngineAPI));
        printf("AxEngine: Plugin loaded and API registered\n");
    } else {
        if (gEngine) {
            delete gEngine;
            gEngine = nullptr;
        }
        gAPIRegistry = nullptr;
    }
}
