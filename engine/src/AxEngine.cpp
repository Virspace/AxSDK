#include "AxEngine/AxEngine.h"
#include "AxEngine/AxResource.h"
#include "AxEngine/AxShaderManager.h"

#include "AxOpenGL/AxOpenGL.h"
#include "AxScene/AxScene.h"
#include "Foundation/AxApplication.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxPlatform.h"
#include "Foundation/AxMath.h"
#include "Foundation/AxHashTable.h"
#include "Foundation/AxPlugin.h"
#include "AxWindow/AxWindow.h"

#define AXARRAY_IMPLEMENTATION
#include "Foundation/AxArray.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// STB Image for texture loading
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// CGLTF for GLTF model loading
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

// Forward declaration of global API registry (defined at end of file)
static AxAPIRegistry* gAPIRegistry = nullptr;

static const int32_t DefaultWindowWidth = 1280;
static const int32_t DefaultWindowHeight = 720;

bool AxEngine::LoadPlugins()
{
    AXON_ASSERT(PluginAPI && "PluginAPI is NULL in AxEngine::LoadPlugins!");

    auto LoadPluginOrFail = [&](const char* PluginName, const char* ErrorMessage) -> bool {
        if (!PluginAPI->Load(PluginName, false)) {
            fprintf(stderr, "Failed to load %s plugin\n", PluginName);
            WindowAPI->CreateMessageBox(
                Window,
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
    RenderAPI = static_cast<AxOpenGLAPI *>(APIRegistry->Get(AXON_OPENGL_API_NAME));
    SceneAPI = static_cast<AxSceneAPI *>(APIRegistry->Get(AXON_SCENE_API_NAME));
    PlatformAPI = static_cast<AxPlatformAPI*>(APIRegistry->Get(AXON_PLATFORM_API_NAME));

    if (!WindowAPI || !RenderAPI || !SceneAPI || !PlatformAPI) {
        return (false);
    }

    return (true);
}

///////////////////////////////////////////////////////////////
// Lifecycle Functions
///////////////////////////////////////////////////////////////
AxEngine::AxEngine()
{
    // APIRegistry will be set in Initialize()
}

bool AxEngine::Initialize(const AxEngineConfig* config)
{
    if (!config) {
        return (false);
    }

    // Use global APIRegistry from LoadPlugin
    APIRegistry = gAPIRegistry;
    if (!APIRegistry) {
        fprintf(stderr, "Failed to get API registry - LoadPlugin not called?\n");
        return (false);
    }

    // Get PluginAPI from registry (required for loading plugins)
    PluginAPI = static_cast<AxPluginAPI*>(APIRegistry->Get(AXON_PLUGIN_API_NAME));
    if (!PluginAPI) {
        fprintf(stderr, "Failed to get PluginAPI from registry\n");
        return (false);
    }

    // Load Window plugin
    if (!PluginAPI->Load("libAxWindow.dll", false)) {
        fprintf(stderr, "Failed to load AxWindow plugin\n");
        return false;
    }

    // Load and initialize Window API
    WindowAPI = static_cast<AxWindowAPI *>(APIRegistry->Get(AXON_WINDOW_API_NAME));
    WindowAPI->Init();

    // Initialize Window
    AxWindowConfig WindowConfig = {
        "AxonSDK OpenGL Example",
        100,
        100,
        DefaultWindowWidth,
        DefaultWindowHeight,
        (AxWindowStyle)(AX_WINDOW_STYLE_DECORATED | AX_WINDOW_STYLE_RESIZABLE)
    };

    enum AxWindowError Error = AX_WINDOW_ERROR_NONE;
    Window = WindowAPI->CreateWindowWithConfig(&WindowConfig, &Error);
    if (Error != AX_WINDOW_ERROR_NONE) {
        return (false);
    }

    // Load all engine plugins
    if (!LoadPlugins()) {
        return (false);
    }

    // Initialize Renderer
    Viewport = new AxViewport();
    Viewport->Position = { 0.0f, 0.0f };
    Viewport->Size = { DefaultWindowWidth, DefaultWindowHeight };
    Viewport->Scale = { 1.0f, 1.0f };
    Viewport->Depth = { 0.0f, 1.0f };
    Viewport->IsActive = true;
    Viewport->ClearColor = { 0.42f, 0.51f, 0.54f, 0.0f };

    AxWindowPlatformData WindowPlatformData = WindowAPI->GetPlatformData(Window);
    RenderAPI->CreateContext(WindowPlatformData.Win32.Handle);

    // Initialize model storage hash table
    AxHashTableAPI* HashTableAPI = static_cast<AxHashTableAPI *>(APIRegistry->Get(AXON_HASH_TABLE_API_NAME));
    if (HashTableAPI) {
        LoadedModels = HashTableAPI->CreateTable();
    }

    WindowAPI->SetWindowVisible(Window, true, NULL);
    isRunning_ = true;

    return (true);
}

bool AxEngine::Tick()
{
    // Calculate frame time
    LastFrameTime = PlatformAPI->TimeAPI->WallTime();
    AxWallClock CurrentTime = PlatformAPI->TimeAPI->WallTime();
    float DeltaT = PlatformAPI->TimeAPI->ElapsedWallTime(LastFrameTime, CurrentTime);
    LastFrameTime = CurrentTime;

    // Poll window events
    WindowAPI->PollEvents(Window);
    if (WindowAPI->HasRequestedClose(Window)) {
        isRunning_ = false;
        return (false);
    }

    // Render
    RenderAPI->NewFrame();
    RenderAPI->SetActiveViewport(Viewport);
    RenderAPI->SwapBuffers();

    return (true);
}

int AxEngine::Run()
{
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

    // Destroy renderer context
    if (RenderAPI) {
        RenderAPI->DestroyContext();
    }

    // Destroy window
    if (Window && WindowAPI) {
        WindowAPI->DestroyWindow(Window);
    }

    // Release all shader handles
    AxShaderManager& ShaderManager = AxShaderManager::GetInstance();
    if (CompiledShaders)
    {
        size_t ShaderCount = ArraySize(CompiledShaders);
        for (size_t i = 0; i < ShaderCount; i++) {
            if (ShaderManager.IsValid(CompiledShaders[i])) {
                ShaderManager.Release(CompiledShaders[i]);
            }
        }
        ArrayFree(CompiledShaders);
        CompiledShaders = NULL;
    }

    // Clean up hash table
    if (LoadedModels && APIRegistry) {
        struct AxHashTableAPI* HashTableAPI = (struct AxHashTableAPI*)APIRegistry->Get(AXON_HASH_TABLE_API_NAME);
        if (HashTableAPI) {
            HashTableAPI->DestroyTable(LoadedModels);
            LoadedModels = NULL;
        }
    }
}

AxEngine::~AxEngine()
{
    if (isRunning_) {
        Shutdown();
    }
}

///////////////////////////////////////////////////////////////
// Orchestration Functions
///////////////////////////////////////////////////////////////

AxScene* AxEngine::GetScene()
{
    return (Scene_);
}

// Global engine instance for DLL
static AxEngine* gEngine = nullptr;

// Plugin load function - called by PluginAPI when DLL is loaded
extern "C" AXENGINE_API void LoadPlugin(AxAPIRegistry* Registry, bool Load)
{
    if (Load) {
        // Store global registry for engine to use
        gAPIRegistry = Registry;

        // Register AxEngineAPI in the registry
        // (Host will retrieve it via Registry->Get(AX_ENGINE_API_NAME))
        if (Registry) {
            // Note: We don't register GetEngineAPI() directly since the host
            // will call it via DLL symbol lookup
        }
    }  else {
        // Unload: cleanup global state
        if (gEngine) {
            delete gEngine;
            gEngine = nullptr;
        }
        gAPIRegistry = nullptr;
    }
}

// Exported API for host applications
extern "C" AXENGINE_API AxEngineAPI* GetEngineAPI() {
    static AxEngineAPI api = {
        .Initialize = [](const AxEngineConfig* cfg) -> bool {
            if (!gEngine) {
                gEngine = new AxEngine();
            }
            return (gEngine->Initialize(cfg));
        },
        .Run = []() -> int {
            if (!gEngine) {
                return (1);
            }
            return (gEngine->Run());
        },
        .Shutdown = []() {
            if (gEngine) {
                gEngine->Shutdown();
                delete gEngine;
                gEngine = nullptr;
            }
        },
        .IsRunning = []() -> bool {
            return (gEngine && gEngine->IsRunning());
        }
    };
    return (&api);
}
