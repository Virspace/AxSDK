#pragma once

#include "Foundation/AxTypes.h"
#include "Foundation/AxPlatform.h"
#include "AxEngine/AxSceneParser.h"
#include <string>

struct AxAPIRegistry;
struct AxWindowAPI;
struct AxResourceAPI;
struct AxPluginAPI;
struct AxPlatformAPI;

struct AxWindow;
class SceneTree;
class AxRenderer;

/**
 * AxEngineMode - Engine operating mode.
 *
 * Edit: Scene renders with transform propagation, but scripts, physics,
 *       and fixed/late update are skipped. Used by the editor for static
 *       scene manipulation.
 * Play: Full simulation -- all scripts and update phases run normally.
 *       Identical to standalone behavior.
 */
enum class AxEngineMode : int32_t {
  Edit = 0,
  Play = 1
};

/**
 * EditorCameraState - Separate camera state for editor viewport navigation.
 *
 * This camera is independent of any scene CameraNode. It provides the
 * view/projection matrices during Edit mode so the editor viewport can
 * be navigated without modifying the scene.
 */
struct EditorCameraState {
  AxVec3 Position{0.0f, 5.0f, 10.0f};
  AxVec3 Target{0.0f, 0.0f, 0.0f};
  float FOV{60.0f};
  float Near{0.1f};
  float Far{1000.0f};
};

// Configuration for engine initialization
struct AxEngineConfig {
  std::string PluginPath;       // Directory containing plugins
  std::string ConfigPath;       // Path to engine config file (optional)
  int argc;                     // Command-line argument count
  char** argv;                  // Command-line arguments

  // Editor-hosted mode: when non-zero, the engine renders into this
  // externally-owned HWND instead of creating its own window via AxWindow.
  // AxWindow plugin loading, window creation, and cursor mode setup are
  // skipped entirely. The editor owns the window lifecycle.
  uint64_t ExternalWindowHandle{0};

  // Viewport dimensions for editor-hosted mode. Used when
  // ExternalWindowHandle is non-zero to initialize the renderer.
  // Ignored in standalone mode (uses default window dimensions).
  int32_t ViewportWidth{1280};
  int32_t ViewportHeight{720};
};

/**
 * AxEngine - Core engine
 *
 * Manages plugins, window, input, scripting, and rendering subsystems.
 * Supports two hosting modes:
 *   - Standalone: engine creates its own window and runs the frame loop
 *   - Editor-hosted: editor supplies an HWND and drives Tick() externally
 */
class AxEngine
{
public:
    AxEngine();
    ~AxEngine();

    bool Initialize(const AxEngineConfig* config, AxAPIRegistry* Registry);
    int Run();
    void Shutdown();
    bool IsRunning() const { return (isRunning_); }

    bool Tick();

    // Resize renderer viewport and update camera aspect ratio
    void Resize(int32_t Width, int32_t Height);

    // Set the engine operating mode (Edit or Play)
    void SetMode(AxEngineMode Mode);

    // Query the current engine mode
    AxEngineMode GetMode() const { return (Mode_); }

    // Returns true when running in editor-hosted mode (external window handle)
    bool IsEditorHosted() const { return (ExternalWindowHandle_ != 0); }

    // === Runtime Scene Management ===

    /**
     * Load a .ats scene file, replacing the current scene tree.
     * Handles cleanup of the old scene before loading the new one.
     * After loading, sets up the main camera from the scene if present.
     * @param Path Path to the .ats scene file.
     * @return true on success, false on failure.
     */
    bool LoadSceneFromPath(const char* Path);

    /**
     * Unload the current scene, leaving the engine running with an empty
     * scene (nullptr SceneTree). Releases all model handles and destroys
     * the scene tree.
     */
    void UnloadCurrentScene();

    /**
     * Create a fresh empty scene tree with a root node, replacing any
     * existing scene. The new scene has default settings and no nodes
     * other than the root.
     */
    void NewScene();

    /**
     * Save the current scene tree to a .ats file.
     * @param Path Path to the output .ats file.
     * @return true on success, false on failure.
     */
    bool SaveCurrentScene(const char* Path);

    // === Play Mode Scene Snapshot & Restore ===

    /**
     * Serialize the current scene tree to an in-memory string buffer
     * using the .ats format. The snapshot is stored internally.
     */
    void SnapshotScene();

    /**
     * Unload the current scene and reload from the in-memory snapshot
     * buffer. Restores the scene to its pre-play state.
     */
    void RestoreSnapshot();

    /**
     * Enter play mode: takes a scene snapshot, resets the fixed-timestep
     * accumulator, switches mode to Play, and initializes LastFrameTime_.
     * The renderer switches to the scene's game camera.
     */
    void EnterPlayMode();

    /**
     * Exit play mode: switches mode to Edit, restores the scene from
     * the snapshot, discards the snapshot. The renderer switches back
     * to the editor camera.
     */
    void ExitPlayMode();

    // === Editor Camera ===

    /**
     * Set the editor camera parameters. The editor camera provides the
     * view/projection matrices during Edit mode.
     * @param PosX/PosY/PosZ Camera position
     * @param TargetX/TargetY/TargetZ Look-at target
     * @param FOV Field of view in degrees
     */
    void SetEditorCamera(float PosX, float PosY, float PosZ,
                         float TargetX, float TargetY, float TargetZ,
                         float FOV);

    /** Get the current editor camera state (read-only). */
    const EditorCameraState& GetEditorCameraState() const { return (EditorCamera_); }

private:
    bool InitWindow();
    bool LoadPlugins();
    bool InitRenderer();
    void InitInput();
    bool InitScene();
    bool InitGameScript();

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

    // Editor-hosted mode
    uint64_t ExternalWindowHandle_{0};
    int32_t ViewportWidth_{1280};
    int32_t ViewportHeight_{720};
    AxEngineMode Mode_{AxEngineMode::Play};

    // Scene parser
    SceneParser SceneParser_;

    // Subsystems
    AxRenderer* Renderer_{nullptr};
    SceneTree* SceneTree_{nullptr};

    // Internal scene loading helper (creates SceneTree from file)
    SceneTree* LoadScene(const char* FilePath);
    void LoadSceneModels(SceneTree* Scene);
    void UnloadScene();

    // Play mode scene snapshot (in-memory .ats string)
    std::string SceneSnapshot_;

    // Editor camera state (separate from scene CameraNodes)
    EditorCameraState EditorCamera_;

    // Game script DLL
    AxDLL GameDLL_;

    bool isRunning_{false};
};
