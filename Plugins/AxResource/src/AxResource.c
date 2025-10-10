#include "Foundation/AxLinearAllocator.h"
#include "Foundation/AxPlugin.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxMath.h"
#include "Foundation/AxPlatform.h"
#include "Foundation/AxArray.h"
#include "AxResource/AxResource.h"
#include "AxResource/AxShaderManager.h"

///////////////////////////////////////////////////////////////
// Resource Manager Implementation
///////////////////////////////////////////////////////////////

static void Initialize(struct AxAPIRegistry* Registry)
{
    if (!Registry) {
        return;
    }

    // Initialize ShaderManager
    ShaderManagerAPI->Initialize(Registry, 32); // Initial capacity of 32 shaders

    // Future: Initialize other managers (TextureManager, MeshManager, etc.)
}

static void Shutdown(void)
{
    // Shutdown all managers in reverse order
    ShaderManagerAPI->Shutdown();

    // Future: Shutdown other managers
}

///////////////////////////////////////////////////////////////
// API Structure and Plugin Interface
///////////////////////////////////////////////////////////////

struct AxResourceAPI* ResourceAPI = &(struct AxResourceAPI) {
    .Initialize = Initialize,
    .Shutdown = Shutdown
};

// Plugin load function - exported for dynamic loading
AXON_DLL_EXPORT void LoadPlugin(struct AxAPIRegistry* APIRegistry, bool Load)
{
    if (APIRegistry) {
        // Register the main Resource API
        APIRegistry->Set(AXON_RESOURCE_API_NAME, ResourceAPI, sizeof(struct AxResourceAPI));

        // Register individual manager APIs (so they can be accessed directly)
        APIRegistry->Set(AXON_SHADER_MANAGER_API_NAME, ShaderManagerAPI, sizeof(struct AxShaderManagerAPI));

        // Future: Register other manager APIs
        // APIRegistry->Set(AXON_TEXTURE_MANAGER_API_NAME, TextureManagerAPI, sizeof(struct AxTextureManagerAPI));
    }
}