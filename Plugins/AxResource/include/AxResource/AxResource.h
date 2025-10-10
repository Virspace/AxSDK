#pragma once

#include "Foundation/AxTypes.h"
#include "AxResource/AxShaderManager.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AXON_RESOURCE_API_NAME "AxonResourceAPI"

/**
 * Main Resource API - Provides access to all resource managers
 *
 * Individual managers are also registered separately with their own API names:
 * - AxShaderManagerAPI: AXON_SHADER_MANAGER_API_NAME
 * - (Future) AxTextureManagerAPI, AxMeshManagerAPI, etc.
 */
struct AxResourceAPI {
    /**
     * Initialize all resource managers
     * @param Registry API registry for accessing other APIs
     */
    void (*Initialize)(struct AxAPIRegistry* Registry);

    /**
     * Shutdown all resource managers and free resources
     */
    void (*Shutdown)(void);
};

extern struct AxResourceAPI* ResourceAPI;

#ifdef __cplusplus
}
#endif
