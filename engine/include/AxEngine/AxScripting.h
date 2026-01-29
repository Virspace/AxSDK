#pragma once

#include "Foundation/AxTypes.h"

#ifndef SCRIPTING_API
#  ifdef SCRIPTINGMODULE
 /* We are building this library */
#    define SCRIPTING_API __declspec(dllexport)
#  else
 /* We are using this library */
#    define SCRIPTING_API __declspec(dllimport)
#  endif
#endif

struct AxAPIRegistry;
struct AxPlatformAPI;
struct AxDLLAPI;

typedef void InitF();
typedef void StartF();
typedef void FixedTickF(const float FixedDeltaT);
typedef void TickF(const float Alpha, const float DeltaT);
typedef void StopF();
typedef void TermF();

class SCRIPTING_API AxScripting
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

private:
    AxAPIRegistry *APIRegistry_;
    AxPlatformAPI *PlatformAPI_;
    AxPlatformDLLAPI *DLLAPI_;
};