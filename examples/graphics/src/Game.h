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
struct AxCamera;
struct AxScene;
struct AxViewport;

class Game
{
public:
    void Initialize(AxAPIRegistry *APIRegistry, AxWindow *Window, const AxViewport *Viewport);
    bool Create();
    bool Tick(float DeltaT);
    void Destroy();

    void UpdateCamera(float DeltaTime);
    void IsRequestingExit(bool Value);
    bool IsRequestingExit() { return IsRequestingExit_; }

private:
    bool CreateScene();
    void SetupSceneCamera(const AxScene* Scene);

    void RegisterCallbacks();

    AxCamera *Camera_{nullptr};
    AxTransform *Transform_{nullptr};
    bool IsRequestingExit_{false};
};