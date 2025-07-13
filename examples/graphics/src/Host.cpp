#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxPlugin.h"
#include "Foundation/AxApplication.h"

#ifdef AX_OS_WINDOWS
#include <windows.h>
#include <conio.h>
#include <stdio.h>
#include <string.h>

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

typedef struct AppData
{
    struct AxApplicationAPI *AppAPI;
    int argc;
    // Pad 4?
    char **argv;
} AppData;

static void RunApplication(const AppData *Data)
{
    struct AxApplicationAPI *AppAPI = Data->AppAPI;
    AxApplication *App = AppAPI->Create(Data->argc, Data->argv);
    if (!App) {
        return;
    }

    while(!AppAPI->Tick(App)) {
        // TODO(mdeforge): Check for plugin hot reload
    }

    AppAPI->Destroy(App);
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
