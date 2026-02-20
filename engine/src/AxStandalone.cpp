#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxPlugin.h"
#include "AxEngine/AxEngine.h"

#include <stdio.h>
#include <stdlib.h>

// Simple bootstrap host that loads AxEngine.dll and delegates to it
int main(int argc, char** argv)
{
    printf("AxStandalone - Bootstrapping AxEngine...\n");

    // Initialize Foundation's global API registry
    AxonInitGlobalAPIRegistry();

    // Register all Foundation APIs (Platform, Plugin, etc.)
    AxonRegisterAllFoundationAPIs(AxonGlobalAPIRegistry);

    // Get PluginAPI from registry
    AxPluginAPI* PluginAPI = static_cast<AxPluginAPI*>(
        AxonGlobalAPIRegistry->Get(AXON_PLUGIN_API_NAME)
    );

    if (!PluginAPI) {
        fprintf(stderr, "ERROR: Failed to get PluginAPI from Foundation\n");
        AxonTermGlobalAPIRegistry();
        return (1);
    }

    // Load AxEngine.dll
    printf("Loading libAxEngine.dll...\n");
    uint64_t EngineHandle = PluginAPI->Load("libAxEngine.dll", false);

    if (!PluginAPI->IsValid(EngineHandle)) {
        fprintf(stderr, "ERROR: Failed to load libAxEngine.dll\n");
        AxonTermGlobalAPIRegistry();
        return (1);
    }

    // Get the engine API from the registry
    AxEngineAPI* Engine = static_cast<AxEngineAPI*>(
        AxonGlobalAPIRegistry->Get(AX_ENGINE_API_NAME)
    );

    if (!Engine) {
        fprintf(stderr, "ERROR: Failed to get AxEngineAPI from registry\n");
        PluginAPI->Unload(EngineHandle);
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
    printf("Initializing engine...\n");
    if (!Engine->Initialize(&Config)) {
        fprintf(stderr, "ERROR: Engine initialization failed\n");
        Engine->Shutdown();
        PluginAPI->Unload(EngineHandle);
        AxonTermGlobalAPIRegistry();
        return (1);
    }

    // Run engine (blocks until exit)
    printf("Running engine...\n");
    int ExitCode = Engine->Run();
    printf("Engine exited with code: %d\n", ExitCode);

    // Shutdown engine
    printf("Shutting down engine...\n");
    Engine->Shutdown();

    // Cleanup
    PluginAPI->Unload(EngineHandle);
    AxonTermGlobalAPIRegistry();

    printf("AxStandalone - Shutdown complete\n");
    return (ExitCode);
}
