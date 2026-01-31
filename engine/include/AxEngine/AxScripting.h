#pragma once

#include "Foundation/AxTypes.h"
#include "Foundation/AxPlatform.h"
#include "AxResource/AxResourceTypes.h"

struct AxAPIRegistry;
struct AxCamera;
struct AxInputState;
struct AxModelData;

typedef void InitF();
typedef void StartF();
typedef void FixedTickF(const float FixedDeltaT);
typedef void TickF(const float Alpha, const float DeltaT);
typedef void StopF();
typedef void TermF();

class AxScripting
{
public:
	// IModule Interface
	bool Create(AxAPIRegistry *APIRegistry);
	void Destroy();

    bool LoadScript(const char *scriptPath);
    bool LoadScripts(const char *scriptList);
    bool UnloadScripts();

    void Init();
    void Start();
    void FixedTick(const double FixedDeltaT);
    void Tick(const double Alpha, const double DeltaT);
    void LateTick();
    void Stop();
    void Term();

    void OnGUI(int64_t *ctx);

    // Set engine state on all scripts (camera, scene)
    void SetEngineState(AxCamera* Camera, const AxModelData* Scene);

    // Update per-frame state on all scripts (mouse delta)
    void UpdateFrameState(AxVec2 MouseDelta);

private:
    AxAPIRegistry *APIRegistry_;
    AxPlatformAPI *PlatformAPI_;
    AxPlatformDLLAPI *DLLAPI_;
};