#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <stdio.h>
#include <string.h>

#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxPlugin.h"
#include "Foundation/AxApplication.h"
#include "Foundation/AxPlatform.h"
#include "AxWindow/AxWindow.h"
#include "AxOpenGL/AxOpenGL.h"

#ifdef AX_OS_WINDOWS
#include <windows.h>
#include <conio.h>

bool ConsoleAttached = false;

static bool AttachOutputToConsole(void)
{
    if (AttachConsole(ATTACH_PARENT_PROCESS))
    {
        HANDLE ConsoleHandleOut = GetStdHandle(STD_OUTPUT_HANDLE);
        // Redirect unbuffered STDOUT to the console
        if (ConsoleHandleOut != INVALID_HANDLE_VALUE) {
            freopen("CONOUT$", "w", stdout);
            setvbuf(stdout, NULL, _IONBF, 0);
        } else {
            return (false);
        }

        HANDLE ConsoleHandleError = GetStdHandle(STD_ERROR_HANDLE);
        // Redirect unbuffered STDOUT to the console
        if (ConsoleHandleOut != INVALID_HANDLE_VALUE) {
            freopen("CONOUT$", "w", stderr);
            setvbuf(stderr, NULL, _IONBF, 0);
        } else {
            return (false);
        }

        return (true);
    }

    return (false);
}

static void SendEnterKey(void)
{
    INPUT ip = {
        .type = INPUT_KEYBOARD,
        .ki = {
            .wScan = 0, // Hardware scan code for key
            .time = 0,
            .dwExtraInfo = 0,
        }
    };

    // Send the "Enter" key
    ip.ki.wVk = 0x0D; // Virtual keycode for the "Enter" key
    ip.ki.dwFlags = 0;
    SendInput(1, &ip, sizeof(INPUT));

    // Release the "Enter" key
    ip.ki.dwFlags = KEYEVENTF_KEYUP; // Event for key release
    SendInput(1, &ip, sizeof(INPUT));
}

static void FreeCon(void)
{
    if (ConsoleAttached && GetConsoleWindow() == GetForegroundWindow()) {
        SendEnterKey();
    }
}

#endif

// Host bootstrapping structure
typedef struct HostBootstrap
{
    struct AxAPIRegistry *RegistryAPI;
    AxWindowAPI *WindowAPI;
    AxPluginAPI *PluginAPI;
    AxPlatformAPI *PlatformAPI;
    AxOpenGLAPI *RenderAPI;

    AxWindow *Window;
    AxViewport Viewport;
    bool IsInitialized;
} HostBootstrap;

typedef struct AppData
{
    struct AxApplicationAPI *AppAPI;
    HostBootstrap *Host;
    int argc;
    char **argv;
} AppData;

static constexpr AxVec2 WINDOW_SIZE = { 1920, 1080 };

// Host bootstrapping functions
static bool HostInitialize(HostBootstrap *Host, struct AxAPIRegistry *RegistryAPI)
{
    if (!Host || !RegistryAPI) {
        return false;
    }

    memset(Host, 0, sizeof(HostBootstrap));
    Host->RegistryAPI = RegistryAPI;

    Host->PlatformAPI = (struct AxPlatformAPI *)RegistryAPI->Get(AXON_PLATFORM_API_NAME);
    Host->PluginAPI = (struct AxPluginAPI *)RegistryAPI->Get(AXON_PLUGIN_API_NAME);


    // Load required plugins
    AxWallClock loadStartTime = Host->PlatformAPI->TimeAPI->WallTime();

    if (!Host->PluginAPI->Load("libAxWindow.dll", false)) {
        fprintf(stderr, "Failed to load AxWindow plugin\n");
        return false;
    }

    if (!Host->PluginAPI->Load("libAxOpenGL.dll", false)) {
        fprintf(stderr, "Failed to load AxOpenGL plugin\n");
        return false;
    }

    if (!Host->PluginAPI->Load("libAxScene.dll", false)) {
        fprintf(stderr, "Failed to load AxScene plugin\n");
        return false;
    }

    // Get core APIs
    Host->WindowAPI = (struct AxWindowAPI *)RegistryAPI->Get(AXON_WINDOW_API_NAME);

    AxWallClock loadEndTime = Host->PlatformAPI->TimeAPI->WallTime();
    float loadTime = Host->PlatformAPI->TimeAPI->ElapsedWallTime(loadStartTime, loadEndTime);
    printf("Plugin loading completed in %.3f seconds\n", loadTime);

    if (!Host->PluginAPI || !Host->WindowAPI || !Host->PlatformAPI) {
        fprintf(stderr, "Failed to get core APIs from registry\n");
        return false;
    }

    // Initialize window system
    Host->WindowAPI->Init();



    // Get render API after plugin loading
    Host->RenderAPI = (struct AxOpenGLAPI *)RegistryAPI->Get(AXON_OPENGL_API_NAME);
    if (!Host->RenderAPI) {
        fprintf(stderr, "Failed to get OpenGL API after plugin loading\n");
        return false;
    }

    Host->IsInitialized = true;
    return true;
}

static bool HostSetupPlatform(HostBootstrap *Host)
{
    if (!Host || !Host->IsInitialized) {
        return false;
    }

    // Validate platform
    enum AxWindowError error = AX_WINDOW_ERROR_NONE;
    if (!Host->WindowAPI->ValidatePlatform(&error)) {
        fprintf(stderr, "Platform validation failed: %s\n", Host->WindowAPI->GetErrorString(error));
        return false;
    }

    // Get and display platform info
    AxPlatformInfo platformInfo = {};
    if (Host->WindowAPI->GetPlatformInfo(&platformInfo, &error)) {
        printf("Platform: %s %s\n", platformInfo.Name, platformInfo.Version);
    }

    // Set platform hints for optimal performance
    AxPlatformHints hints = {};
    hints.Windows.EnableCompositor = true;
    hints.Windows.UseImmersiveDarkMode = false;
    if (!Host->WindowAPI->SetPlatformHints(&hints, &error)) {
        fprintf(stderr, "Failed to set platform hints: %s\n", Host->WindowAPI->GetErrorString(error));
        // Continue anyway - not critical
    }

    return true;
}

static bool HostCreateWindow(HostBootstrap *Host)
{
    if (!Host || !Host->IsInitialized) {
        return false;
    }

    // Create window with configuration
    AxWindowConfig config = {
        .Title = "AxonSDK OpenGL Example",
        .X = 100,
        .Y = 100,
        .Width = static_cast<int32_t>(WINDOW_SIZE.Width),
        .Height = static_cast<int32_t>(WINDOW_SIZE.Height),
        .Style = (AxWindowStyle)(AX_WINDOW_STYLE_VISIBLE | AX_WINDOW_STYLE_DECORATED | AX_WINDOW_STYLE_RESIZABLE)
    };

    enum AxWindowError Error = AX_WINDOW_ERROR_NONE;
    Host->Window = Host->WindowAPI->CreateWindowWithConfig(&config, &Error);

    if (Error != AX_WINDOW_ERROR_NONE || !Host->Window) {
        fprintf(stderr, "Failed to create window: %s\n", Host->WindowAPI->GetErrorString(Error));
        return false;
    }

    Host->WindowAPI->SetWindowVisible(Host->Window, true, NULL);

    // Set cursor mode for FPS-style controls
    enum AxCursorMode CursorMode = AX_CURSOR_DISABLED;
    if (!Host->WindowAPI->SetCursorMode(Host->Window, CursorMode, &Error)) {
        fprintf(stderr, "Failed to set cursor mode: %s\n", Host->WindowAPI->GetErrorString(Error));
    }

    return true;
}

static bool HostCreateRenderer(HostBootstrap *Host)
{
    if (!Host || !Host->IsInitialized || !Host->Window) {
        return false;
    }

    // Setup viewport
    Host->Viewport.Position = { 0.0f, 0.0f };
    Host->Viewport.Size = { WINDOW_SIZE.Width, WINDOW_SIZE.Height };
    Host->Viewport.Scale = { 1.0f, 1.0f };
    Host->Viewport.Depth = { 0.0f, 1.0f };
    Host->Viewport.IsActive = true;
    Host->Viewport.ClearColor = { 0.42f, 0.51f, 0.54f, 0.0f };

    // Initialize renderer
    AxWindowPlatformData WindowPlatformData = Host->WindowAPI->GetPlatformData(Host->Window);
    Host->RenderAPI->CreateContext(WindowPlatformData.Win32.Handle); // Need the handle here

    // Display renderer info
    auto info = Host->RenderAPI->GetInfo(true);
    printf("Graphics Renderer Initialized:\n");
    printf("  Vendor: %s\n", info.Vendor);
    printf("  Renderer: %s\n", info.Renderer);
    printf("  OpenGL Version: %s\n", info.GLVersion);
    printf("  GLSL Version: %s\n", info.GLSLVersion);

    return true;
}

static bool HostBootstrapAll(HostBootstrap *Host, struct AxAPIRegistry *RegistryAPI)
{
    if (!HostInitialize(Host, RegistryAPI)) {
        return false;
    }

    if (!HostSetupPlatform(Host)) {
        return false;
    }

    if (!HostCreateWindow(Host)) {
        return false;
    }

    if (!HostCreateRenderer(Host)) {
        return false;
    }

    printf("Host bootstrap completed successfully\n");
    return true;
}

static void HostShutdown(HostBootstrap *host)
{
    if (!host) {
        return;
    }

    if (host->RenderAPI) {
        host->RenderAPI->DestroyContext();
    }

    if (host->Window && host->WindowAPI) {
        host->WindowAPI->DestroyWindow(host->Window);
    }

    printf("Host shutdown completed\n");
}

// Host interface functions for Engine to access host resources
static AxWindow *HostGetWindow(const HostBootstrap *Host)
{
    return Host ? Host->Window : nullptr;
}

static const AxViewport *HostGetViewport(const HostBootstrap *Host)
{
    return Host ? &Host->Viewport : nullptr;
}

static void RunApplication(const AppData *Data)
{
    // Bootstrap the host infrastructure first
    HostBootstrap Host = {};
    if (!HostBootstrapAll(&Host, AxonGlobalAPIRegistry)) {
        fprintf(stderr, "Failed to bootstrap host infrastructure\n");
        return;
    }

    // Expose host resources to Engine via registry
    if (Host.RegistryAPI) {
        // Store pointers to host resources in the API registry
        Host.RegistryAPI->Set("HostWindow", &Host.Window, sizeof(AxWindow*));
        Host.RegistryAPI->Set("HostViewport", &Host.Viewport, sizeof(AxViewport));
    }

    // Now create and run the application with pre-configured infrastructure
    struct AxApplicationAPI *AppAPI = Data->AppAPI;
    AxApplication *App = AppAPI->Create(Data->argc, Data->argv);
    if (!App) {
        HostShutdown(&Host);
        return;
    }

    while(!AppAPI->Tick(App)) {
        // TODO(mdeforge): Check for plugin hot reload
    }

    AppAPI->Destroy(App);
    HostShutdown(&Host);
}

bool Run(int argc, char *argv[])
{
    ConsoleAttached = AttachOutputToConsole();

    AxonInitGlobalAPIRegistry();
    AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);

    // Load the main DLL
    PluginAPI->Load("libOpenGLGame.dll", false);

    // Get the application interface
    struct AxApplicationAPI *AppAPI = (struct AxApplicationAPI *)AxonGlobalAPIRegistry->Get(AXON_APPLICATION_API_NAME);
    if (AppAPI->Create) {
        AppData Data = { .AppAPI = AppAPI, .argc = argc, .argv = argv };
        RunApplication(&Data);
    } else {
        fprintf(stderr, "Failed to create application!\n");
    }

    AxonTermGlobalAPIRegistry();
    FreeCon();

    return (0);
}

#ifdef AX_OS_WINDOWS

#include <shellapi.h>

// TODO(mdeforge): Get rid of the console window, but also don't use WinMain
int main(int argc, char** argv)
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);

    bool Result = Run(argc, argv);

    return (Result);
}

#endif
