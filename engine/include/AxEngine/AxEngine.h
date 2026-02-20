#pragma once

#include "Foundation/AxTypes.h"
#include <string>

#define AX_ENGINE_API_NAME "AxEngineAPI"

struct AxAPIRegistry;
struct AxWindowAPI;
struct AxResourceAPI;
struct AxPluginAPI;
struct AxPlatformAPI;

struct AxWindow;
struct AxScene;
class AxScripting;
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
 * AxEngine - Core engine orchestrator
 *
 * Manages plugins, window, input, scripting, and rendering subsystems.
 * Rendering is delegated to AxRender.
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
    bool LoadPlugins();

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

    // Subsystems
    AxRenderer* Renderer_{nullptr};
    AxScripting* Scripting_{nullptr};

    bool isRunning_{false};
};
