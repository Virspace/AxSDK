#pragma once

#include "Foundation/AxTypes.h"

// Forward declarations
struct AxWindow;
struct AxWindowAPI;
struct AxAPIRegistry;
struct AxOpenGLAPI;
struct AxPlatformAPI;
struct AxPlatformFileAPI;
struct AxSceneAPI;
struct AxEngineAPI;
struct AxEngineHandle;
struct AxCamera;
struct AxScene;
struct AxViewport;

class Game
{
public:
    // Lifecycle
    void Initialize(AxAPIRegistry *APIRegistry, AxWindow *Window, const AxViewport *Viewport);
    bool Create();
    bool Tick(float DeltaT);
    void Destroy();

    // Input and state
    void UpdateCamera(float DeltaTime);
    void IsRequestingExit(bool Value);
    bool IsRequestingExit() { return IsRequestingExit_; }
    inline AxTransform *GetTransform() { return Transform_; }
    inline AxWindowAPI *GetWindowAPI() { return WindowAPI_; }

private:
    // Scene loading
    bool CreateScene();
    void SetupSceneCamera(const AxScene* Scene);

    // Input callbacks
    void RegisterCallbacks();

    // API handles
    AxAPIRegistry *APIRegistry_{nullptr};
    AxEngineAPI *EngineAPI_{nullptr};
    AxOpenGLAPI *RenderAPI_{nullptr};
    AxSceneAPI *SceneAPI_{nullptr};
    AxWindowAPI *WindowAPI_{nullptr};

    // Infrastructure
    AxEngineHandle *Engine_{nullptr};
    AxWindow *Window_{nullptr};
    const AxViewport *Viewport_{nullptr};

    // State
    AxScene *Scene_{nullptr};
    AxCamera *Camera_{nullptr};
    AxTransform *Transform_{nullptr};
    bool IsRequestingExit_{false};
};