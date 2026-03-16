# AxEngine Editor Integration Guide

> Version: 2.0.0
> Last Updated: 2026-03-16

This document describes how to integrate AxEngine into an external editor application (AxEditor). The engine is a **static C++ library** (DEC-016) that the editor links directly at compile time. The editor owns the window, the frame loop, and the overall application lifecycle. The engine renders into an editor-provided HWND and advances one frame per `Tick()` call.

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Bootstrapping the Engine](#bootstrapping-the-engine)
3. [Frame Loop](#frame-loop)
4. [Viewport Resize](#viewport-resize)
5. [Editor Camera](#editor-camera)
6. [Edit and Play Modes](#edit-and-play-modes)
7. [Scene Management](#scene-management)
8. [Programmatic Scene Manipulation](#programmatic-scene-manipulation)
9. [Debug Draw](#debug-draw)
10. [Resource Lifecycle](#resource-lifecycle)
11. [Shutdown](#shutdown)
12. [Constraints and Ordering Requirements](#constraints-and-ordering-requirements)
13. [Complete Example: Editor Main Loop](#complete-example-editor-main-loop)

---

## Architecture Overview

```
+------------------+          +---------------------+
|    AxEditor      |          |     AxEngine        |
|  (owns window)   |  links   |  (static library)   |
|  (owns frame     | -------> |  Direct C++ API:    |
|   loop)          |          |    Initialize()     |
|  (owns UI)       |          |    Tick()           |
|                  |          |    Shutdown()       |
|  Creates HWND    |          |    Resize()         |
|  Constructs      |          |    SetEditorCamera()|
|  AxEngine        |          |    EnterPlayMode()  |
|  directly        |          |    ExitPlayMode()   |
+------------------+          |    LoadScene()      |
                              |    SaveScene()      |
                              |    NewScene()       |
                              |    UnloadScene()    |
                              |    SetMode()        |
                              |    IsRunning()      |
                              +---------------------+
```

Key architectural points:

- **AxEngine is a static library** (DEC-016). The editor links it at compile time via `Virspace::AxEngine`. No DLL loading, no function-pointer indirection, no `AxEngineAPI` struct. All calls are direct C++ method calls on the `AxEngine` class.
- **AxWindow plugin is NOT loaded** in editor-hosted mode. The editor owns the window.
- **AxInput receives nullptr** and no-ops on `Update()`. The editor handles input separately and translates it into engine API calls (camera movement, play/pause, etc.).
- **Scene loading is NOT automatic.** The editor calls `LoadScene()`, `NewScene()`, or `SaveScene()` explicitly.
- **Game script DLL is NOT auto-loaded.** The editor manages script compilation and hot-reload.
- **ESC-to-quit is disabled** in editor-hosted mode.
- The engine defaults to **Edit mode** when editor-hosted (standalone defaults to Play mode).
- **Game.dll links the editor executable's import library** (not the static lib) to resolve engine symbols. The editor exe needs `ENABLE_EXPORTS TRUE` and `-Wl,--export-all-symbols` so Game.dll can find `ScriptRegistry::Get()` and other engine symbols.

---

## Bootstrapping the Engine

The editor links AxEngine as a static library. There is no DLL to load and no `AxEngineAPI` struct to retrieve. The editor constructs an `AxEngine` instance directly and calls methods on it.

### Required Headers

```cpp
#include "Foundation/AxAPIRegistry.h"
#include "AxEngine/AxEngine.h"
#include "AxLog/AxLog.h"
```

### CMake Setup

The editor's CMakeLists.txt links the engine static library:

```cmake
add_executable(AxEditor)

target_compile_definitions(AxEditor PRIVATE AXON_LINKS_FOUNDATION)
target_link_libraries(AxEditor PRIVATE Virspace::AxEngine)

# Export all symbols so Game.dll can resolve engine symbols from the editor exe
target_link_options(AxEditor PRIVATE -Wl,--export-all-symbols)
set_target_properties(AxEditor PROPERTIES ENABLE_EXPORTS TRUE)
```

### Initialization Sequence

Foundation must be initialized before the engine. The engine depends on Foundation APIs (Platform, Plugin, Allocator, HashTable) being registered in the global API registry.

```cpp
// Step 1: Initialize Foundation's global API registry
AxonInitGlobalAPIRegistry();
AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);

// Step 2: Open the log file (optional but recommended)
AxLogOpenFile("editor.log");

// Step 3: Create the editor's viewport panel (your windowing/UI framework)
HWND ViewportPanel = CreateViewportPanel();
int32_t VpWidth = 1280, VpHeight = 720;

// Step 4: Configure the engine for editor-hosted mode
AxEngineConfig Config{};
Config.PluginPath = "./plugins/";
Config.argc = argc;
Config.argv = argv;
Config.ExternalWindowHandle = reinterpret_cast<uint64_t>(ViewportPanel);
Config.ViewportWidth = VpWidth;
Config.ViewportHeight = VpHeight;

// Step 5: Construct and initialize the engine directly
AxEngine Engine;
if (!Engine.Initialize(&Config, AxonGlobalAPIRegistry)) {
    Engine.Shutdown();
    AxLogCloseFile();
    AxonTermGlobalAPIRegistry();
    return 1;
}

// Step 6: Load initial scene (editor-hosted mode does NOT auto-load)
Engine.LoadScene("scenes/default.ats");
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
    Engine.Tick();

    // Swap the editor window's back buffer
    SwapEditorBuffers();
}
```

### What Tick() Does Each Frame

1. Calculates delta time from the last call
2. Polls window events (skipped in editor-hosted mode — no AxWindow)
3. Updates AxInput (no-op in editor-hosted mode)
4. Propagates mouse delta to SceneTree (for script access)
5. **Play mode only:** Runs fixed-timestep updates (physics placeholder, `ScriptBase::OnFixedUpdate`)
6. Runs variable-rate update (transform propagation always runs; `ScriptBase::OnUpdate` runs in Play mode only)
7. **Play mode only:** Runs late update (`ScriptBase::OnLateUpdate`)
8. Renders the scene (using editor camera in Edit mode, game camera in Play mode)
9. Flushes debug draw primitives (see [Debug Draw](#debug-draw))
10. Processes pending resource releases (deferred destruction)

### Frame Timing

The engine tracks wall-clock time internally. The first `Tick()` after `Initialize()` produces a valid delta time (typically near zero). There is no need to seed or reset the timer — `Initialize()` records the starting timestamp.

---

## Viewport Resize

When the editor's viewport panel changes size, call `Resize()` before the next `Tick()`.

```cpp
void OnViewportResized(int32_t NewWidth, int32_t NewHeight)
{
    Engine.Resize(NewWidth, NewHeight);
}
```

`Resize()` updates the renderer's viewport dimensions and recalculates the camera aspect ratio. If the editor camera is active (Edit mode), its projection matrix is also updated.

**Ordering:** Call `Resize()` before `Tick()` when dimensions change. This ensures the frame renders at the correct resolution.

---

## Editor Camera

The editor camera is independent of any scene `CameraNode`. It provides view and projection matrices during Edit mode so the editor viewport can be navigated (orbit, pan, zoom) without modifying the scene.

### Setting the Editor Camera

```cpp
Engine.SetEditorCamera(
    0.0f, 5.0f, 10.0f,    // Camera position (x, y, z)
    0.0f, 0.0f, 0.0f,     // Look-at target (x, y, z)
    60.0f                   // Field of view in degrees
);
```

### Default Editor Camera

| Parameter | Default Value |
|-----------|--------------|
| Position  | (0, 5, 10)   |
| Target    | (0, 0, 0)    |
| FOV       | 60 degrees   |
| Near Clip | 0.1          |
| Far Clip  | 1000.0       |

### Editor Camera and Mode Switching

- **Edit mode:** The editor camera is active. Scene `CameraNode` instances are ignored for rendering.
- **Play mode:** The scene's first `CameraNode` is used for rendering. The editor camera state is preserved and restored when returning to Edit mode.

### Orbit Camera Example

```cpp
void UpdateEditorOrbitCamera(AxEngine& Engine,
                              float Yaw, float Pitch, float Distance,
                              float TargetX, float TargetY, float TargetZ)
{
    float CosP = cosf(Pitch);
    float PosX = TargetX + Distance * CosP * sinf(Yaw);
    float PosY = TargetY + Distance * sinf(Pitch);
    float PosZ = TargetZ + Distance * CosP * cosf(Yaw);

    Engine.SetEditorCamera(PosX, PosY, PosZ,
                            TargetX, TargetY, TargetZ,
                            60.0f);
}
```

---

## Edit and Play Modes

The engine operates in one of two modes:

```cpp
enum class AxEngineMode : int32_t {
    Edit = 0,   // Transforms propagate, scripts/physics skipped
    Play = 1    // Full simulation (identical to standalone)
};
```

### Edit Mode (Default for Editor)

- Transform propagation runs every `Tick()` — the viewport always reflects current node positions
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

```cpp
Engine.SetMode(AxEngineMode::Play);  // Enable scripts
Engine.SetMode(AxEngineMode::Edit);  // Disable scripts
```

**Warning:** `SetMode()` does NOT snapshot or restore the scene. For proper play-mode behavior with scene preservation, use `EnterPlayMode()` and `ExitPlayMode()` instead.

### Play Mode with Snapshot/Restore

```cpp
// User presses "Play" button in editor toolbar
Engine.EnterPlayMode();
// Scene is now snapshotted. Scripts run. Game camera is active.

// ... user tests their game ...

// User presses "Stop" button in editor toolbar
Engine.ExitPlayMode();
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

- Scripts start fresh on each `EnterPlayMode()`. Script runtime state is **not** preserved across play/stop cycles. `OnInit` runs again on the next `EnterPlayMode()`.
- If `EnterPlayMode()` is called while already in Play mode, it logs a warning and returns without action.
- If `ExitPlayMode()` is called while already in Edit mode, it logs a warning and returns without action.

---

## Scene Management

The engine provides scene management methods on the `AxEngine` class. In editor-hosted mode, no scene is loaded automatically during `Initialize()` — the editor must call one of these to populate the viewport.

### Loading a Scene from Disk

```cpp
bool Success = Engine.LoadScene("scenes/my_level.ats");
```

`LoadScene()` unloads the current scene, parses the `.ats` file, creates a new scene tree, loads model resources, and sets up the main camera.

### Creating a New Empty Scene

```cpp
Engine.NewScene();
// Scene now contains only a root node named "NewScene"
```

### Saving the Current Scene

```cpp
bool Success = Engine.SaveScene("scenes/my_level.ats");
```

### Unloading the Current Scene

```cpp
Engine.UnloadScene();
// Engine continues running with an empty viewport
```

---

## Programmatic Scene Manipulation

Since the editor links the engine as a static C++ library (DEC-016), it has **full direct access** to all engine types. This is the primary way the editor interacts with the scene — no function-pointer indirection, no marshaling, no C wrapper functions.

### Direct C++ Access

```cpp
#include "AxEngine/AxSceneTree.h"
#include "AxEngine/AxNode.h"
#include "AxEngine/AxTypedNodes.h"

// Access the scene tree from the engine instance
SceneTree* Tree = Engine.GetSceneTree();

// Create typed nodes
MeshInstance* Mesh = Tree->CreateNode<MeshInstance>("Ground");
CameraNode* Cam = Tree->CreateNode<CameraNode>("MainCamera");
LightNode* Sun = Tree->CreateNode<LightNode>("Sun");

// Set properties directly on typed nodes
Cam->FOV = 60.0f;
Cam->Near = 0.1f;
Cam->Far = 1000.0f;

Sun->Type = LightType::Directional;
Sun->Color = {1.0f, 0.95f, 0.9f};
Sun->Intensity = 1.0f;

// Set transforms
Mesh->SetLocalPosition({0.0f, 0.0f, 0.0f});
Mesh->SetLocalScale({10.0f, 1.0f, 10.0f});

// Build hierarchy
Tree->GetRoot()->AddChild(Mesh);
Tree->GetRoot()->AddChild(Cam);
Tree->GetRoot()->AddChild(Sun);

// Query nodes
Node* Found = Tree->FindNode("MainCamera");
std::vector<MeshInstance*> Meshes = Tree->GetNodesByType<MeshInstance>();
```

### File-Based Manipulation

Create or modify `.ats` files and load them:

```cpp
Engine.LoadScene("scenes/generated.ats");
```

The `.ats` format is text-based and human-readable — scene files can be generated programmatically if needed.

---

## Debug Draw

The engine provides an immediate-mode debug draw API for visualizing lines, boxes, spheres, and rays in 3D world space. This is used by the editor for gizmos, selection outlines, and grid rendering, and by game scripts for debug visualization.

### API

```cpp
// Access from the engine instance
DebugDraw* Debug = Engine.GetDebugDraw();

// Draw primitives (all positions in world space)
Debug->Line(From, To, Color);                       // Line segment
Debug->Ray(Origin, Direction, Length, Color);        // Ray from origin
Debug->Box(Center, HalfExtents, Color);             // Wireframe box
Debug->Sphere(Center, Radius, Color, Segments);     // Wireframe sphere (default 16 segments)
```

Colors are `AxVec4` (RGBA). Alpha < 1.0 produces semi-transparent lines.

### Behavior

- **Immediate-mode:** All primitives are cleared after each frame. Submit every frame to keep them visible.
- **Batched:** All primitives are collected into a single vertex buffer and drawn with one `GL_LINES` call per frame — minimal GPU overhead.
- **Depth-tested:** Debug lines are hidden behind closer scene geometry by default (depth-test ON, depth-write OFF).
- **Rendered after scene:** Debug geometry renders on top of the scene, before SwapBuffers.

### Editor Usage

The editor uses debug draw for visualization overlays:

```cpp
// Grid
for (int i = -10; i <= 10; i++) {
    AxVec4 GridColor = {0.5f, 0.5f, 0.5f, 0.3f};
    Debug->Line({(float)i, 0.0f, -10.0f}, {(float)i, 0.0f, 10.0f}, GridColor);
    Debug->Line({-10.0f, 0.0f, (float)i}, {10.0f, 0.0f, (float)i}, GridColor);
}

// Selection wireframe around a node
AxVec3 Pos = SelectedNode->GetWorldPosition();
AxVec3 Half = {0.5f, 0.5f, 0.5f};
Debug->Box(Pos, Half, {1.0f, 0.8f, 0.0f, 1.0f});  // Yellow selection box
```

### Script Usage

Scripts access debug draw through `ScriptBase::GetDebugDraw()`:

```cpp
void MyScript::OnUpdate(float DeltaT)
{
    // Visualize patrol path
    DebugDraw* Debug = GetDebugDraw();
    if (Debug) {
        Debug->Line(GetNode()->GetWorldPosition(), TargetPos, {0.0f, 1.0f, 0.0f, 1.0f});
        Debug->Sphere(TargetPos, 0.5f, {1.0f, 0.0f, 0.0f, 1.0f});
    }
}
```

### Shipping Builds

Debug draw is compiled out in shipping builds (`AX_SHIPPING`). All methods become no-ops and the flush is skipped entirely — zero runtime cost.

---

## Resource Lifecycle

The engine uses handle-based resource management with deferred destruction (DEC-007). Resources (textures, models, shaders) are reference-counted. When a resource's reference count reaches zero, it is queued for deferred destruction.

### Editor Considerations

- **No manual `ProcessPendingReleases()` calls needed.** `Tick()` handles this automatically.
- **Scene transitions are safe.** Loading a new scene releases the old scene's resources and loads the new ones in a single operation.
- **Play mode restore reloads resources.** When `ExitPlayMode()` restores the scene from a snapshot, it reloads model resources.

---

## Shutdown

When the editor exits, shut down in reverse initialization order:

```cpp
Engine.Shutdown();
AxLogCloseFile();
AxonTermGlobalAPIRegistry();
```

### What Shutdown() Does

1. Stops the engine
2. Shuts down AxInput
3. Detaches and destroys game scripts (if any were loaded)
4. Clears the ScriptRegistry
5. Unloads the Game DLL (if loaded)
6. Unloads the scene (releases model handles, destroys scene tree)
7. Discards any scene snapshot buffer
8. Shuts down the renderer
9. Shuts down ResourceAPI
10. Terminates the scene parser
11. Destroys the resource allocator
12. Skips window destruction (editor owns the window)

---

## Constraints and Ordering Requirements

### Initialization Order

```
1. AxonInitGlobalAPIRegistry()        -- creates the API registry
2. AxonRegisterAllFoundationAPIs()    -- registers Platform, Plugin, Allocator, etc.
3. AxLogOpenFile()                    -- optional, enables file logging
4. AxEngine Engine;                   -- construct engine instance
5. Engine.Initialize(&Config, AxonGlobalAPIRegistry)  -- initialize subsystems
6. Engine.LoadScene() or NewScene()   -- populate the scene (editor-hosted only)
```

### Per-Frame Ordering

```
1. Process editor UI events
2. Update editor camera if changed:  Engine.SetEditorCamera(...)
3. Handle viewport resize if changed: Engine.Resize(Width, Height)
4. Advance the engine:                Engine.Tick()
5. Composite editor UI / swap buffers
```

**Resize before Tick.** Call `Resize()` before `Tick()` when viewport dimensions change.

**SetEditorCamera before Tick.** Call before `Tick()` so the frame renders from the updated viewpoint.

### Scene Management Constraints

- **One scene at a time.** Loading a new scene unloads the previous one.
- **SaveScene() requires a loaded scene.** Returns `false` if no scene is loaded.
- **EnterPlayMode() requires a loaded scene.**
- **Do not call LoadScene/NewScene during Play mode** without first calling `ExitPlayMode()`.

### Thread Safety

The engine is **single-threaded**. All calls must come from the same thread.

### Game.dll Integration

When the editor loads Game.dll for script hot-reload, the DLL resolves engine symbols from the editor executable's export table — the same pattern used by `OpenGLGameHost`. The editor's CMakeLists must include:

```cmake
target_link_options(AxEditor PRIVATE -Wl,--export-all-symbols)
set_target_properties(AxEditor PROPERTIES ENABLE_EXPORTS TRUE)
```

Game.dll links against the editor executable's import library (not `Virspace::AxEngine` directly) to ensure a single `ScriptRegistry::Get()` instance is shared between the editor and scripts.

---

## Complete Example: Editor Main Loop

```cpp
#include "Foundation/AxAPIRegistry.h"
#include "AxEngine/AxEngine.h"
#include "AxLog/AxLog.h"

int main(int argc, char** argv)
{
    // === Foundation Initialization ===
    AxonInitGlobalAPIRegistry();
    AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);
    AxLogOpenFile("editor.log");

    // === Create Editor Window and Viewport Panel ===
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

    AxEngine Engine;
    if (!Engine.Initialize(&Config, AxonGlobalAPIRegistry)) {
        Engine.Shutdown();
        AxLogCloseFile();
        AxonTermGlobalAPIRegistry();
        return 1;
    }

    // === Load Initial Scene ===
    Engine.LoadScene("scenes/default.ats");

    // === Set Initial Editor Camera ===
    Engine.SetEditorCamera(0.0f, 5.0f, 10.0f,
                            0.0f, 0.0f, 0.0f,
                            60.0f);

    // === Editor State ===
    bool IsPlaying = false;
    float CamYaw = 0.0f, CamPitch = 0.3f, CamDist = 15.0f;

    // === Main Loop ===
    while (!ShouldClose(MainWindow)) {
        ProcessEditorEvents(MainWindow);

        // Handle viewport resize
        int32_t NewW, NewH;
        if (GetViewportSize(ViewportPanel, &NewW, &NewH)) {
            if (NewW != VpWidth || NewH != VpHeight) {
                VpWidth = NewW;
                VpHeight = NewH;
                Engine.Resize(VpWidth, VpHeight);
            }
        }

        // Handle play/stop toggle
        if (PlayButtonPressed()) {
            if (!IsPlaying) {
                Engine.EnterPlayMode();
                IsPlaying = true;
            } else {
                Engine.ExitPlayMode();
                IsPlaying = false;
            }
        }

        // Update editor camera from mouse input (Edit mode only)
        if (!IsPlaying) {
            UpdateOrbitFromMouse(&CamYaw, &CamPitch, &CamDist);
            float CosP = cosf(CamPitch);
            Engine.SetEditorCamera(
                CamDist * CosP * sinf(CamYaw),
                CamDist * sinf(CamPitch),
                CamDist * CosP * cosf(CamYaw),
                0.0f, 0.0f, 0.0f,
                60.0f
            );
        }

        // Draw editor overlays (grid, selection, etc.)
        DebugDraw* Debug = Engine.GetDebugDraw();
        if (Debug && !IsPlaying) {
            for (int i = -10; i <= 10; i++) {
                AxVec4 GridColor = {0.5f, 0.5f, 0.5f, 0.3f};
                Debug->Line({(float)i, 0.f, -10.f}, {(float)i, 0.f, 10.f}, GridColor);
                Debug->Line({-10.f, 0.f, (float)i}, {10.f, 0.f, (float)i}, GridColor);
            }
        }

        // Advance the engine (renders scene + debug draw into ViewportPanel)
        Engine.Tick();

        // Render editor UI (panels, menus, etc.) and swap buffers
        RenderEditorUI(MainWindow);
        SwapBuffers(MainWindow);
    }

    // === Shutdown ===
    Engine.Shutdown();
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

### AxEngine Public Methods

| Method             | Signature | Description |
|--------------------|-----------|-------------|
| `Initialize`       | `bool Initialize(const AxEngineConfig*, AxAPIRegistry*)` | Initialize engine with configuration |
| `Run`              | `int Run()` | Run standalone frame loop (blocks; not used by editor) |
| `Tick`             | `bool Tick()` | Advance one frame (editor drives this) |
| `Shutdown`         | `void Shutdown()` | Clean up engine |
| `IsRunning`        | `bool IsRunning() const` | Query if engine is running |
| `Resize`           | `void Resize(int32_t, int32_t)` | Update viewport dimensions |
| `SetMode`          | `void SetMode(AxEngineMode)` | Switch Edit/Play mode (no snapshot) |
| `LoadScene`        | `bool LoadScene(const char*)` | Load .ats scene file |
| `UnloadScene`      | `void UnloadScene()` | Clear current scene |
| `NewScene`         | `void NewScene()` | Create empty scene with root node |
| `SaveScene`        | `bool SaveScene(const char*)` | Save scene to .ats file |
| `EnterPlayMode`    | `void EnterPlayMode()` | Snapshot scene, switch to Play mode |
| `ExitPlayMode`     | `void ExitPlayMode()` | Restore scene, switch to Edit mode |
| `SetEditorCamera`  | `void SetEditorCamera(float, float, float, float, float, float, float)` | Set editor camera (pos, target, fov) |
| `GetSceneTree`     | `SceneTree* GetSceneTree()` | Direct access to the scene tree |
| `GetDebugDraw`     | `DebugDraw* GetDebugDraw()` | Debug draw API (nullptr in shipping) |

### EditorCameraState

| Field      | Type    | Default          |
|------------|---------|------------------|
| `Position` | `AxVec3`| (0, 5, 10)      |
| `Target`   | `AxVec3`| (0, 0, 0)       |
| `FOV`      | `float` | 60.0             |
| `Near`     | `float` | 0.1              |
| `Far`      | `float` | 1000.0           |
