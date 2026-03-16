/**
 * Game Host - standalone executable that links AxEngine as a static library.
 *
 * Replaces the old AxStandalone.exe which loaded AxEngine.dll via PluginAPI.
 * Now constructs AxEngine directly as a C++ object (DEC-016).
 *
 * Game.dll (scripts) is still loaded at runtime by the engine for hot-reload.
 */

#include "Foundation/AxAPIRegistry.h"
#include "AxEngine/AxEngine.h"
#include "AxLog/AxLog.h"

int main(int argc, char** argv)
{
    // Initialize Foundation's global API registry
    AxonInitGlobalAPIRegistry();
    AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);

    // Open log file early -- sets the uptime epoch for all subsequent log calls
    AxLogOpenFile("engine.log");

    AX_LOG(INFO, "Starting game host...");

    // Configure engine
    AxEngineConfig Config {
        .PluginPath = "./plugins/",
        .ConfigPath = "",
        .argc = argc,
        .argv = argv
    };

    // Construct engine directly (static library, no DLL loading)
    AxEngine Engine;
    if (!Engine.Initialize(&Config, AxonGlobalAPIRegistry)) {
        AX_LOG(FATAL, "Engine initialization failed");
        Engine.Shutdown();
        AxLogCloseFile();
        AxonTermGlobalAPIRegistry();
        return (1);
    }

    // Run engine (blocks until exit)
    AX_LOG(INFO, "Running engine...");
    int ExitCode = Engine.Run();
    AX_LOG(INFO, "Engine exited with code: %d", ExitCode);

    // Shutdown
    Engine.Shutdown();

    AX_LOG(INFO, "Shutdown complete");
    AxLogCloseFile();
    AxonTermGlobalAPIRegistry();

    return (ExitCode);
}
