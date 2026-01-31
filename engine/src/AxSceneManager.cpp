/**
 * AxSceneManager.cpp - Centralized scene management
 *
 * Singleton that handles scene loading/unloading via ResourceAPI.
 */

#include "AxEngine/AxSceneManager.h"
#include "AxResource/AxResource.h"
#include "AxResource/AxResourceTypes.h"
#include "AxScene/AxScene.h"
#include "Foundation/AxAPIRegistry.h"

#include <stdio.h>

AxSceneManager::~AxSceneManager()
{
    Shutdown();
}

AxSceneManager& AxSceneManager::Get()
{
    static AxSceneManager s_Instance;
    return (s_Instance);
}

void AxSceneManager::Initialize(AxAPIRegistry* Registry)
{
    ResourceAPI_ = static_cast<AxResourceAPI*>(Registry->Get(AXON_RESOURCE_API_NAME));
}

void AxSceneManager::Shutdown()
{
    Unload();
    ResourceAPI_ = nullptr;
}

bool AxSceneManager::Load(const char* Path)
{
    if (!ResourceAPI_) {
        fprintf(stderr, "AxSceneManager: Not initialized\n");
        return (false);
    }

    // Release existing scene if any
    Unload();

    // Load scene file using ResourceAPI
    SceneHandle_ = ResourceAPI_->LoadScene(Path);
    if (!AX_HANDLE_IS_VALID(SceneHandle_)) {
        fprintf(stderr, "AxSceneManager: Failed to load scene '%s'\n", Path);
        return (false);
    }

    // Log scene info
    AxScene* Scene = ResourceAPI_->GetScene(SceneHandle_);
    if (Scene) {
        printf("AxSceneManager: Loaded scene '%s' with %u objects, %u lights\n",
               Scene->Name, Scene->ObjectCount, Scene->LightCount);
    }

    return (true);
}

void AxSceneManager::Unload()
{
    if (!ResourceAPI_) return;

    if (AX_HANDLE_IS_VALID(SceneHandle_)) {
        ResourceAPI_->ReleaseScene(SceneHandle_);
        SceneHandle_ = AX_INVALID_HANDLE;
    }
}

AxScene* AxSceneManager::GetScene() const
{
    if (!ResourceAPI_ || !AX_HANDLE_IS_VALID(SceneHandle_)) {
        return (nullptr);
    }
    return (ResourceAPI_->GetScene(SceneHandle_));
}
