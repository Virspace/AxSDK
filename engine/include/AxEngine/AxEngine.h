#pragma once

#include "Foundation/AxTypes.h"
#include "Foundation/AxPlatform.h"
#include "AxEngine/AxSceneParser.h"
#include <string>

#define AX_ENGINE_API_NAME "AxEngineAPI"

struct AxAPIRegistry;
struct AxWindowAPI;
struct AxResourceAPI;
struct AxPluginAPI;
struct AxPlatformAPI;

struct AxWindow;
class SceneTree;
class AxRenderer;

// Configuration for engine initialization
struct AxEngineConfig {
  std::string PluginPath;       // Directory containing plugins
  std::string ConfigPath;       // Path to engine config file (optional)
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

/**
 * AxEngine - Core engine
 *
 * Manages plugins, window, input, scripting, and rendering subsystems.
 */
class AxEngine
{
public:
    AxEngine();
    ~AxEngine();

    bool Initialize(const AxEngineConfig* config);
    int Run();
    void Shutdown();
    bool IsRunning() const { return (isRunning_); }

    bool Tick();

private:
    bool InitWindow();
    bool LoadPlugins();
    bool InitRenderer();
    void InitInput();
    bool InitScene();
    void InitGameScript();

    // Core APIs
    AxAPIRegistry* APIRegistry_{nullptr};
    AxWindowAPI* WindowAPI_{nullptr};
    AxResourceAPI* ResourceAPI_{nullptr};
    AxPlatformAPI* PlatformAPI_{nullptr};
    AxPluginAPI* PluginAPI_{nullptr};

    // Window
    AxWindow* Window_{nullptr};
    AxWallClock LastFrameTime_;
    float FixedAccumulator_{0.0f};
    static constexpr float FixedTimestep_ = 1.0f / 60.0f;

    // Scene parser
    SceneParser SceneParser_;

    // Subsystems
    AxRenderer* Renderer_{nullptr};
    SceneTree* SceneTree_{nullptr};

    // Scene loading
    SceneTree* LoadScene(const char* FilePath);
    void LoadSceneModels(SceneTree* Scene);
    void UnloadScene();

    // Game script DLL
    AxDLL GameDLL_;

    bool isRunning_{false};
};
