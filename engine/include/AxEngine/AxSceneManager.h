#pragma once

#include "Foundation/AxTypes.h"
#include "AxResource/AxResourceTypes.h"

struct AxAPIRegistry;
struct AxResourceAPI;
struct AxScene;

/**
 * AxSceneManager - Centralized scene management
 *
 * Singleton class that handles scene loading and unloading.
 * Manages the currently loaded scene and provides access to scene data.
 */
class AXENGINE_API AxSceneManager
{
public:
    static AxSceneManager& Get();

    /**
     * Initialize with API registry.
     * Must be called before Load/Unload.
     */
    void Initialize(AxAPIRegistry* Registry);

    /**
     * Shutdown and release resources.
     */
    void Shutdown();

    /**
     * Load a scene from file path.
     * Unloads any previously loaded scene.
     *
     * @param Path - Path to scene file (e.g., "scenes/sponza_atrium.ats")
     * @return true if loaded successfully, false on error
     */
    bool Load(const char* Path);

    /**
     * Unload the current scene and free resources.
     */
    void Unload();

    /**
     * Get the current scene handle.
     */
    AxSceneHandle GetHandle() const { return SceneHandle_; }

    /**
     * Get the current scene data. Returns nullptr if no scene loaded.
     */
    AxScene* GetScene() const;

    /**
     * Check if a scene is currently loaded.
     */
    bool IsLoaded() const { return AX_HANDLE_IS_VALID(SceneHandle_); }

private:
    AxSceneManager() = default;
    ~AxSceneManager();
    AxSceneManager(const AxSceneManager&) = delete;
    AxSceneManager& operator=(const AxSceneManager&) = delete;

    AxResourceAPI* ResourceAPI_{nullptr};
    AxSceneHandle SceneHandle_{AX_INVALID_HANDLE};
};
