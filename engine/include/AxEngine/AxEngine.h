#pragma once

#include "Foundation/AxTypes.h"

// DLL export/import macro
#ifdef AXENGINE_EXPORTS
  #define AXENGINE_API AXON_DLL_EXPORT
#else
  #define AXENGINE_API AXON_DLL_IMPORT
#endif

#define AX_ENGINE_API_NAME "AxEngineAPI"

struct AxAPIRegistry;
struct AxWindowAPI;
struct AxOpenGLAPI;
struct AxSceneAPI;
struct AxResourceAPI;
struct AxPluginAPI;
struct AxPlatformAPI;
struct AxShaderHandle;

struct AxWindow;
struct AxHashTable;
struct AxScene;

// Configuration for engine initialization
struct AxEngineConfig {
  const char* PluginPath;       // Directory containing plugins
  const char* ConfigPath;       // Path to engine config file (optional)
  int argc;                     // Command-line argument count
  char** argv;                  // Command-line arguments
};

// Engine API exposed to host applications
struct AxEngineAPI {
  // Initialize engine with configuration
  bool (*Initialize)(const AxEngineConfig* config);

  // Run main loop (blocks until exit)
  int (*Run)();

  // Shutdown and cleanup
  void (*Shutdown)();

  // Query engine state
  bool (*IsRunning)();
};

// Get engine API (entry point for hosts)
extern "C" AXENGINE_API AxEngineAPI* GetEngineAPI();

class AxEngine
{
public:
    AxEngine();
    ~AxEngine();

    bool Initialize(const AxEngineConfig* config);
    int Run();
    void Shutdown();
    bool IsRunning() const { return (isRunning_); }

    /**
     * Update engine systems (future: resource streaming, etc.)
     * @param Engine Engine handle
     */
    bool Tick();

    /**
     * Get currently loaded scene
     * @param Engine Engine handle
     * @return AxScene pointer or NULL if no scene loaded
     */
    AxScene* GetScene();

private:
    bool LoadPlugins();
    void CreateViewport();
    bool InitWindow();
    bool InitRenderer();
    bool InitializeSubsystems();
    bool CreateApplication();

    AxAPIRegistry *APIRegistry{nullptr};
    AxWindowAPI *WindowAPI{nullptr};
    AxOpenGLAPI *RenderAPI{nullptr};
    AxSceneAPI *SceneAPI{nullptr};
    AxResourceAPI *ResourceAPI{nullptr};
    AxPlatformAPI* PlatformAPI{nullptr};
    AxPluginAPI* PluginAPI{nullptr};

    AxWindow *Window{nullptr};
    AxViewport *Viewport{nullptr};
    AxScene* Scene_{nullptr};
    AxHashTable* LoadedModels;
    AxShaderHandle* CompiledShaders{nullptr};
    AxWallClock LastFrameTime;

    bool isRunning_{false};
};
