/**
 * AxEngine.cpp - Core Engine Implementation
 *
 * Engine orchestrates plugins, window, input, scripting, and rendering.
 * Rendering is delegated to AxRender. Input is managed by AxInput singleton.
 */

#include "AxEngine/AxEngine.h"
#include "AxEngine/AxInput.h"
#include "AxEngine/AxRenderer.h"
#include "AxEngine/AxSceneManager.h"
#include "AxEngine/AxScripting.h"

#include "AxResource/AxResource.h"
#include "AxScene/AxScene.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxAllocatorAPI.h"
#include "Foundation/AxPlatform.h"
#include "Foundation/AxPlugin.h"
#include "AxWindow/AxWindow.h"

#include <stdio.h>

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

    auto LoadPluginOrFail = [&](const char* PluginName, const char* ErrorMessage) -> bool {
        if (!PluginAPI_->Load(PluginName, false)) {
            fprintf(stderr, "Failed to load %s plugin\n", PluginName);
            WindowAPI_->CreateMessageBox(
                Window_,
                "Error",
                ErrorMessage,
                static_cast<AxMessageBoxFlags>(AX_MESSAGE_BOX_ICON_ERROR | AX_MESSAGE_BOX_TYPE_OK)
            );
            return false;
        }
        return true;
    };

    if (!LoadPluginOrFail("libAxOpenGL.dll", "Failed to load AxOpenGL plugin.") ||
        !LoadPluginOrFail("libAxScene.dll", "Failed to load AxScene plugin.") ||
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
    ResourceAPI_->Initialize(APIRegistry_, gResourceAllocator, nullptr);
    printf("AxEngine: ResourceAPI initialized\n");

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
        fprintf(stderr, "Failed to get API registry - LoadPlugin not called?\n");
        return (false);
    }

    // Get PluginAPI from registry
    PluginAPI_ = static_cast<AxPluginAPI*>(APIRegistry_->Get(AXON_PLUGIN_API_NAME));
    if (!PluginAPI_) {
        fprintf(stderr, "Failed to get PluginAPI from registry\n");
        return (false);
    }

    // Load Window plugin
    if (!PluginAPI_->Load("libAxWindow.dll", false)) {
        fprintf(stderr, "Failed to load AxWindow plugin\n");
        return (false);
    }

    // Initialize Window API
    WindowAPI_ = static_cast<AxWindowAPI*>(APIRegistry_->Get(AXON_WINDOW_API_NAME));
    WindowAPI_->Init();

    // Create window
    AxWindowConfig WindowConfig = {
        "AxonSDK Game",
        100, 100,
        DefaultWindowWidth, DefaultWindowHeight,
        static_cast<AxWindowStyle>(AX_WINDOW_STYLE_DECORATED | AX_WINDOW_STYLE_RESIZABLE)
    };

    AxWindowError Error = AX_WINDOW_ERROR_NONE;
    Window_ = WindowAPI_->CreateWindowWithConfig(&WindowConfig, &Error);
    if (Error != AX_WINDOW_ERROR_NONE) {
        return (false);
    }

    // Load plugins (OpenGL, Scene, Resource)
    if (!LoadPlugins()) {
        return (false);
    }

    // Initialize renderer
    AxWindowPlatformData WindowPlatformData = WindowAPI_->GetPlatformData(Window_);
    Renderer_ = new AxRenderer();
    if (!Renderer_->Initialize(APIRegistry_, WindowPlatformData.Win32.Handle, DefaultWindowWidth, DefaultWindowHeight)) {
        fprintf(stderr, "AxEngine: Failed to initialize renderer\n");
        return (false);
    }

    // Set cursor mode to disabled for FPS-style controls
    WindowAPI_->SetCursorMode(Window_, AX_CURSOR_DISABLED, &Error);

    // Initialize input system
    AxInput::Get().Initialize(APIRegistry_, Window_);

    // Initialize scene manager
    AxSceneManager::Get().Initialize(APIRegistry_);

    // Create scripting system
    Scripting_ = new AxScripting();
    if (!Scripting_->Create(APIRegistry_)) {
        fprintf(stderr, "AxEngine: Failed to create scripting system\n");
        return (false);
    }

    // Load game script
    if (Scripting_->LoadScript("libGame.dll")) {
        printf("AxEngine: Loaded Game script\n");
    } else {
        printf("AxEngine: No Game script found (libGame.dll)\n");
    }

    // Set initial engine state on scripts
    Scripting_->SetEngineState(Renderer_->GetMainCamera(), nullptr);

    // Initialize scripts
    Scripting_->Init();

    WindowAPI_->SetWindowVisible(Window_, true, NULL);
    isRunning_ = true;

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

    // Update scripts
    if (Scripting_) {
        Scripting_->UpdateFrameState(AxInput::Get().GetMouseDelta());
        Scripting_->Tick(0.0, DeltaT);
    }

    // Render
    Renderer_->BeginFrame();

    // Get scene from scene manager and render
    AxScene* Scene = AxSceneManager::Get().GetScene();
    Renderer_->RenderScene(Scene);

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

    // Shutdown scripts
    if (Scripting_) {
        Scripting_->Term();
        Scripting_->Destroy();
        delete Scripting_;
        Scripting_ = nullptr;
    }

    // Shutdown input
    AxInput::Get().Shutdown();

    // Shutdown scene manager
    AxSceneManager::Get().Shutdown();

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
