#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "Foundation/AxTypes.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxApplication.h"
#include "Foundation/AxPlatform.h"
#include "AxWindow/AxWindow.h"
#include "AxOpenGL/AxOpenGL.h"

#include "Game.h"

// Engine serves as minimal coordinator between Host infrastructure and Game logic
struct AxAPIRegistry *RegistryAPI;

// Minimal engine state - just what's needed to coordinate
static Game *TheGame;
static AxPlatformAPI *PlatformAPI;
static AxWindowAPI *WindowAPI;
static AxOpenGLAPI *RenderAPI;
static AxWindow *Window;
static AxViewport *Viewport;
static AxWallClock LastFrameTime;

static AxApplication *Create(int argc, char **argv)
{
    // Get APIs from the registry
    PlatformAPI = (struct AxPlatformAPI *)RegistryAPI->Get(AXON_PLATFORM_API_NAME);
    WindowAPI = (struct AxWindowAPI *)RegistryAPI->Get(AXON_WINDOW_API_NAME);
    RenderAPI = (struct AxOpenGLAPI *)RegistryAPI->Get(AXON_OPENGL_API_NAME);

    if (!PlatformAPI || !WindowAPI || !RenderAPI) {
        fprintf(stderr, "Engine: Failed to get required APIs from registry\n");
        return nullptr;
    }

    // Get Host-created resources from registry
    AxWindow **WindowPtr = (AxWindow**)RegistryAPI->Get("HostWindow");
    AxViewport *ViewportPtr = (AxViewport*)RegistryAPI->Get("HostViewport");

    if (!WindowPtr || !*WindowPtr || !ViewportPtr) {
        fprintf(stderr, "Engine: Failed to get Host resources from registry\n");
        return nullptr;
    }

    Window = *WindowPtr;
    Viewport = ViewportPtr;

    printf("Engine: Host resources acquired, ready to create game\n");

    // Initialize timing
    LastFrameTime = PlatformAPI->TimeAPI->WallTime();

    // Create and initialize the game with pre-configured infrastructure
    TheGame = new Game();

    // Initialize the game with Host-provided resources (pass Host's original viewport pointer)
    TheGame->Initialize(RegistryAPI, Window, Viewport);

    if (!TheGame->Create()) {
        delete TheGame;
        return nullptr;
    }

    printf("Engine: Game created and initialized successfully\n");
    return (AxApplication*)TheGame;
}

static bool Tick(AxApplication *App)
{
    if (!TheGame || !Window) {
        return true; // Exit if not properly initialized
    }

    // Calculate frame time
    AxWallClock currentTime = PlatformAPI->TimeAPI->WallTime();
    float deltaTime = PlatformAPI->TimeAPI->ElapsedWallTime(LastFrameTime, currentTime);
    LastFrameTime = currentTime;

    // Poll window events
    WindowAPI->PollEvents(Window);

    // Check for exit conditions
    if (WindowAPI->HasRequestedClose(Window) || TheGame->IsRequestingExit()) {
        return true;
    }

    // Begin frame BEFORE any draw calls
    RenderAPI->NewFrame();
    RenderAPI->SetActiveViewport(Viewport);

    // Let the game do its logic and rendering
    TheGame->Tick(deltaTime);

    // Present AFTER rendering
    RenderAPI->SwapBuffers();

    return false; // Continue running
}

static bool Destroy(AxApplication *App)
{
    // Clean up game
    if (TheGame) {
        TheGame->Destroy();
        delete TheGame;
        TheGame = nullptr;
    }

    printf("Engine: Game destroyed\n");
    // Note: Host will handle renderer and window cleanup
    return true;
}

static struct AxApplicationAPI EngineAPI_ {
    .Create = Create,
    .Tick = Tick,
    .Destroy = Destroy,
};

static struct AxApplicationAPI *EngineAPI = &EngineAPI_;

extern "C" AXON_DLL_EXPORT void LoadPlugin(AxAPIRegistry *APIRegistry, bool Load)
{
    if (APIRegistry) {
        // Store the registry for Engine to use later
        RegistryAPI = APIRegistry;

        // Register the Engine's application API
        APIRegistry->Set(AXON_APPLICATION_API_NAME, EngineAPI, sizeof(struct AxApplicationAPI));

        printf("Engine: Plugin loaded and API registered\n");
    }
}