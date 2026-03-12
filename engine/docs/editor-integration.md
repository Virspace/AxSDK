# AxEngine Editor Integration Guide

> Version: 1.0.0
> Last Updated: 2026-03-11

This document describes how to integrate AxEngine into an external editor application (AxEditor). The engine exposes its functionality through the `AxEngineAPI` function-pointer table, which the editor retrieves from the Foundation API registry after initialization. The editor owns the window, the frame loop, and the overall application lifecycle. The engine renders into an editor-provided HWND and advances one frame per `Tick()` call.

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Bootstrapping the Engine](#bootstrapping-the-engine)
3. [Frame Loop](#frame-loop)
4. [Viewport Resize](#viewport-resize)
5. [Editor Camera](#editor-camera)
6. [Edit and Play Modes](#edit-and-play-modes)
7. [Scene Management](#scene-management)
8. [Programmatic Scene Manipulation](#programmatic-scene-manipulation)
9. [Resource Lifecycle](#resource-lifecycle)
10. [Shutdown](#shutdown)
11. [Constraints and Ordering Requirements](#constraints-and-ordering-requirements)
12. [Complete Example: Editor Main Loop](#complete-example-editor-main-loop)

---

## Architecture Overview

```
+------------------+          +---------------------+
|    AxEditor      |          |     AxEngine        |
|  (owns window)   |  calls   |  (shared library)   |
|  (owns frame     | -------> |  AxEngineAPI:       |
|   loop)          |          |    Initialize()     |
|  (owns UI)       |          |    Tick()           |
|                  |          |    Shutdown()       |
|  Creates HWND    |          |    Resize()         |
|  Passes to       |          |    SetEditorCamera()|
|  engine config   |          |    EnterPlayMode()  |
+------------------+          |    ExitPlayMode()   |
                              |    LoadScene()      |
                              |    SaveScene()      |
                              |    NewScene()       |
                              |    UnloadScene()    |
                              |    SetMode()        |
                              |    IsRunning()      |
                              +---------------------+
```

Key architectural points:

- **AxWindow plugin is NOT loaded** in editor-hosted mode. The editor owns the window.
- **AxInput receives nullptr** and no-ops on `Update()`. The editor handles input separately and translates it into engine API calls (camera movement, play/pause, etc.).
- **Scene loading is NOT automatic.** The editor calls `LoadScene()`, `NewScene()`, or `SaveScene()` explicitly.
- **Game script DLL is NOT auto-loaded.** The editor manages script compilation and hot-reload.
- **ESC-to-quit is disabled** in editor-hosted mode.
- The engine defaults to **Edit mode** when editor-hosted (standalone defaults to Play mode).

---

## Bootstrapping the Engine

Before using the engine, the editor must initialize Foundation (the API registry, platform APIs, allocators) and then load the engine plugin. In Debug/Development builds, the engine is a shared library (`libAxEngine.dll`). In Shipping builds, it is statically linked.

### Required Headers

```cpp
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxPlugin.h"
#include "AxEngine/AxEngine.h"
#include "AxLog/AxLog.h"
```

### Initialization Sequence

Foundation must be initialized before the engine. The engine depends on Foundation APIs (Platform, Plugin, Allocator, HashTable) being registered in the global API registry.

```cpp
// Step 1: Initialize Foundation's global API registry
AxonInitGlobalAPIRegistry();
AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);

// Step 2: Open the log file (optional but recommended)
AxLogOpenFile("editor.log");

// Step 3: Get the PluginAPI to load engine DLL
AxPluginAPI* PluginAPI = static_cast<AxPluginAPI*>(
    AxonGlobalAPIRegistry->Get(AXON_PLUGIN_API_NAME)
);

// Step 4: Load AxEngine.dll
uint64_t EngineHandle = PluginAPI->Load("libAxEngine.dll", false);
if (!PluginAPI->IsValid(EngineHandle)) {
    // Handle error: engine DLL not found
    AxLogCloseFile();
    AxonTermGlobalAPIRegistry();
    return 1;
}

// Step 5: Get the engine API from the registry
AxEngineAPI* Engine = static_cast<AxEngineAPI*>(
    AxonGlobalAPIRegistry->Get(AX_ENGINE_API_NAME)
);

if (!Engine) {
    // Handle error: engine API not registered
    PluginAPI->Unload(EngineHandle);
    AxLogCloseFile();
    AxonTermGlobalAPIRegistry();
    return 1;
}
```

### Configuring Editor-Hosted Mode

The key difference from standalone mode is the `ExternalWindowHandle` field. When this is non-zero, the engine skips AxWindow plugin loading and window creation entirely.

```cpp
// The editor creates its own viewport panel (e.g., an HWND from a docking panel)
HWND ViewportHWND = CreateEditorViewportPanel();  // Your editor code

AxEngineConfig Config{};
Config.PluginPath = "./plugins/";
Config.argc = argc;
Config.argv = argv;

// Editor-hosted mode: pass the editor's viewport HWND
Config.ExternalWindowHandle = reinterpret_cast<uint64_t>(ViewportHWND);
Config.ViewportWidth = 1280;   // Initial viewport width in pixels
Config.ViewportHeight = 720;   // Initial viewport height in pixels

if (!Engine->Initialize(&Config)) {
    Engine->Shutdown();
    // Handle error
    return 1;
}
```

### What Happens During Initialize()

When `ExternalWindowHandle` is non-zero, `Initialize()`:

1. Stores the external window handle and viewport dimensions
2. Sets the default mode to `AxEngineMode::Edit`
3. Skips AxWindow plugin loading (no `InitWindow()`)
4. Loads AxOpenGL and AxResource plugins
5. Initializes the renderer using the provided HWND and viewport dimensions
6. Initializes AxInput with nullptr (all input calls become no-ops)
7. Initializes the scene parser
8. Skips automatic scene loading (no `InitScene()`)
9. Skips automatic game script DLL loading (no `InitGameScript()`)
10. Records the current wall-clock time for accurate first-frame delta

After `Initialize()` returns, the engine is running with an empty viewport. The editor must call `LoadScene()` or `NewScene()` to populate the scene.

---

## Frame Loop

The editor owns the frame loop. Each iteration, the editor calls `Tick()` to advance the engine by one frame. The engine calculates delta time internally using wall-clock timestamps.

```cpp
// Editor main loop (simplified)
while (EditorIsRunning()) {
    // Process editor UI events (menus, panels, docking, etc.)
    ProcessEditorEvents();

    // Advance the engine by one frame
    // Tick() handles: input update, scene tree update, transform propagation,
    // script dispatch (Play mode only), rendering, and resource cleanup
    Engine->Tick();

    // Swap the editor window's back buffer
    // (Engine renders into the viewport panel, editor composites its UI)
    SwapEditorBuffers();
}
```

### What Tick() Does Each Frame

1. Calculates delta time from the last call
2. Polls window events (skipped in editor-hosted mode -- no AxWindow)
3. Updates AxInput (no-op in editor-hosted mode)
4. Propagates mouse delta to SceneTree (for script access)
5. **Play mode only:** Runs fixed-timestep updates (physics placeholder, `ScriptBase::OnFixedUpdate`)
6. Runs variable-rate update (transform propagation always runs; `ScriptBase::OnUpdate` runs in Play mode only)
7. **Play mode only:** Runs late update (`ScriptBase::OnLateUpdate`)
8. Renders the scene (using editor camera in Edit mode, game camera in Play mode)
9. Processes pending resource releases (deferred destruction)

### Frame Timing

The engine tracks wall-clock time internally. The first `Tick()` after `Initialize()` produces a valid delta time (typically near zero). There is no need to seed or reset the timer -- `Initialize()` records the starting timestamp.

---

## Viewport Resize

When the editor's viewport panel changes size (user resizes the docking panel, window maximizes, etc.), call `Resize()` before the next `Tick()`.

```cpp
void OnViewportResized(int32_t NewWidth, int32_t NewHeight)
{
    Engine->Resize(NewWidth, NewHeight);
}
```

`Resize()` updates the renderer's viewport dimensions and recalculates the camera aspect ratio. If the editor camera is active (Edit mode), its projection matrix is also updated.

**Ordering:** Call `Resize()` before `Tick()` when dimensions change. This ensures the frame renders at the correct resolution. Calling `Resize()` multiple times between `Tick()` calls is safe -- only the last dimensions are used.

---

## Editor Camera

The editor camera is independent of any scene `CameraNode`. It provides view and projection matrices during Edit mode so the editor viewport can be navigated (orbit, pan, zoom) without modifying the scene.

### Setting the Editor Camera

```cpp
// Position the editor camera looking at the scene origin
Engine->SetEditorCamera(
    0.0f, 5.0f, 10.0f,    // Camera position (x, y, z)
    0.0f, 0.0f, 0.0f,     // Look-at target (x, y, z)
    60.0f                   // Field of view in degrees
);
```

### Default Editor Camera

If `SetEditorCamera()` is never called, the engine uses these defaults:

| Parameter | Default Value |
|-----------|--------------|
| Position  | (0, 5, 10)   |
| Target    | (0, 0, 0)    |
| FOV       | 60 degrees   |
| Near Clip | 0.1          |
| Far Clip  | 1000.0       |

Near and far clip planes are not exposed through the API -- they use fixed defaults.

### Editor Camera and Mode Switching

- **Edit mode:** The editor camera is active. Scene `CameraNode` instances are ignored for rendering.
- **Play mode:** The scene's first `CameraNode` is used for rendering. The editor camera state is preserved and restored when returning to Edit mode.

### Orbit Camera Example

The editor implements orbit/pan/zoom by translating mouse input into `SetEditorCamera()` calls each frame:

```cpp
// Example: simple orbit camera (editor-side logic)
void UpdateEditorOrbitCamera(AxEngineAPI* Engine,
                              float Yaw, float Pitch, float Distance,
                              float TargetX, float TargetY, float TargetZ)
{
    float CosP = cosf(Pitch);
    float PosX = TargetX + Distance * CosP * sinf(Yaw);
    float PosY = TargetY + Distance * sinf(Pitch);
    float PosZ = TargetZ + Distance * CosP * cosf(Yaw);

    Engine->SetEditorCamera(PosX, PosY, PosZ,
                            TargetX, TargetY, TargetZ,
                            60.0f);
}
```

---

## Edit and Play Modes

The engine operates in one of two modes, controlled by the `AxEngineMode` enum:

```cpp
enum class AxEngineMode : int32_t {
    Edit = 0,   // Transforms propagate, scripts/physics skipped
    Play = 1    // Full simulation (identical to standalone)
};
```

### Edit Mode (Default for Editor)

- Transform propagation runs every `Tick()` -- the viewport always reflects current node positions
- Script `OnUpdate`, `OnFixedUpdate`, and `OnLateUpdate` are **not** called
- Script `OnInit` is **not** processed
- The editor camera provides view/projection matrices
- Scene can be modified programmatically (add/remove nodes, change transforms)

### Play Mode

- Full simulation: all script callbacks run (`OnInit`, `OnUpdate`, `OnFixedUpdate`, `OnLateUpdate`)
- Fixed-timestep accumulator drives physics and `OnFixedUpdate`
- The scene's game camera (first `CameraNode`) provides view/projection matrices
- Scene modifications by scripts happen normally

### Switching Modes Directly

Use `SetMode()` to switch modes without snapshot/restore behavior:

```cpp
Engine->SetMode(AxEngineMode::Play);  // Enable scripts
Engine->SetMode(AxEngineMode::Edit);  // Disable scripts
```

**Warning:** `SetMode()` does NOT snapshot or restore the scene. If scripts modify the scene during Play and you call `SetMode(Edit)`, those modifications persist. For proper play-mode behavior with scene preservation, use `EnterPlayMode()` and `ExitPlayMode()` instead.

### Play Mode with Snapshot/Restore

The recommended way to enter and exit play mode:

```cpp
// User presses "Play" button in editor toolbar
Engine->EnterPlayMode();
// Scene is now snapshotted. Scripts run. Game camera is active.

// ... user tests their game ...

// User presses "Stop" button in editor toolbar
Engine->ExitPlayMode();
// Scene is restored to its pre-play state. Editor camera is active.
// All runtime modifications by scripts are discarded.
```

### What EnterPlayMode() Does

1. Serializes the current scene tree to an in-memory `.ats` string (snapshot)
2. Resets the fixed-timestep accumulator to zero
3. Records the current wall-clock time (prevents a large first-frame delta)
4. Switches the renderer to use the scene's game camera
5. Sets the mode to `Play` (enables scripts in SceneTree)

### What ExitPlayMode() Does

1. Sets the mode to `Edit` (disables scripts)
2. Unloads the current scene (releases model handles, destroys tree)
3. Reloads the scene from the in-memory snapshot
4. Reloads model resources for the restored scene
5. Re-establishes the main camera from the restored scene
6. Discards the snapshot buffer
7. Switches the renderer back to the editor camera

### Important Notes

- Scripts start fresh on each `EnterPlayMode()`. Script runtime state (member variables, etc.) is **not** preserved across play/stop cycles. `OnInit` runs again on the next `EnterPlayMode()`.
- If `EnterPlayMode()` is called while already in Play mode, it logs a warning and returns without action.
- If `ExitPlayMode()` is called while already in Edit mode, it logs a warning and returns without action.

---

## Scene Management

The engine provides four scene management functions through `AxEngineAPI`. In editor-hosted mode, no scene is loaded automatically during `Initialize()` -- the editor must call one of these to populate the viewport.

### Loading a Scene from Disk

```cpp
bool Success = Engine->LoadScene("scenes/my_level.ats");
if (!Success) {
    // Handle error: file not found, parse error, etc.
}
```

`LoadScene()`:
- Unloads the current scene (releases model handles, destroys tree)
- Parses the `.ats` file and creates a new scene tree
- Loads model resources for all `MeshInstance` nodes
- Sets up the main camera from the first `CameraNode` in the scene
- Propagates the current mode (Edit/Play) to the new scene tree

### Creating a New Empty Scene

```cpp
Engine->NewScene();
// Scene now contains only a root node named "NewScene"
```

`NewScene()`:
- Unloads the current scene
- Creates a fresh `SceneTree` with a single root `Node`
- The scene name defaults to `"NewScene"`

### Saving the Current Scene

```cpp
bool Success = Engine->SaveScene("scenes/my_level.ats");
if (!Success) {
    // Handle error: write failure, no scene loaded, etc.
}
```

`SaveScene()`:
- Serializes the current scene tree to the `.ats` text format
- Writes the output to the specified file path
- Returns `false` if no scene is loaded or the write fails

### Unloading the Current Scene

```cpp
Engine->UnloadScene();
// Engine continues running with an empty viewport (no scene)
```

`UnloadScene()`:
- Releases all model handles for `MeshInstance` nodes
- Destroys the scene tree
- Clears the renderer's main camera reference
- The engine continues to run -- `Tick()` renders an empty frame

---

## Programmatic Scene Manipulation

While the engine's scene tree internals are C++ classes (not exposed through `AxEngineAPI`), the editor can manipulate scenes through two mechanisms:

### 1. File-Based Manipulation

Create or modify `.ats` files and load them:

```cpp
// Create a scene file programmatically
// .ats format is text-based and human-readable
const char* SceneContent = R"(
scene "MyScene" {
    node "Root" Node {
        node "MainCamera" Camera {
            fov: 60.0
            near: 0.1
            far: 1000.0
            position: 0.0 5.0 10.0
        }
        node "Ground" MeshInstance {
            mesh: "models/ground.gltf"
            position: 0.0 0.0 0.0
            scale: 10.0 1.0 10.0
        }
        node "Sun" Light {
            type: directional
            color: 1.0 0.95 0.9
            intensity: 1.0
            direction: -0.5 -1.0 -0.3
        }
    }
}
)";

// Write to disk and load
WriteFile("scenes/generated.ats", SceneContent);
Engine->LoadScene("scenes/generated.ats");
```

### 2. Direct C++ Access (Editor Linking Against Engine)

If the editor links against the engine library, it can access the scene tree directly through the `AxEngine` class (not the API function-pointer table). This requires including engine headers and linking against the engine:

```cpp
#include "AxEngine/AxSceneTree.h"
#include "AxEngine/AxNode.h"
#include "AxEngine/AxTypedNodes.h"

// Access the engine instance directly (requires linking against engine)
// Note: This approach is for tight integration only. The AxEngineAPI
// function-pointer table is the stable, recommended interface.
```

The `AxEngineAPI` function-pointer table is the stable public interface. Direct C++ access is available but subject to change between engine versions.

---

## Resource Lifecycle

The engine uses handle-based resource management with deferred destruction (see DEC-007). Resources (textures, models, shaders) are reference-counted. When a resource's reference count reaches zero, it is queued for deferred destruction rather than being destroyed immediately.

### How It Works in Editor Mode

- `Tick()` calls `ProcessPendingReleases()` at the end of each frame automatically
- When `LoadScene()` is called, model resources for all `MeshInstance` nodes are loaded automatically
- When `UnloadScene()` or a scene replacement occurs, model handles are released (queued for deferred destruction)
- The deferred destruction happens on the next `Tick()` after the release

### Editor Considerations

- **No manual `ProcessPendingReleases()` calls needed.** `Tick()` handles this automatically.
- **Scene transitions are safe.** Loading a new scene releases the old scene's resources and loads the new ones in a single operation.
- **Play mode restore reloads resources.** When `ExitPlayMode()` restores the scene from a snapshot, it reloads model resources because the handles were released during the unload step.

---

## Shutdown

When the editor exits, shut down in reverse initialization order:

```cpp
// Shutdown the engine (cleans up scene, renderer, resources, plugins)
Engine->Shutdown();

// Unload the engine DLL (Debug/Development mode only)
PluginAPI->Unload(EngineHandle);

// Close the log file
AxLogCloseFile();

// Terminate Foundation's global API registry
AxonTermGlobalAPIRegistry();
```

### What Shutdown() Does

1. Stops the engine (`isRunning_ = false`)
2. Shuts down AxInput
3. Detaches and destroys game scripts (if any were loaded)
4. Clears the ScriptRegistry
5. Unloads the Game DLL (Debug/Development mode, if loaded)
6. Unloads the scene (releases model handles, destroys scene tree)
7. Discards any scene snapshot buffer
8. Shuts down the renderer
9. Shuts down ResourceAPI (cleans up all loaded resources)
10. Terminates the scene parser
11. Destroys the resource allocator
12. Skips window destruction (editor owns the window in editor-hosted mode)

---

## Constraints and Ordering Requirements

### Initialization Order

The following initialization order must be respected:

```
1. AxonInitGlobalAPIRegistry()        -- creates the API registry
2. AxonRegisterAllFoundationAPIs()    -- registers Platform, Plugin, Allocator, etc.
3. AxLogOpenFile()                    -- optional, enables file logging
4. PluginAPI->Load("libAxEngine.dll") -- loads engine (registers AxEngineAPI)
5. Engine->Initialize(&Config)        -- initializes engine subsystems
6. Engine->LoadScene() or NewScene()  -- populates the scene (editor-hosted only)
```

Violating this order (e.g., calling `Initialize()` before Foundation APIs are registered) will cause null pointer dereferences or assertion failures.

### Per-Frame Ordering

Within the editor's frame loop:

```
1. Process editor UI events
2. Update editor camera if changed:  Engine->SetEditorCamera(...)
3. Handle viewport resize if changed: Engine->Resize(Width, Height)
4. Advance the engine:                Engine->Tick()
5. Composite editor UI / swap buffers
```

**Resize before Tick.** If the viewport dimensions changed since the last frame, call `Resize()` before `Tick()`. This ensures the renderer uses the correct viewport size and aspect ratio for the frame. Failing to resize first may cause a single frame to render at the wrong aspect ratio.

**SetEditorCamera before Tick.** If the editor camera moved (orbit, pan, zoom from mouse input), call `SetEditorCamera()` before `Tick()` so the frame renders from the updated viewpoint.

### Resource Cleanup

`ProcessPendingReleases()` is called automatically inside `Tick()`. The editor does not need to call it manually. However, be aware that resource destruction is deferred -- a resource released in frame N is actually destroyed during `Tick()` of frame N+1 (or later).

### Scene Management Constraints

- **One scene at a time.** Loading a new scene unloads the previous one. There is no multi-scene support.
- **SaveScene() requires a loaded scene.** Returns `false` if no scene is loaded.
- **EnterPlayMode() requires a loaded scene.** The snapshot will be empty if called with no scene, and `ExitPlayMode()` will log an error.
- **Do not call LoadScene/NewScene during Play mode** without first calling `ExitPlayMode()`. The snapshot references the pre-play scene -- loading a different scene during play means `ExitPlayMode()` will restore the wrong scene.

### Thread Safety

The engine is **single-threaded**. All `AxEngineAPI` calls must come from the same thread. Do not call engine functions from background threads, UI threads, or worker pools. If the editor uses a multi-threaded architecture, marshal all engine calls to the thread that owns the frame loop.

---

## Complete Example: Editor Main Loop

This example demonstrates a minimal editor that initializes the engine, loads a scene, provides orbit camera navigation, and supports play/stop toggling.

```cpp
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxPlugin.h"
#include "AxEngine/AxEngine.h"
#include "AxLog/AxLog.h"

int main(int argc, char** argv)
{
    // === Foundation Initialization ===
    AxonInitGlobalAPIRegistry();
    AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);
    AxLogOpenFile("editor.log");

    // === Load Engine ===
    AxPluginAPI* PluginAPI = static_cast<AxPluginAPI*>(
        AxonGlobalAPIRegistry->Get(AXON_PLUGIN_API_NAME)
    );
    uint64_t EngineHandle = PluginAPI->Load("libAxEngine.dll", false);
    AxEngineAPI* Engine = static_cast<AxEngineAPI*>(
        AxonGlobalAPIRegistry->Get(AX_ENGINE_API_NAME)
    );

    // === Create Editor Window and Viewport Panel ===
    // (Your windowing/UI framework code here)
    HWND MainWindow = CreateEditorWindow();
    HWND ViewportPanel = CreateViewportPanel(MainWindow);
    int32_t VpWidth = 1280, VpHeight = 720;

    // === Initialize Engine in Editor-Hosted Mode ===
    AxEngineConfig Config{};
    Config.PluginPath = "./plugins/";
    Config.argc = argc;
    Config.argv = argv;
    Config.ExternalWindowHandle = reinterpret_cast<uint64_t>(ViewportPanel);
    Config.ViewportWidth = VpWidth;
    Config.ViewportHeight = VpHeight;

    Engine->Initialize(&Config);

    // === Load Initial Scene ===
    Engine->LoadScene("scenes/default.ats");

    // === Set Initial Editor Camera ===
    Engine->SetEditorCamera(0.0f, 5.0f, 10.0f,
                            0.0f, 0.0f, 0.0f,
                            60.0f);

    // === Editor State ===
    bool IsPlaying = false;
    float CamYaw = 0.0f, CamPitch = 0.3f, CamDist = 15.0f;

    // === Main Loop ===
    while (!ShouldClose(MainWindow)) {
        // Process editor UI events
        ProcessEditorEvents(MainWindow);

        // Handle viewport resize
        int32_t NewW, NewH;
        if (GetViewportSize(ViewportPanel, &NewW, &NewH)) {
            if (NewW != VpWidth || NewH != VpHeight) {
                VpWidth = NewW;
                VpHeight = NewH;
                Engine->Resize(VpWidth, VpHeight);
            }
        }

        // Handle play/stop toggle (e.g., toolbar button)
        if (PlayButtonPressed()) {
            if (!IsPlaying) {
                Engine->EnterPlayMode();
                IsPlaying = true;
            } else {
                Engine->ExitPlayMode();
                IsPlaying = false;
            }
        }

        // Update editor camera from mouse input (Edit mode only)
        if (!IsPlaying) {
            UpdateOrbitFromMouse(&CamYaw, &CamPitch, &CamDist);
            float CosP = cosf(CamPitch);
            Engine->SetEditorCamera(
                CamDist * CosP * sinf(CamYaw),
                CamDist * sinf(CamPitch),
                CamDist * CosP * cosf(CamYaw),
                0.0f, 0.0f, 0.0f,
                60.0f
            );
        }

        // Advance the engine (renders into ViewportPanel)
        Engine->Tick();

        // Render editor UI (panels, menus, etc.) and swap buffers
        RenderEditorUI(MainWindow);
        SwapBuffers(MainWindow);
    }

    // === Shutdown ===
    Engine->Shutdown();
    PluginAPI->Unload(EngineHandle);
    AxLogCloseFile();
    AxonTermGlobalAPIRegistry();

    return 0;
}
```

---

## API Reference Summary

### AxEngineConfig

| Field                  | Type        | Default | Description |
|------------------------|-------------|---------|-------------|
| `PluginPath`           | `std::string` | `""`  | Directory containing plugin DLLs |
| `ConfigPath`           | `std::string` | `""`  | Path to engine config file (optional) |
| `argc`                 | `int`       | 0       | Command-line argument count |
| `argv`                 | `char**`    | nullptr | Command-line arguments |
| `ExternalWindowHandle` | `uint64_t`  | 0       | When non-zero, renders into this HWND (editor mode) |
| `ViewportWidth`        | `int32_t`   | 1280    | Initial viewport width (editor mode only) |
| `ViewportHeight`       | `int32_t`   | 720     | Initial viewport height (editor mode only) |

### AxEngineMode

| Value  | Int | Description |
|--------|-----|-------------|
| `Edit` | 0   | Transforms propagate, scripts/physics skipped |
| `Play` | 1   | Full simulation (identical to standalone) |

### AxEngineAPI Function Pointers

| Function           | Signature | Description |
|--------------------|-----------|-------------|
| `Initialize`       | `bool (*)(const AxEngineConfig*)` | Initialize engine with configuration |
| `Run`              | `int (*)()` | Run standalone frame loop (blocks until exit; not used by editor) |
| `Tick`             | `bool (*)()` | Advance one frame (editor drives this) |
| `Shutdown`         | `void (*)()` | Clean up engine |
| `IsRunning`        | `bool (*)()` | Query if engine is running |
| `Resize`           | `void (*)(int32_t, int32_t)` | Update viewport dimensions |
| `SetMode`          | `void (*)(AxEngineMode)` | Switch Edit/Play mode (no snapshot) |
| `LoadScene`        | `bool (*)(const char*)` | Load .ats scene file |
| `UnloadScene`      | `void (*)()` | Clear current scene |
| `NewScene`         | `void (*)()` | Create empty scene with root node |
| `SaveScene`        | `bool (*)(const char*)` | Save scene to .ats file |
| `EnterPlayMode`    | `void (*)()` | Snapshot scene, switch to Play mode |
| `ExitPlayMode`     | `void (*)()` | Restore scene, switch to Edit mode |
| `SetEditorCamera`  | `void (*)(float, float, float, float, float, float, float)` | Set editor camera (pos, target, fov) |

### EditorCameraState

| Field      | Type    | Default          |
|------------|---------|------------------|
| `Position` | `AxVec3`| (0, 5, 10)      |
| `Target`   | `AxVec3`| (0, 0, 0)       |
| `FOV`      | `float` | 60.0             |
| `Near`     | `float` | 0.1              |
| `Far`      | `float` | 1000.0           |
