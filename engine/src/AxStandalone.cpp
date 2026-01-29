#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxPlugin.h"
#include "Foundation/AxPlatform.h"
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
    printf("Loading AxEngine.dll...\n");
    uint64_t EngineHandle = PluginAPI->Load("libAxEngine.dll", false);

    if (!EngineHandle || !PluginAPI->IsValid(EngineHandle)) {
        fprintf(stderr, "ERROR: Failed to load AxEngine.dll\n");
        AxonTermGlobalAPIRegistry();
        return (1);
    }

    // Get PlatformAPI to access DLL symbol loading
    AxPlatformAPI* PlatformAPI = static_cast<AxPlatformAPI*>(
        AxonGlobalAPIRegistry->Get(AXON_PLATFORM_API_NAME)
    );

    if (!PlatformAPI || !PlatformAPI->DLLAPI) {
        fprintf(stderr, "ERROR: Failed to get PlatformAPI or DLLAPI\n");
        PluginAPI->Unload(EngineHandle);
        AxonTermGlobalAPIRegistry();
        return (1);
    }

    // Get the plugin path to load the DLL handle
    char* PluginPath = PluginAPI->GetPath(EngineHandle);
    if (!PluginPath) {
        fprintf(stderr, "ERROR: Failed to get AxEngine.dll path\n");
        PluginAPI->Unload(EngineHandle);
        AxonTermGlobalAPIRegistry();
        return (1);
    }

    // Load DLL handle for symbol lookup
    AxDLL DLLHandle = PlatformAPI->DLLAPI->Load(PluginPath);
    if (!PlatformAPI->DLLAPI->IsValid(DLLHandle)) {
        fprintf(stderr, "ERROR: Failed to load DLL handle for AxEngine.dll\n");
        PluginAPI->Unload(EngineHandle);
        AxonTermGlobalAPIRegistry();
        return (1);
    }

    // Get GetEngineAPI function pointer from DLL
    typedef AxEngineAPI* (*GetEngineAPIFunc)();
    GetEngineAPIFunc GetEngineAPI = (GetEngineAPIFunc)PlatformAPI->DLLAPI->Symbol(
        DLLHandle,
        "GetEngineAPI"
    );

    if (!GetEngineAPI) {
        fprintf(stderr, "ERROR: Failed to get GetEngineAPI symbol from DLL\n");
        PlatformAPI->DLLAPI->Unload(DLLHandle);
        PluginAPI->Unload(EngineHandle);
        AxonTermGlobalAPIRegistry();
        return (1);
    }

    // Get the engine API
    AxEngineAPI* Engine = GetEngineAPI();
    if (!Engine) {
        fprintf(stderr, "ERROR: GetEngineAPI() returned NULL\n");
        PlatformAPI->DLLAPI->Unload(DLLHandle);
        PluginAPI->Unload(EngineHandle);
        AxonTermGlobalAPIRegistry();
        return (1);
    }

    // Configure engine
    AxEngineConfig Config = {0};
    Config.PluginPath = "./plugins/";  // TODO: Make configurable
    Config.ConfigPath = nullptr;       // TODO: Load from config file
    Config.argc = argc;
    Config.argv = argv;

    // Initialize engine
    printf("Initializing engine...\n");
    if (!Engine->Initialize(&Config)) {
        fprintf(stderr, "ERROR: Engine initialization failed\n");
        Engine->Shutdown();
        PlatformAPI->DLLAPI->Unload(DLLHandle);
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
    PlatformAPI->DLLAPI->Unload(DLLHandle);
    PluginAPI->Unload(EngineHandle);
    AxonTermGlobalAPIRegistry();

    printf("AxStandalone - Shutdown complete\n");
    return (ExitCode);
}
