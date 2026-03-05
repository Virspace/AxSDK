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
#include "AxEngine/AxSceneParser.h"
#include "AxEngine/AxScriptBase.h"
#include "AxEngine/AxNode.h"
#include "AxEngine/AxTypedNodes.h"
#include "AxEngine/AxPrimitives.h"

#include "AxResource/AxResource.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxAllocatorAPI.h"
#include "Foundation/AxPlatform.h"
#include "Foundation/AxPlugin.h"
#include "AxWindow/AxWindow.h"
#include "AxLog/AxLog.h"

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
        AX_LOG(ERROR, "WindowAPI or Window is NULL in AxEngine::LoadPlugins!");
        return false;
    }

    auto ShowPluginLoadError = [&](const char* PluginName, const char* ErrorMessage) {
        AX_LOG(ERROR, "Failed to load %s plugin", PluginName);
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
        AX_LOG(ERROR, "Failed to get required APIs from registry");
        return (false);
    }

    // Initialize ResourceAPI with an allocator
    AxAllocatorAPI* AllocatorAPI = static_cast<AxAllocatorAPI*>(APIRegistry_->Get(AXON_ALLOCATOR_API_NAME));
    if (!AllocatorAPI) {
        AX_LOG(ERROR, "Failed to get AllocatorAPI from registry");
        return (false);
    }

    // Create a heap allocator for engine resources (64MB initial, 256MB max)
    gResourceAllocator = AllocatorAPI->CreateHeap("EngineResources", 64 * 1024 * 1024, 256 * 1024 * 1024);
    if (!gResourceAllocator) {
        AX_LOG(ERROR, "Failed to create resource allocator");
        return (false);
    }

    // Initialize ResourceAPI with our allocator
    ResourceAPI_->Initialize(APIRegistry_, gResourceAllocator, nullptr);

    // Initialize primitive mesh generation (needs ResourceAPI to be ready)
    PrimitiveMesh::Init(APIRegistry_);

    return (true);
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

AxEngine::AxEngine() = default;

bool AxEngine::InitWindow()
{
    if (!PluginAPI_->Load("libAxWindow.dll", false)) {
        AX_LOG(ERROR, "Failed to load AxWindow plugin");
        return (false);
    }

    WindowAPI_ = static_cast<AxWindowAPI*>(APIRegistry_->Get(AXON_WINDOW_API_NAME));
    if (!WindowAPI_) {
        AX_LOG(ERROR, "Failed to get WindowAPI from registry");
        return (false);
    }
    WindowAPI_->Init();

    AxWindowConfig WindowConfig = {
        "AxonSDK Game",
        100, 100,
        DefaultWindowWidth, DefaultWindowHeight,
        static_cast<AxWindowStyle>(AX_WINDOW_STYLE_DECORATED | AX_WINDOW_STYLE_RESIZABLE)
    };

    AxWindowError Error = AX_WINDOW_ERROR_NONE;
    Window_ = WindowAPI_->CreateWindowWithConfig(&WindowConfig, &Error);
    if (Error != AX_WINDOW_ERROR_NONE) {
        AX_LOG(ERROR, "Window creation failed (error %d)", (int)Error);
        return (false);
    }

    AX_LOG(INFO, "Window created");
    return (true);
}

bool AxEngine::InitRenderer()
{
    AxWindowPlatformData WindowPlatformData = WindowAPI_->GetPlatformData(Window_);
    Renderer_ = new AxRenderer();
    if (!Renderer_->Initialize(APIRegistry_, WindowPlatformData.Win32.Handle, DefaultWindowWidth, DefaultWindowHeight)) {
        AX_LOG(ERROR, "Failed to initialize renderer");
        return (false);
    }

    AxWindowError Error = AX_WINDOW_ERROR_NONE;
    WindowAPI_->SetCursorMode(Window_, AX_CURSOR_DISABLED, &Error);

    AX_LOG(INFO, "Renderer initialized");
    return (true);
}

void AxEngine::InitInput()
{
    AxInput::Get().Initialize(APIRegistry_, Window_);
}

bool AxEngine::InitScene()
{
    SceneParser_.Init(APIRegistry_);

    SceneTree_ = LoadScene("examples/graphics/scenes/sponza_atrium.ats");
    if (!SceneTree_) {
        AX_LOG(ERROR, "Failed to load scene");
        return (false);
    }

    uint32_t CamCount = 0;
    Node** CamNodes = SceneTree_->GetNodesByType(NodeType::Camera, &CamCount);
    if (CamNodes && CamCount > 0) {
        CameraNode* SceneCam = static_cast<CameraNode*>(CamNodes[0]);
        Renderer_->SetMainCamera(SceneCam);
        SceneTree_->SetMainCamera(SceneCam);
    }

    AX_LOG(INFO, "Scene loaded");
    return (true);
}

void AxEngine::InitGameScript()
{
    AxPlatformDLLAPI* DLLAPI = PlatformAPI_->DLLAPI;
    GameDLL_ = DLLAPI->Load("libGame.dll");
    if (!DLLAPI->IsValid(GameDLL_)) {
        AX_LOG(WARNING, "Failed to load Game DLL");
        return;
    }

    typedef ScriptBase* (*CreateNodeScriptFn)();
    auto CreateNodeScript = reinterpret_cast<CreateNodeScriptFn>(
        DLLAPI->Symbol(GameDLL_, "CreateNodeScript"));
    if (!CreateNodeScript) {
        AX_LOG(WARNING, "CreateNodeScript symbol not found in Game DLL");
        return;
    }

    ScriptBase* GameScript = CreateNodeScript();
    if (GameScript) {
        SceneTree_->GetRootNode()->AttachScript(GameScript);
        AX_LOG(INFO, "Game script attached to root node");
    }
}

bool AxEngine::Initialize(const AxEngineConfig* config)
{
    if (!config) {
        return (false);
    }

    gEngine = this;

    APIRegistry_ = gAPIRegistry;
    if (!APIRegistry_) {
        AX_LOG(ERROR, "Failed to get API registry");
        return (false);
    }

    PluginAPI_ = static_cast<AxPluginAPI*>(APIRegistry_->Get(AXON_PLUGIN_API_NAME));
    if (!PluginAPI_) {
        AX_LOG(ERROR, "Failed to get PluginAPI from registry");
        return (false);
    }

    if (!InitWindow())
        return (false);
    if (!LoadPlugins())
        return (false);
    if (!InitRenderer())
        return (false);

        InitInput();

    if (!InitScene())
        return (false);

    InitGameScript();

    WindowAPI_->SetWindowVisible(Window_, true, NULL);
    isRunning_ = true;

    AX_LOG(INFO, "Initialization complete");
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

    // Release scene (unload models, destroy tree)
    UnloadScene();

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

    // Terminate scene parser
    SceneParser_.Term();

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
// Scene Loading (Engine-owned)
//=============================================================================

SceneTree* AxEngine::LoadScene(const char* FilePath)
{
    if (!FilePath) {
        return (nullptr);
    }

    SceneTree* Scene = SceneParser_.LoadSceneFromFile(FilePath);
    if (Scene) {
        LoadSceneModels(Scene);
    }
    return (Scene);
}

void AxEngine::LoadSceneModels(SceneTree* Scene)
{
    if (!Scene || !ResourceAPI_) {
        return;
    }

    uint32_t Count = 0;
    Node** MeshNodes = Scene->GetNodesByType(NodeType::MeshInstance, &Count);
    if (!MeshNodes || Count == 0) {
        return;
    }

    AX_LOG(INFO, "Loading models for %u MeshInstance node(s)", Count);

    for (uint32_t i = 0; i < Count; ++i) {
        MeshInstance* MI = static_cast<MeshInstance*>(MeshNodes[i]);
        if (!MI || MI->MeshPath[0] == '\0') {
            continue;
        }

        if (AX_HANDLE_IS_VALID(MI->ModelHandle)) {
            continue;
        }

        AxModelHandle Handle = ResourceAPI_->LoadModel(MI->MeshPath);
        if (AX_HANDLE_IS_VALID(Handle)) {
            MI->ModelHandle = Handle;
            AX_LOG(DEBUG, "Loaded model '%s' -> [%u:%u]",
                   MI->MeshPath, Handle.Index, Handle.Generation);
        } else {
            AX_LOG(ERROR, "Failed to load model '%s'", MI->MeshPath);
        }
    }
}

void AxEngine::UnloadScene()
{
    if (!SceneTree_) {
        return;
    }

    // Release model handles for all MeshInstance nodes
    if (ResourceAPI_) {
        uint32_t Count = 0;
        Node** MeshNodes = SceneTree_->GetNodesByType(NodeType::MeshInstance, &Count);
        for (uint32_t i = 0; i < Count; ++i) {
            MeshInstance* MI = static_cast<MeshInstance*>(MeshNodes[i]);
            if (MI && AX_HANDLE_IS_VALID(MI->ModelHandle)) {
                ResourceAPI_->ReleaseModel(MI->ModelHandle);
            }
        }
    }

    delete SceneTree_;
    SceneTree_ = nullptr;
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
        AX_LOG(INFO, "Plugin loaded and API registered");
    } else {
        if (gEngine) {
            delete gEngine;
            gEngine = nullptr;
        }
        gAPIRegistry = nullptr;
    }
}
