#pragma once

#include "Foundation/AxTypes.h"

struct AxWindow;
struct AxWindowAPI;
struct AxPluginAPI;
struct AxAPIRegistry;

struct Timing
{
    uint64_t FrameCount;
    AxWallClock WallClock;
    float ElapsedWallTime;
    float DeltaT;
};

// NOTE(mdeforge): Application specific data goes here, could become a dumping ground
struct AxApplication
{
    // These get assigned to in Create()
    AxAPIRegistry *APIRegistry;
    AxWindowAPI *WindowAPI;
    AxPluginAPI *PluginAPI;

    AxWindow *Window;
    Timing Timing;
    const char *ProjectDirectory;

    bool IsRequestingExit{false};
};