#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxPlugin.h"
#include "AxEngine/AxEngine.h"
#include "AxLog/AxLog.h"

// Simple bootstrap host that loads AxEngine.dll and delegates to it
int main(int argc, char** argv)
{
    // Initialize Foundation's global API registry
    AxonInitGlobalAPIRegistry();

    // Register all Foundation APIs (Platform, Plugin, etc.)
    AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);

    // Open log file early -- this also sets the uptime epoch for all subsequent log calls.
    // Foundation/Platform is available now, so file I/O works.
    AxLogOpenFile("engine.log");

    AX_LOG(INFO, "Bootstrapping AxEngine...");

    // Get PluginAPI from registry
    AxPluginAPI* PluginAPI = static_cast<AxPluginAPI*>(
        AxonGlobalAPIRegistry->Get(AXON_PLUGIN_API_NAME)
    );

    if (!PluginAPI) {
        AX_LOG(FATAL, "Failed to get PluginAPI from Foundation");
        AxLogCloseFile();
        AxonTermGlobalAPIRegistry();
        return (1);
    }

    // Load AxEngine.dll
    AX_LOG(INFO, "Loading libAxEngine.dll...");
    uint64_t EngineHandle = PluginAPI->Load("libAxEngine.dll", false);

    if (!PluginAPI->IsValid(EngineHandle)) {
        AX_LOG(FATAL, "Failed to load libAxEngine.dll");
        AxLogCloseFile();
        AxonTermGlobalAPIRegistry();
        return (1);
    }

    // Get the engine API from the registry
    AxEngineAPI* Engine = static_cast<AxEngineAPI*>(
        AxonGlobalAPIRegistry->Get(AX_ENGINE_API_NAME)
    );

    if (!Engine) {
        AX_LOG(FATAL, "Failed to get AxEngineAPI from registry");
        PluginAPI->Unload(EngineHandle);
        AxLogCloseFile();
        AxonTermGlobalAPIRegistry();
        return (1);
    }

    // Configure engine
    AxEngineConfig Config {
        .PluginPath = "./plugins/",  // TODO: Make configurable
        .ConfigPath = "",
        .argc = argc,
        .argv = argv
    };

    // Initialize engine
    AX_LOG(INFO, "Initializing engine...");
    if (!Engine->Initialize(&Config)) {
        AX_LOG(FATAL, "Engine initialization failed");
        Engine->Shutdown();
        PluginAPI->Unload(EngineHandle);
        AxLogCloseFile();
        AxonTermGlobalAPIRegistry();
        return (1);
    }

    // Run engine (blocks until exit)
    AX_LOG(INFO, "Running engine...");
    int ExitCode = Engine->Run();
    AX_LOG(INFO, "Engine exited with code: %d", ExitCode);

    // Shutdown engine
    AX_LOG(INFO, "Shutting down engine...");
    Engine->Shutdown();

    // Cleanup
    PluginAPI->Unload(EngineHandle);

    AX_LOG(INFO, "Shutdown complete");

    AxLogCloseFile();
    AxonTermGlobalAPIRegistry();

    return (ExitCode);
}
