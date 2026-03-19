/**
 * AxEngine.cpp - Core Engine Implementation
 *
 * Engine orchestrates plugins, window, input, scripting, and rendering.
 * Rendering is delegated to AxRender. Input is managed by AxInput singleton.
 *
 * Supports two hosting modes:
 *   - Standalone: engine creates its own window and runs the frame loop
 *   - Editor-hosted: editor supplies an HWND, engine skips window creation,
 *     and the editor drives Tick() externally
 */

#include "AxEngine/AxEngine.h"
#include "AxEngine/AxInput.h"
#include "AxEngine/AxRenderer.h"
#include "AxEngine/AxSceneTree.h"
#include "AxEngine/AxSceneParser.h"
#include "AxEngine/AxScriptBase.h"
#include "AxEngine/AxScriptRegistry.h"
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

static const int32_t DefaultWindowWidth = 1280;
static const int32_t DefaultWindowHeight = 720;

// Resource allocator for the engine (created during initialization)
static struct AxAllocator* gResourceAllocator = nullptr;

//=============================================================================
// Plugin Loading
//=============================================================================

bool AxEngine::LoadPlugins()
{
#if !defined(AX_SHIPPING)
    AXON_ASSERT(PluginAPI_ && "PluginAPI is NULL in AxEngine::LoadPlugins!");

    // In editor-hosted mode, WindowAPI and Window are not available.
    // Only show message box errors when we have a window.
    auto ShowPluginLoadError = [&](const char* PluginName, const char* ErrorMessage) {
        AX_LOG(ERROR, "Failed to load %s plugin", PluginName);
        if (WindowAPI_ && Window_) {
            WindowAPI_->CreateMessageBox(
                Window_,
                "Error",
                ErrorMessage,
                static_cast<AxMessageBoxFlags>(AX_MESSAGE_BOX_ICON_ERROR | AX_MESSAGE_BOX_TYPE_OK)
            );
        }
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
#endif

    // Get core APIs
    PlatformAPI_ = static_cast<AxPlatformAPI*>(APIRegistry_->Get(AXON_PLATFORM_API_NAME));
    ResourceAPI_ = static_cast<AxResourceAPI*>(APIRegistry_->Get(AXON_RESOURCE_API_NAME));

    if (!PlatformAPI_ || !ResourceAPI_) {
        AX_LOG(ERROR, "Failed to get required APIs from registry");
        return (false);
    }

    // WindowAPI may be nullptr in editor-hosted mode -- that is expected
    if (!WindowAPI_ && !IsEditorHosted()) {
        AX_LOG(ERROR, "WindowAPI is NULL in standalone mode");
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
#if !defined(AX_SHIPPING)
    if (!PluginAPI_->Load("libAxWindow.dll", false)) {
        AX_LOG(ERROR, "Failed to load AxWindow plugin");
        return (false);
    }
#endif

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
    uint64_t WindowHandle = 0;
    int32_t Width = DefaultWindowWidth;
    int32_t Height = DefaultWindowHeight;

    if (IsEditorHosted()) {
        // Editor-hosted: use the externally-provided window handle and dimensions
        WindowHandle = ExternalWindowHandle_;
        Width = ViewportWidth_;
        Height = ViewportHeight_;
    } else {
        // Standalone: extract handle from AxWindow
        AxWindowPlatformData WindowPlatformData = WindowAPI_->GetPlatformData(Window_);
        WindowHandle = WindowPlatformData.Win32.Handle;
    }

    Renderer_ = new AxRenderer();
    if (!Renderer_->Initialize(APIRegistry_, WindowHandle, Width, Height)) {
        AX_LOG(ERROR, "Failed to initialize renderer");
        return (false);
    }

    // Only set cursor mode in standalone mode (editor owns cursor)
    if (!IsEditorHosted() && WindowAPI_ && Window_) {
        AxWindowError Error = AX_WINDOW_ERROR_NONE;
        WindowAPI_->SetCursorMode(Window_, AX_CURSOR_DISABLED, &Error);
    }

    // In editor-hosted mode, set default editor camera view and enable it
    if (IsEditorHosted()) {
        Renderer_->SetEditorCameraView(
            EditorCamera_.Position, EditorCamera_.Target,
            EditorCamera_.FOV, EditorCamera_.Near, EditorCamera_.Far);
        Renderer_->SetUseEditorCamera(true);
    }

    AX_LOG(INFO, "Renderer initialized");
    return (true);
}

void AxEngine::InitInput()
{
    AXON_ASSERT(APIRegistry_ && "InitInput called before APIRegistry_ set");

    if (IsEditorHosted()) {
        // Editor-hosted: initialize with nullptr -- AxInput will no-op on Update()
        AxInput::Get().Initialize(APIRegistry_, nullptr);
    } else {
        AxInput::Get().Initialize(APIRegistry_, Window_);
    }
}

bool AxEngine::InitScene()
{
    SceneTree_ = LoadScene("examples/graphics/scenes/sponza_atrium.ats");
    if (!SceneTree_) {
        AX_LOG(ERROR, "Failed to load scene");
        return (false);
    }

    uint32_t CamCount = 0;
    Node** CamNodes = SceneTree_->GetNodesByType(NodeType::Camera, &CamCount);
    if (CamNodes && CamCount > 0) {
        CameraNode* SceneCam = static_cast<CameraNode*>(CamNodes[0]);
        if (Renderer_) {
            Renderer_->SetMainCamera(SceneCam);
        }
        SceneTree_->SetMainCamera(SceneCam);
    }

    AX_LOG(INFO, "Scene loaded");
    return (true);
}

bool AxEngine::InitGameScript()
{
#if !defined(AX_SHIPPING)
    // Development/Debug: load Game DLL (static initializers register scripts on load)
    AXON_ASSERT(PlatformAPI_ && "InitGameScript called before PlatformAPI_ set");
    AXON_ASSERT(PlatformAPI_->DLLAPI && "PlatformAPI_->DLLAPI is null");
    AxPlatformDLLAPI* DLLAPI = PlatformAPI_->DLLAPI;
    if (!DLLAPI) {
        AX_LOG(ERROR, "DLLAPI not available");
        return (false);
    }
    GameDLL_ = DLLAPI->Load("libGame.dll");
    if (!DLLAPI->IsValid(GameDLL_)) {
        AX_LOG(ERROR, "Failed to load Game DLL");
        return (false);
    }
#endif

    // Scripts are now registered (via static initializers in DLL or monolithic)
    const auto& Scripts = ScriptRegistry::Get().GetAll();
    if (Scripts.empty()) {
        AX_LOG(ERROR, "No scripts registered");
        return (false);
    }

    // Attach all registered scripts to the root node
    // TODO: When scene files support script references, this moves to the scene parser
    for (const auto& [Name, Factory] : Scripts) {
        ScriptBase* Script = Factory();
        if (Script) {
            SceneTree_->GetRootNode()->AttachScript(Script);
            AX_LOG(INFO, "Script '%s' attached to root node", Name.c_str());
        }
    }

    return (true);
}

bool AxEngine::Initialize(const AxEngineConfig* config, AxAPIRegistry* Registry)
{
    if (!config || !Registry) {
        return (false);
    }

    // Store editor-hosted mode state from config
    ExternalWindowHandle_ = config->ExternalWindowHandle;
    ViewportWidth_ = config->ViewportWidth;
    ViewportHeight_ = config->ViewportHeight;

    // Set default mode based on hosting mode:
    // Editor-hosted defaults to Edit; standalone defaults to Play
    if (IsEditorHosted()) {
        Mode_ = AxEngineMode::Edit;
    } else {
        Mode_ = AxEngineMode::Play;
    }

    APIRegistry_ = Registry;

    PluginAPI_ = static_cast<AxPluginAPI*>(APIRegistry_->Get(AXON_PLUGIN_API_NAME));
    if (!PluginAPI_) {
        AX_LOG(ERROR, "Failed to get PluginAPI from registry");
        return (false);
    }

    // Window initialization: skip when editor provides an external handle
    if (!IsEditorHosted()) {
        if (!InitWindow())
            return (false);
    }

    if (!LoadPlugins())
        return (false);
    if (!InitRenderer())
        return (false);

    InitInput();

    // Initialize scene parser for all modes (editor will call LoadScene/NewScene/SaveScene)
    SceneParser_.Init(APIRegistry_);

    // Scene loading: skip in editor-hosted mode (editor calls LoadScene/NewScene explicitly)
    if (!IsEditorHosted()) {
        if (!InitScene())
            return (false);

        if (!InitGameScript())
            return (false);

        WindowAPI_->SetWindowVisible(Window_, true, NULL);
    }

    isRunning_ = true;

    // Initialize LastFrameTime_ so the first Tick() call produces a valid delta.
    // In standalone mode, Run() also sets this, but for editor-hosted mode
    // where Tick() is called directly (without Run()), this is essential.
    LastFrameTime_ = PlatformAPI_->TimeAPI->WallTime();

    AX_LOG(INFO, "Initialization complete");
    return (true);
}

bool AxEngine::Tick()
{
    // Calculate frame time
    AxWallClock CurrentTime = PlatformAPI_->TimeAPI->WallTime();
    float DeltaT = PlatformAPI_->TimeAPI->ElapsedWallTime(LastFrameTime_, CurrentTime);
    LastFrameTime_ = CurrentTime;

    // Poll window events -- only when AxWindow is available (standalone mode)
    if (WindowAPI_ && Window_) {
        WindowAPI_->PollEvents(Window_);
        if (WindowAPI_->HasRequestedClose(Window_)) {
            isRunning_ = false;
            return (false);
        }
    }

    // Update input
    AxInput::Get().Update();

#if !defined(AX_SHIPPING)
    // Check for ESC to exit (dev-only, standalone only)
    if (!IsEditorHosted() && AxInput::Get().IsKeyPressed(AX_KEY_ESCAPE)) {
        isRunning_ = false;
        return (false);
    }
#endif

    // Propagate mouse delta to SceneTree for script access
    if (SceneTree_) {
        SceneTree_->UpdateMouseDelta(AxInput::Get().GetMouseDelta());
    }

    // In Edit mode, skip fixed-update and late-update entirely.
    // SceneTree::Update() internally skips script dispatch when
    // scripts are disabled, but still propagates transforms.
    if (Mode_ == AxEngineMode::Play) {
        // Fixed-timestep update (Play mode only)
        FixedAccumulator_ += DeltaT;
        while (FixedAccumulator_ >= FixedTimestep_) {
            // PhysicsServer::Tick -- stub placeholder (runs BEFORE scripts, Godot ordering)
            if (SceneTree_) {
                SceneTree_->FixedUpdate(FixedTimestep_);
            }
            FixedAccumulator_ -= FixedTimestep_;
        }
    }

    // Variable-rate update (always runs -- transform propagation needed for rendering)
    if (SceneTree_) {
        SceneTree_->Update(DeltaT);
    }

    if (Mode_ == AxEngineMode::Play) {
        // Late update (Play mode only)
        if (SceneTree_) {
            SceneTree_->LateUpdate(DeltaT);
        }
    }

    // Render
    if (Renderer_) {
        Renderer_->BeginFrame();
        Renderer_->RenderScene(SceneTree_);
        Renderer_->FlushDebugDraw();
        Renderer_->EndFrame();
    }

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

void AxEngine::Resize(int32_t Width, int32_t Height)
{
    if (Renderer_) {
        Renderer_->Resize(Width, Height);

        // Update editor camera projection if active
        if (Renderer_->IsUsingEditorCamera()) {
            Renderer_->SetEditorCameraView(
                EditorCamera_.Position, EditorCamera_.Target,
                EditorCamera_.FOV, EditorCamera_.Near, EditorCamera_.Far);
        }
    }
}

void AxEngine::SetMode(AxEngineMode Mode)
{
    Mode_ = Mode;

    // Propagate script execution state to SceneTree
    if (SceneTree_) {
        SceneTree_->SetScriptsEnabled(Mode == AxEngineMode::Play);
    }
}

void AxEngine::Shutdown()
{
    isRunning_ = false;

    // Shutdown input
    AxInput::Get().Shutdown();

    // Release scene BEFORE unloading Game DLL. The scene tree contains nodes
    // with scripts and signal connections whose code lives in libGame.dll.
    // All nodes (and their std::function captures) must be destroyed while
    // the DLL is still loaded.
    UnloadScene();

    // Clear the script registry
    ScriptRegistry::Get().Clear();

#if !defined(AX_SHIPPING)
    // Unload Game DLL (scene is already destroyed)
    if (PlatformAPI_ && PlatformAPI_->DLLAPI && PlatformAPI_->DLLAPI->IsValid(GameDLL_)) {
        PlatformAPI_->DLLAPI->Unload(GameDLL_);
        GameDLL_ = {};
    }
#endif

    // Discard any scene snapshot
    SceneSnapshot_.clear();

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

    // Destroy window (only in standalone mode)
    if (Window_ && WindowAPI_) {
        WindowAPI_->DestroyWindow(Window_);
        Window_ = nullptr;
    }
}

AxEngine::~AxEngine()
{
    if (isRunning_) {
        Shutdown();
    }
}

//=============================================================================
// Scene Loading (Internal helper)
//=============================================================================

SceneTree* AxEngine::LoadScene(const char* FilePath)
{
    AXON_ASSERT(FilePath && "LoadScene called with null FilePath");
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
    AXON_ASSERT(Scene && "LoadSceneModels called with null Scene");
    AXON_ASSERT(ResourceAPI_ && "LoadSceneModels called before ResourceAPI_ init");
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
        if (!MI || MI->MeshPath.empty()) {
            continue;
        }

        if (AX_HANDLE_IS_VALID(MI->ModelHandle)) {
            continue;
        }

        AxModelHandle Handle = ResourceAPI_->LoadModel(MI->MeshPath.c_str());
        if (AX_HANDLE_IS_VALID(Handle)) {
            MI->ModelHandle = Handle;
            AX_LOG(DEBUG, "Loaded model '%s' -> [%u:%u]",
                   MI->MeshPath.c_str(), Handle.Index, Handle.Generation);
        } else {
            AX_LOG(ERROR, "Failed to load model '%s'", MI->MeshPath.c_str());
        }
    }
}

void AxEngine::UnloadScene()
{
    AXON_ASSERT(SceneTree_ && "UnloadScene called with no scene loaded");
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
// Runtime Scene Management API
//=============================================================================

bool AxEngine::LoadSceneFromPath(const char* Path)
{
    if (!Path) {
        AX_LOG(ERROR, "LoadSceneFromPath: NULL path");
        return (false);
    }

    // Cleanup existing scene
    UnloadScene();

    // Load the new scene
    SceneTree_ = LoadScene(Path);
    if (!SceneTree_) {
        AX_LOG(ERROR, "Failed to load scene from '%s'", Path);
        return (false);
    }

    // Set up the main camera from the loaded scene
    uint32_t CamCount = 0;
    Node** CamNodes = SceneTree_->GetNodesByType(NodeType::Camera, &CamCount);
    if (CamNodes && CamCount > 0) {
        CameraNode* SceneCam = static_cast<CameraNode*>(CamNodes[0]);
        if (Renderer_) {
            Renderer_->SetMainCamera(SceneCam);
        }
        SceneTree_->SetMainCamera(SceneCam);
    }

    // Propagate current mode and debug draw to the new scene tree
    SceneTree_->SetScriptsEnabled(Mode_ == AxEngineMode::Play);


    AX_LOG(INFO, "Scene loaded from '%s'", Path);
    return (true);
}

void AxEngine::UnloadCurrentScene()
{
    // Clear main camera from renderer before unloading
    if (Renderer_) {
        Renderer_->SetMainCamera(nullptr);
    }

    UnloadScene();

    AX_LOG(INFO, "Scene unloaded");
}

void AxEngine::NewScene()
{
    // Cleanup existing scene
    if (Renderer_) {
        Renderer_->SetMainCamera(nullptr);
    }
    UnloadScene();

    // Get the HashTableAPI for creating the new SceneTree
    AxHashTableAPI* HashTableAPI = static_cast<AxHashTableAPI*>(
        APIRegistry_->Get(AXON_HASH_TABLE_API_NAME));
    if (!HashTableAPI) {
        AX_LOG(ERROR, "NewScene: Failed to get HashTableAPI");
        return;
    }

    // Create a fresh scene tree with just a root node
    SceneTree_ = new SceneTree(HashTableAPI, nullptr);
    SceneTree_->Name = "NewScene";

    // Propagate current mode and debug draw to the new scene tree
    SceneTree_->SetScriptsEnabled(Mode_ == AxEngineMode::Play);


    AX_LOG(INFO, "New empty scene created");
}

bool AxEngine::SaveCurrentScene(const char* Path)
{
    if (!Path) {
        AX_LOG(ERROR, "SaveCurrentScene: NULL path");
        return (false);
    }

    if (!SceneTree_) {
        AX_LOG(ERROR, "SaveCurrentScene: No scene loaded");
        return (false);
    }

    bool Result = SceneParser_.SaveSceneToFile(SceneTree_, Path);
    if (Result) {
        AX_LOG(INFO, "Scene saved to '%s'", Path);
    } else {
        AX_LOG(ERROR, "Failed to save scene to '%s': %s", Path,
               SceneParser_.GetLastError() ? SceneParser_.GetLastError() : "Unknown error");
    }

    return (Result);
}

//=============================================================================
// Play Mode Scene Snapshot & Restore
//=============================================================================

void AxEngine::SnapshotScene()
{
    if (!SceneTree_) {
        AX_LOG(ERROR, "SnapshotScene: No scene loaded");
        SceneSnapshot_.clear();
        return;
    }

    SceneSnapshot_ = SceneParser_.SaveSceneToString(SceneTree_);
    if (SceneSnapshot_.empty()) {
        AX_LOG(ERROR, "SnapshotScene: Failed to serialize scene");
    } else {
        AX_LOG(INFO, "Scene snapshot captured (%zu bytes)", SceneSnapshot_.size());
    }
}

void AxEngine::RestoreSnapshot()
{
    if (SceneSnapshot_.empty()) {
        AX_LOG(ERROR, "RestoreSnapshot: No snapshot to restore");
        return;
    }

    // Clear main camera from renderer before unloading
    if (Renderer_) {
        Renderer_->SetMainCamera(nullptr);
    }

    // Unload the current scene (releases model handles, destroys tree)
    UnloadScene();

    // Re-parse the scene from the snapshot string
    SceneTree_ = SceneParser_.LoadSceneFromString(SceneSnapshot_.c_str());
    if (!SceneTree_) {
        AX_LOG(ERROR, "RestoreSnapshot: Failed to parse snapshot");
        return;
    }

    // Reload models for the restored scene (handles were released during unload)
    LoadSceneModels(SceneTree_);

    // Set up the main camera from the restored scene
    uint32_t CamCount = 0;
    Node** CamNodes = SceneTree_->GetNodesByType(NodeType::Camera, &CamCount);
    if (CamNodes && CamCount > 0) {
        CameraNode* SceneCam = static_cast<CameraNode*>(CamNodes[0]);
        if (Renderer_) {
            Renderer_->SetMainCamera(SceneCam);
        }
        SceneTree_->SetMainCamera(SceneCam);
    }

    // Propagate current mode and debug draw to the restored scene tree
    SceneTree_->SetScriptsEnabled(Mode_ == AxEngineMode::Play);


    AX_LOG(INFO, "Scene restored from snapshot");
}

void AxEngine::EnterPlayMode()
{
    if (Mode_ == AxEngineMode::Play) {
        AX_LOG(WARNING, "EnterPlayMode: Already in Play mode");
        return;
    }

    // Snapshot the current scene before entering play
    SnapshotScene();

    // Reset fixed-timestep accumulator
    FixedAccumulator_ = 0.0f;

    // Initialize LastFrameTime_ to avoid a large first-frame delta
    if (PlatformAPI_) {
        LastFrameTime_ = PlatformAPI_->TimeAPI->WallTime();
    }

    // Switch renderer to use scene's game camera instead of editor camera
    if (Renderer_) {
        Renderer_->SetUseEditorCamera(false);
    }

    // Switch to Play mode (enables scripts in SceneTree)
    SetMode(AxEngineMode::Play);

    AX_LOG(INFO, "Entered Play mode");
}

void AxEngine::ExitPlayMode()
{
    if (Mode_ == AxEngineMode::Edit) {
        AX_LOG(WARNING, "ExitPlayMode: Already in Edit mode");
        return;
    }

    // Switch to Edit mode (disables scripts in SceneTree)
    SetMode(AxEngineMode::Edit);

    // Restore the scene from the snapshot (discards play-time modifications)
    RestoreSnapshot();

    // Discard the snapshot
    SceneSnapshot_.clear();

    // Switch renderer back to editor camera
    if (Renderer_) {
        Renderer_->SetUseEditorCamera(true);
        Renderer_->SetEditorCameraView(
            EditorCamera_.Position, EditorCamera_.Target,
            EditorCamera_.FOV, EditorCamera_.Near, EditorCamera_.Far);
    }

    AX_LOG(INFO, "Exited Play mode");
}

//=============================================================================
// Editor Camera
//=============================================================================

void AxEngine::SetEditorCamera(float PosX, float PosY, float PosZ,
                               float TargetX, float TargetY, float TargetZ,
                               float FOV)
{
    EditorCamera_.Position = {PosX, PosY, PosZ};
    EditorCamera_.Target = {TargetX, TargetY, TargetZ};
    EditorCamera_.FOV = FOV;

    // Update the renderer's editor camera matrices if it exists
    if (Renderer_) {
        Renderer_->SetEditorCameraView(
            EditorCamera_.Position, EditorCamera_.Target,
            EditorCamera_.FOV, EditorCamera_.Near, EditorCamera_.Far);
    }
}

