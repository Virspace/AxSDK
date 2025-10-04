#pragma once

#include "Foundation/AxTypes.h"
#include <unordered_map>
#include <string>

struct AxWindow;
struct AxWindowAPI;
struct AxPluginAPI;
struct AxAPIRegistry;
struct AxOpenGLAPI;
struct AxPlatformAPI;
struct AxPlatformFileAPI;
struct AxHashTableAPI;
struct AxSceneAPI;
struct AxModel;
struct AxCamera;
struct AxMesh;
struct AxScene;
struct AxSceneObject;
struct AxTexture;
struct AxShaderData;
struct AxViewport;

struct cgltf_data;
struct cgltf_sampler;
struct cgltf_texture_view;

class Game
{
public:
    // Game lifecycle - now receives pre-configured APIs
    bool Create();
    bool Tick(float DeltaT);
    void Destroy();

    // Game logic methods
    void UpdateCamera(float DeltaTime);
    void IsRequestingExit(bool Value);
    bool IsRequestingExit() { return IsRequestingExit_; }
    inline AxTransform *GetTransform() { return Transform_; }

    // Public for Engine to access during initialization
    void Initialize(AxAPIRegistry *APIRegistry, AxWindow *Window, const AxViewport *Viewport);

    // Public for callbacks to access
    AxWindowAPI *WindowAPI_{nullptr};

    // Scene adapter methods for callbacks - need to be public for static callback access
    void LoadObjectModel(const AxSceneObject* Object);
    void SetupSceneCamera(const AxScene* Scene);
    bool LoadShadersForMaterials();
    bool LoadShadersForMaterials(const AxScene* Scene);
    void ApplySceneMaterialsToModel(AxModel* Model, const char* ObjectName);
    void AssignCompiledShadersToScene(const AxScene* Scene);
    AxShaderData *ConstructShader(const char *VertexShaderPath, const char *FragmentShaderPath);

    // Temporary storage for compiled shaders during scene parsing
    std::unordered_map<std::string, AxShaderData*> CompiledShaders;

private:
    // Game content methods
    bool LoadTexture(AxTexture *Texture, const char *Path);
    bool LoadModel(const char *File, AxModel *Model);
    void ProcessSceneObject(AxSceneObject* Object);
    bool CreateScene();
    void RenderModelWithMeshes(AxModel* Model, AxCamera* Camera, AxTransform *CameraTransform, const AxViewport* Viewport);

    // Input handling
    void RegisterCallbacks();
    void SetSamplerPropertiesFromGLTF(AxTexture* texture, cgltf_sampler* sampler);
    void EnsureCheckerTexture();
    bool LoadModelTexture(AxTexture *Texture, cgltf_texture_view *TextureView, cgltf_data *ModelData, const char *BasePath, bool IsSRGB = true);

    // Scene adapter methods for callbacks
    void RegisterSceneEventHandler();
    void UnregisterSceneEventHandler();

    // APIs - now received from Engine rather than self-managed
    AxAPIRegistry *APIRegistry_{nullptr};
    AxOpenGLAPI *RenderAPI_{nullptr};
    AxPlatformAPI *PlatformAPI_{nullptr};
    AxPlatformFileAPI *FileAPI_{nullptr};
    AxSceneAPI *SceneAPI_{nullptr};

    // Infrastructure provided by Host/Engine
    AxWindow *Window_{nullptr};
    const AxViewport *Viewport_{nullptr};

    // Game state
    AxScene *Scene_{nullptr};
    AxCamera *Camera_{nullptr};
    AxTransform *Transform_{nullptr};
    bool IsRequestingExit_{false};
};