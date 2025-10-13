#include "AxEngine/AxEngine.h"
#include "AxOpenGL/AxOpenGL.h"
#include "AxResource/AxResource.h"
#include "AxResource/AxShaderManager.h"
#include "AxScene/AxScene.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxPlatform.h"
#include "Foundation/AxMath.h"
#include "Foundation/AxHashTable.h"

#define AXARRAY_IMPLEMENTATION
#include "Foundation/AxArray.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// STB Image for texture loading
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// CGLTF for GLTF model loading
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

///////////////////////////////////////////////////////////////
// GLTF Helper Functions
///////////////////////////////////////////////////////////////

static AxTextureWrapMode ConvertGLTFWrapMode(int32_t GLTFWrapMode)
{
    switch (GLTFWrapMode) {
        case 33071: return (AX_TEXTURE_WRAP_CLAMP_TO_EDGE);
        case 33648: return (AX_TEXTURE_WRAP_MIRRORED_REPEAT);
        case 10497: return (AX_TEXTURE_WRAP_REPEAT);
        default: return (AX_TEXTURE_WRAP_REPEAT);
    }
}

static AxTextureFilter ConvertGLTFFilter(int32_t GLTFFilter)
{
    switch (GLTFFilter) {
        case 9728: return (AX_TEXTURE_FILTER_NEAREST);
        case 9729: return (AX_TEXTURE_FILTER_LINEAR);
        case 9984: return (AX_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST);
        case 9985: return (AX_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST);
        case 9986: return (AX_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR);
        case 9987: return (AX_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR);
        default: return (AX_TEXTURE_FILTER_LINEAR);
    }
}

static void SetSamplerPropertiesFromGLTF(
    struct AxOpenGLAPI* OpenGLAPI,
    AxTexture* Texture,
    cgltf_sampler* Sampler
)
{
    // Default filtering: LINEAR with mipmaps for minification
    AxTextureWrapMode WrapS = AX_TEXTURE_WRAP_REPEAT;
    AxTextureWrapMode WrapT = AX_TEXTURE_WRAP_REPEAT;
    AxTextureFilter MagFilter = AX_TEXTURE_FILTER_LINEAR;
    AxTextureFilter MinFilter = AX_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR;

    if (Sampler) {
        WrapS = ConvertGLTFWrapMode(Sampler->wrap_s);
        WrapT = ConvertGLTFWrapMode(Sampler->wrap_t);
        MagFilter = ConvertGLTFFilter(Sampler->mag_filter);
        MinFilter = ConvertGLTFFilter(Sampler->min_filter);
    }

    OpenGLAPI->SetTextureWrapMode(Texture, WrapS, WrapT);
    OpenGLAPI->SetTextureFilterMode(Texture, MagFilter, MinFilter);
}

static void LoadFloatAttribute(
    cgltf_accessor* Accessor,
    float** OutData,
    size_t* OutCount,
    size_t Components
)
{
    if (!Accessor || !OutData || !OutCount) {
        return;
    }

    size_t TotalElements = Accessor->count * Components;
    float* Data = (float*)malloc(TotalElements * sizeof(float));
    if (!Data) {
        *OutData = NULL;
        *OutCount = 0;
        return;
    }

    for (cgltf_size i = 0; i < Accessor->count; ++i) {
        float Values[4] = {0};
        cgltf_accessor_read_float(Accessor, i, Values, Components);

        for (size_t j = 0; j < Components; ++j) {
            Data[i * Components + j] = Values[j];
        }
    }

    *OutData = Data;
    *OutCount = Accessor->count;
}

static bool LoadModelTexture(
    struct AxOpenGLAPI* OpenGLAPI,
    AxTexture* Texture,
    cgltf_texture_view* TextureView,
    cgltf_data* ModelData,
    const char* BasePath,
    bool IsSRGB
)
{
    if (!TextureView->texture) {
        return (false);
    }

    // Handle KTX2/BasisU textures (unsupported)
    if (!TextureView->texture->image && TextureView->texture->basisu_image) {
        cgltf_image* Img = TextureView->texture->basisu_image;
        const char* Name = Img->name ? Img->name : (Img->uri ? Img->uri : "(unnamed)");
        fprintf(stderr,
            "WARNING: Unsupported KTX2/BasisU image '%s' - texture will be missing.\n"
            "         Add a KTX2 loader or re-export as PNG/JPG.\n", Name);
        return (false);
    }

    if (!TextureView->texture->image) {
        return (false);
    }

    cgltf_image* Image = TextureView->texture->image;
    cgltf_sampler* Sampler = TextureView->texture->sampler;

    stbi_uc* Pixels = NULL;
    int Width, Height, Channels;

    // Load from URI (external file)
    if (Image->uri) {
        char ImagePath[1024];
        snprintf(ImagePath, sizeof(ImagePath), "%s%s", BasePath, Image->uri);

        Pixels = stbi_load(ImagePath, &Width, &Height, &Channels, 0);
        if (!Pixels) {
            fprintf(stderr, "Error: Failed to load image '%s'\n", ImagePath);
            return (false);
        }
    }
    // Load from embedded buffer
    else if (Image->buffer_view) {
        cgltf_buffer_view* BufferView = Image->buffer_view;
        cgltf_buffer* Buffer = BufferView->buffer;

        const stbi_uc* ImagePtr = (const stbi_uc*)Buffer->data + BufferView->offset;
        Pixels = stbi_load_from_memory(ImagePtr, (int)BufferView->size, &Width, &Height, &Channels, 0);

        if (!Pixels) {
            const char* StbiError = stbi_failure_reason();
            fprintf(stderr, "Error: Failed to load embedded image: %s (size: %zu bytes)\n",
                    StbiError ? StbiError : "unknown error", BufferView->size);
            return (false);
        }
    }
    else {
        fprintf(stderr, "Warning: Image '%s' has neither URI nor buffer view.\n",
                Image->name ? Image->name : "Unnamed");
        return (false);
    }

    Texture->Width = (uint32_t)Width;
    Texture->Height = (uint32_t)Height;
    Texture->Channels = (uint32_t)Channels;
    Texture->IsSRGB = IsSRGB;

    OpenGLAPI->InitTexture(Texture, Pixels);
    OpenGLAPI->BindTexture(Texture, 0);
    SetSamplerPropertiesFromGLTF(OpenGLAPI, Texture, Sampler);

    stbi_image_free(Pixels);
    return (true);
}

///////////////////////////////////////////////////////////////
// Engine Handle Structure
///////////////////////////////////////////////////////////////

struct AxEngineHandle {
    struct AxAPIRegistry* Registry;
    struct AxOpenGLAPI* OpenGLAPI;
    struct AxShaderManagerAPI* ShaderManagerAPI;
    struct AxSceneAPI* SceneAPI;
    struct AxPlatformAPI* PlatformAPI;
    struct AxResourceAPI* ResourceAPI;

    // Scene management
    AxScene* LoadedScene;

    // Model storage (hash table: object name -> AxModel)
    struct AxHashTable* LoadedModels;

    // Shader compilation cache (dynamic array of shader handles)
    AxShaderHandle* CompiledShaders;
};

///////////////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////////////

static AxEngineHandle* EngineCreate(struct AxAPIRegistry* Registry);
static bool EngineInitialize(AxEngineHandle* Engine);
static void EngineUpdate(AxEngineHandle* Engine, float DeltaTime);
static void EngineDestroy(AxEngineHandle* Engine);
static AxScene* EngineLoadScene(AxEngineHandle* Engine, const char* ScenePath);
static AxScene* EngineGetScene(AxEngineHandle* Engine);
static AxCamera* EngineGetSceneCamera(AxEngineHandle* Engine, uint32_t CameraIndex, AxTransform** OutTransform);
static void EngineUpdateSystems(AxEngineHandle* Engine, float DeltaTime);
static AxTexture* EngineLoadTexture(AxEngineHandle* Engine, const char* Path, bool IsSRGB);
static AxMesh* EngineLoadMesh(AxEngineHandle* Engine, const char* Path);
static bool EngineLoadModel(AxEngineHandle* Engine, const char* Path, AxModel* Model);
static uint32_t EngineLoadShader(AxEngineHandle* Engine, const char* VertPath, const char* FragPath);
static void EngineRenderScene(AxEngineHandle* Engine, const AxViewport* Viewport, uint32_t CameraIndex);

// Scene event callback forward declarations
static void EngineOnLightParsed(const AxLight* Light, void* UserData);
static void EngineOnMaterialParsed(const AxMaterial* Material, void* UserData);
static void EngineOnObjectParsed(const AxSceneObject* Object, void* UserData);
static void EngineOnTransformParsed(const AxTransform* Transform, void* UserData);
static void EngineOnSceneParsed(const AxScene* Scene, void* UserData);

///////////////////////////////////////////////////////////////
// Scene Event Callbacks
///////////////////////////////////////////////////////////////

static void EngineOnLightParsed(const AxLight* Light, void* UserData)
{
    // Lights are automatically added to the scene by SceneAPI
    printf("Engine: Light parsed - %s (Type: %d, Intensity: %.2f)\n",
           Light->Name, Light->Type, Light->Intensity);
}

static void EngineOnMaterialParsed(const AxMaterial* Material, void* UserData)
{
    AxEngineHandle* Engine = (AxEngineHandle*)UserData;
    printf("Engine: Material parsed - %s (Vertex: %s, Fragment: %s)\n",
           Material->Name, Material->VertexShaderPath, Material->FragmentShaderPath);

    // Load and compile shaders immediately when material is parsed
    if (Material->VertexShaderPath[0] == '\0' || Material->FragmentShaderPath[0] == '\0') {
        fprintf(stderr, "Engine: Material '%s' has empty shader paths, skipping\n", Material->Name);
        return;
    }

    AxShaderHandle ShaderHandle = Engine->ShaderManagerAPI->CreateShader(
        Material->VertexShaderPath,
        Material->FragmentShaderPath
    );

    if (!Engine->ShaderManagerAPI->IsValid(ShaderHandle)) {
        fprintf(stderr, "Engine: Failed to compile shaders for material '%s'\n", Material->Name);
        return;
    }

    // Store compiled shader handle in array
    ArrayPush(Engine->CompiledShaders, ShaderHandle);
    printf("Engine: Shaders compiled for material '%s' (Handle index: %zu)\n",
           Material->Name, ArraySize(Engine->CompiledShaders) - 1);
}

static void EngineOnObjectParsed(const AxSceneObject* Object, void* UserData)
{
    AxEngineHandle* Engine = (AxEngineHandle*)UserData;
    printf("Engine: Object parsed - %s (Mesh: %s)\n", Object->Name, Object->MeshPath);

    // Automatically load models for objects with mesh paths
    if (Object->MeshPath[0] != '\0') {
        AxModel LoadedModel = {0};
        if (EngineLoadModel(Engine, Object->MeshPath, &LoadedModel)) {
            // Store model in hash table using object name as key
            struct AxHashTableAPI* HashTableAPI =
                (struct AxHashTableAPI*)Engine->Registry->Get(AXON_HASH_TABLE_API_NAME);
            if (HashTableAPI && Engine->LoadedModels) {
                // Allocate model on heap to store in hash table
                AxModel* ModelPtr = (AxModel*)malloc(sizeof(AxModel));
                if (ModelPtr) {
                    *ModelPtr = LoadedModel;
                    HashTableAPI->Set(Engine->LoadedModels, Object->Name, ModelPtr);
                    printf("Engine: Stored model for object '%s'\n", Object->Name);
                }
            }
        }
    }
}

static void EngineOnTransformParsed(const AxTransform* Transform, void* UserData)
{
    // Transforms are automatically handled by the scene object hierarchy
    printf("Engine: Transform parsed - Position(%.2f, %.2f, %.2f)\n",
           Transform->Translation.X, Transform->Translation.Y, Transform->Translation.Z);
}

static void EngineOnSceneParsed(const AxScene* Scene, void* UserData)
{
    AxEngineHandle* Engine = (AxEngineHandle*)UserData;

    // Assign compiled shaders to scene materials
    size_t ShaderCount = ArraySize(Engine->CompiledShaders);
    for (size_t i = 0; i < Scene->MaterialCount && i < ShaderCount; i++) {
        AxMaterial* Material = &Scene->Materials[i];
        AxShaderHandle Handle = Engine->CompiledShaders[i];

        if (Engine->ShaderManagerAPI->IsValid(Handle)) {
            const AxShaderData* ShaderData = Engine->ShaderManagerAPI->GetShaderData(Handle);
            uint32_t ShaderProgram = Engine->ShaderManagerAPI->GetShaderProgram(Handle);

            Material->ShaderProgram = ShaderProgram;
            Material->ShaderData = (AxShaderData*)ShaderData;
        }
    }

    // Apply scene materials to loaded models
    struct AxHashTableAPI* HashTableAPI =
        (struct AxHashTableAPI*)Engine->Registry->Get(AXON_HASH_TABLE_API_NAME);
    if (HashTableAPI && Engine->LoadedModels) {
        for (uint32_t objIdx = 0; objIdx < Scene->ObjectCount; objIdx++) {
            AxSceneObject* Object = &Scene->RootObject[objIdx];
            if (!Object || Object->MeshPath[0] == '\0') {
                continue;
            }

            // Find loaded model for this object
            AxModel* Model = (AxModel*)HashTableAPI->Find(Engine->LoadedModels, Object->Name);
            if (!Model || !Model->Materials) {
                continue;
            }

            // Find the scene material specified by the object
            if (Object->MaterialPath[0] == '\0') {
                continue;
            }

            AxMaterial* SceneMaterial = Engine->SceneAPI->FindMaterial(Scene, Object->MaterialPath);
            if (!SceneMaterial || !SceneMaterial->ShaderData) {
                continue;
            }

            // Apply scene material to ALL model materials (matches Game.cpp behavior)
            for (size_t matIdx = 0; matIdx < ArraySize(Model->Materials); matIdx++) {
                AxMaterial* ModelMat = &Model->Materials[matIdx];
                ModelMat->ShaderData = SceneMaterial->ShaderData;
                ModelMat->ShaderProgram = SceneMaterial->ShaderProgram;
            }
        }
    }
}

///////////////////////////////////////////////////////////////
// Lifecycle Functions
///////////////////////////////////////////////////////////////

static AxEngineHandle* EngineCreate(struct AxAPIRegistry* Registry)
{
    if (!Registry) {
        return (NULL);
    }

    AxEngineHandle* Engine = (AxEngineHandle*)malloc(sizeof(AxEngineHandle));
    if (!Engine) {
        return (NULL);
    }

    memset(Engine, 0, sizeof(AxEngineHandle));
    Engine->Registry = Registry;

    return (Engine);
}

static bool EngineInitialize(AxEngineHandle* Engine)
{
    if (!Engine || !Engine->Registry) {
        return (false);
    }

    // Query required plugin APIs
    Engine->OpenGLAPI = (struct AxOpenGLAPI*)Engine->Registry->Get(AXON_OPENGL_API_NAME);
    Engine->ShaderManagerAPI = (struct AxShaderManagerAPI*)Engine->Registry->Get(AXON_SHADER_MANAGER_API_NAME);
    Engine->PlatformAPI = (struct AxPlatformAPI*)Engine->Registry->Get(AXON_PLATFORM_API_NAME);
    Engine->ResourceAPI = (struct AxResourceAPI*)Engine->Registry->Get(AXON_RESOURCE_API_NAME);

    // OpenGL API is required
    if (!Engine->OpenGLAPI) {
        return (false);
    }

    // ShaderManagerAPI is required for shader loading
    if (!Engine->ShaderManagerAPI) {
        return (false);
    }

    // ResourceAPI is required for resource management
    if (!Engine->ResourceAPI) {
        return (false);
    }

    // Initialize ResourceAPI
    Engine->ResourceAPI->Initialize(Engine->Registry);

    // Initialize model storage hash table
    struct AxHashTableAPI* HashTableAPI = (struct AxHashTableAPI*)Engine->Registry->Get(AXON_HASH_TABLE_API_NAME);
    if (HashTableAPI) {
        Engine->LoadedModels = HashTableAPI->CreateTable();
    }

    // Initialize shader array as NULL (will be allocated on first use)
    Engine->CompiledShaders = NULL;

    // SceneAPI is optional (may be queried later)
    Engine->SceneAPI = (struct AxSceneAPI*)Engine->Registry->Get(AXON_SCENE_API_NAME);

    return (true);
}

static void EngineUpdate(AxEngineHandle* Engine, float DeltaTime)
{
    if (!Engine) {
        return;
    }

    // Future: Resource streaming, async loading, etc.
    AXON_UNUSED(DeltaTime);
}

static void EngineDestroy(AxEngineHandle* Engine)
{
    if (!Engine) {
        return;
    }

    // Clean up loaded resources
    if (Engine->CompiledShaders) {
        // Release all shader handles
        size_t ShaderCount = ArraySize(Engine->CompiledShaders);
        for (size_t i = 0; i < ShaderCount; i++) {
            if (Engine->ShaderManagerAPI && Engine->ShaderManagerAPI->IsValid(Engine->CompiledShaders[i])) {
                Engine->ShaderManagerAPI->Release(Engine->CompiledShaders[i]);
            }
        }
        ArrayFree(Engine->CompiledShaders);
    }

    // Clean up hash table
    if (Engine->LoadedModels) {
        struct AxHashTableAPI* HashTableAPI = (struct AxHashTableAPI*)Engine->Registry->Get(AXON_HASH_TABLE_API_NAME);
        if (HashTableAPI) {
            HashTableAPI->DestroyTable(Engine->LoadedModels);
        }
    }

    // Shutdown ResourceAPI
    if (Engine->ResourceAPI) {
        Engine->ResourceAPI->Shutdown();
    }

    free(Engine);
}

///////////////////////////////////////////////////////////////
// Orchestration Functions
///////////////////////////////////////////////////////////////

static AxScene* EngineLoadScene(AxEngineHandle* Engine, const char* ScenePath)
{
    if (!Engine || !ScenePath) {
        return (NULL);
    }

    // Ensure SceneAPI is available
    if (!Engine->SceneAPI) {
        Engine->SceneAPI = (struct AxSceneAPI*)Engine->Registry->Get(AXON_SCENE_API_NAME);
    }

    if (!Engine->SceneAPI || !Engine->SceneAPI->LoadSceneFromFile) {
        fprintf(stderr, "AxScene plugin not available or incomplete\n");
        return (NULL);
    }

    // Register scene event callbacks before loading
    AxSceneEvents Events = {0};
    Events.OnLightParsed = EngineOnLightParsed;
    Events.OnMaterialParsed = EngineOnMaterialParsed;
    Events.OnObjectParsed = EngineOnObjectParsed;
    Events.OnTransformParsed = EngineOnTransformParsed;
    Events.OnSceneParsed = EngineOnSceneParsed;
    Events.UserData = Engine;  // Pass Engine as UserData so callbacks can access it

    AxSceneResult Result = Engine->SceneAPI->RegisterEventHandler(&Events);
    if (Result != AX_SCENE_SUCCESS) {
        fprintf(stderr, "Engine: Failed to register scene event handler: %s\n",
                Engine->SceneAPI->GetLastError());
        return (NULL);
    }

    // Load scene using AxScene plugin (callbacks will be invoked during parsing)
    Engine->LoadedScene = Engine->SceneAPI->LoadSceneFromFile(ScenePath);

    // Unregister callbacks after loading
    Engine->SceneAPI->UnregisterEventHandler(&Events);

    if (!Engine->LoadedScene) {
        const char* Error = Engine->SceneAPI->GetLastError ? Engine->SceneAPI->GetLastError() : "Unknown error";
        fprintf(stderr, "Failed to load scene '%s': %s\n", ScenePath, Error);
        return (NULL);
    }

    return (Engine->LoadedScene);
}

static AxScene* EngineGetScene(AxEngineHandle* Engine)
{
    if (!Engine) {
        return (NULL);
    }
    return (Engine->LoadedScene);
}

static AxCamera* EngineGetSceneCamera(AxEngineHandle* Engine, uint32_t CameraIndex, AxTransform** OutTransform)
{
    if (!Engine || !Engine->LoadedScene || !Engine->SceneAPI) {
        return (NULL);
    }

    AxScene* Scene = Engine->LoadedScene;

    // If scene has cameras, use GetCamera API
    if (Scene->CameraCount > 0) {
        return (Engine->SceneAPI->GetCamera(Scene, CameraIndex, OutTransform));
    }

    // If no cameras exist in scene, create a default camera using SceneAPI
    if (Scene->CameraCount == 0) {
        // Ensure OpenGLAPI is available for camera creation
        if (!Engine->OpenGLAPI) {
            Engine->OpenGLAPI = (struct AxOpenGLAPI*)Engine->Registry->Get(AXON_OPENGL_API_NAME);
            if (!Engine->OpenGLAPI) {
                fprintf(stderr, "Engine: OpenGLAPI not available for camera creation\n");
                return (NULL);
            }
        }

        // Create default camera and initialize it
        AxCamera DefaultCamera = {0};
        Engine->OpenGLAPI->CreateCamera(&DefaultCamera);

        // Create default transform
        AxTransform DefaultTransform = {
            .Translation = {0.0f, 2.0f, 5.0f},
            .Rotation = {0.0f, 0.0f, 0.0f, 1.0f},
            .Scale = {1.0f, 1.0f, 1.0f},
            .Up = {0.0f, 1.0f, 0.0f}
        };

        // Add camera to scene using SceneAPI
        AxSceneResult Result = Engine->SceneAPI->AddCamera(Scene, &DefaultCamera, &DefaultTransform);
        if (Result != AX_SCENE_SUCCESS) {
            fprintf(stderr, "Engine: Failed to add default camera to scene\n");
            return (NULL);
        }

        // Now get the camera from the scene
        return (Engine->SceneAPI->GetCamera(Scene, 0, OutTransform));
    }

    // Requested index out of bounds
    fprintf(stderr, "Engine: Camera index %u out of bounds (count: %u)\n", CameraIndex, Scene->CameraCount);
    return (NULL);
}

static void EngineUpdateSystems(AxEngineHandle* Engine, float DeltaTime)
{
    // Alias for Update
    EngineUpdate(Engine, DeltaTime);
}

///////////////////////////////////////////////////////////////
// Resource Loading Functions
///////////////////////////////////////////////////////////////

static AxTexture* EngineLoadTexture(
    AxEngineHandle* Engine,
    const char* Path,
    bool IsSRGB
)
{
    if (!Engine || !Engine->OpenGLAPI || !Path) {
        return (NULL);
    }

    // Allocate texture structure
    AxTexture* Texture = (AxTexture*)malloc(sizeof(AxTexture));
    if (!Texture) {
        return (NULL);
    }

    memset(Texture, 0, sizeof(AxTexture));
    Texture->IsSRGB = IsSRGB;

    // Load image using STB image
    int Width, Height, Channels;
    stbi_uc* Pixels = stbi_load(Path, &Width, &Height, &Channels, 0);

    if (!Pixels) {
        free(Texture);
        return (NULL);
    }

    Texture->Width = (uint32_t)Width;
    Texture->Height = (uint32_t)Height;
    Texture->Channels = (uint32_t)Channels;

    // Initialize texture using OpenGL API
    Engine->OpenGLAPI->InitTexture(Texture, Pixels);

    // Free image data
    stbi_image_free(Pixels);

    return (Texture);
}

static AxMesh* EngineLoadMesh(AxEngineHandle* Engine, const char* Path)
{
    if (!Engine || !Engine->OpenGLAPI || !Path) {
        return (NULL);
    }

    // Parse GLTF file
    cgltf_options Options = {0};
    cgltf_data* ModelData = NULL;
    cgltf_result Result = cgltf_parse_file(&Options, Path, &ModelData);

    if (Result != cgltf_result_success) {
        return (NULL);
    }

    Result = cgltf_load_buffers(&Options, ModelData, Path);
    if (Result != cgltf_result_success) {
        cgltf_free(ModelData);
        return (NULL);
    }

    // Simplified: Load only first mesh (full implementation would load all)
    if (ModelData->meshes_count == 0 || ModelData->meshes[0].primitives_count == 0) {
        cgltf_free(ModelData);
        return (NULL);
    }

    cgltf_primitive* Primitive = &ModelData->meshes[0].primitives[0];

    // Find position accessor to get vertex count
    cgltf_accessor* PosAccessor = NULL;
    for (cgltf_size i = 0; i < Primitive->attributes_count; ++i) {
        if (Primitive->attributes[i].type == cgltf_attribute_type_position) {
            PosAccessor = Primitive->attributes[i].data;
            break;
        }
    }

    if (!PosAccessor) {
        cgltf_free(ModelData);
        return (NULL);
    }

    size_t VertexCount = PosAccessor->count;
    size_t IndexCount = Primitive->indices ? Primitive->indices->count : 0;

    // Allocate vertex buffer
    AxVertex* Vertices = (AxVertex*)malloc(sizeof(AxVertex) * VertexCount);
    if (!Vertices) {
        cgltf_free(ModelData);
        return (NULL);
    }
    memset(Vertices, 0, sizeof(AxVertex) * VertexCount);

    // Extract vertex attributes
    for (cgltf_size i = 0; i < Primitive->attributes_count; ++i) {
        cgltf_attribute* Attr = &Primitive->attributes[i];

        if (Attr->type == cgltf_attribute_type_position) {
            for (cgltf_size v = 0; v < VertexCount; ++v) {
                cgltf_accessor_read_float(Attr->data, v, &Vertices[v].Position.X, 3);
            }
        }
        else if (Attr->type == cgltf_attribute_type_normal) {
            for (cgltf_size v = 0; v < VertexCount; ++v) {
                cgltf_accessor_read_float(Attr->data, v, &Vertices[v].Normal.X, 3);
            }
        }
        else if (Attr->type == cgltf_attribute_type_texcoord) {
            for (cgltf_size v = 0; v < VertexCount; ++v) {
                cgltf_accessor_read_float(Attr->data, v, &Vertices[v].TexCoord.U, 2);
            }
        }
        else if (Attr->type == cgltf_attribute_type_tangent) {
            for (cgltf_size v = 0; v < VertexCount; ++v) {
                cgltf_accessor_read_float(Attr->data, v, &Vertices[v].Tangent.X, 4);
            }
        }
    }

    // Extract indices
    uint32_t* Indices = NULL;
    if (Primitive->indices) {
        Indices = (uint32_t*)malloc(sizeof(uint32_t) * IndexCount);
        if (Indices) {
            for (cgltf_size i = 0; i < IndexCount; ++i) {
                Indices[i] = (uint32_t)cgltf_accessor_read_index(Primitive->indices, i);
            }
        }
    }

    // Create mesh structure
    AxMesh* Mesh = (AxMesh*)malloc(sizeof(AxMesh));
    if (!Mesh) {
        free(Vertices);
        free(Indices);
        cgltf_free(ModelData);
        return (NULL);
    }
    memset(Mesh, 0, sizeof(AxMesh));

    // Initialize mesh with OpenGL buffers
    Engine->OpenGLAPI->InitMesh(Mesh, Vertices, Indices, (uint32_t)VertexCount, (uint32_t)IndexCount);

    snprintf(Mesh->Name, sizeof(Mesh->Name), "%s", ModelData->meshes[0].name ? ModelData->meshes[0].name : "Mesh");

    free(Vertices);
    if (Indices) {
        free(Indices);
    }
    cgltf_free(ModelData);

    return (Mesh);
}

static uint32_t EngineLoadShader(
    AxEngineHandle* Engine,
    const char* VertPath,
    const char* FragPath
)
{
    if (!Engine || !Engine->ShaderManagerAPI || !VertPath || !FragPath) {
        return (0);
    }

    // Use ShaderManagerAPI to create shader
    AxShaderHandle ShaderHandle = Engine->ShaderManagerAPI->CreateShader(VertPath, FragPath);

    if (!Engine->ShaderManagerAPI->IsValid(ShaderHandle)) {
        return (0);
    }

    uint32_t Program = Engine->ShaderManagerAPI->GetShaderProgram(ShaderHandle);
    return (Program);
}

static void EngineRenderScene(
    AxEngineHandle* Engine,
    const AxViewport* Viewport,
    uint32_t CameraIndex
)
{
    if (!Engine || !Engine->LoadedScene || !Viewport || !Engine->OpenGLAPI) {
        return;
    }

    // Get camera and transform
    AxTransform* CameraTransform = NULL;
    AxCamera* Camera = EngineGetSceneCamera(Engine, CameraIndex, &CameraTransform);
    if (!Camera || !CameraTransform) {
        return;
    }

    // Compute view and projection matrices
    AxMat4x4 ViewMatrix = CreateViewMatrix(CameraTransform);
    AxMat4x4 ProjectionMatrix = Engine->OpenGLAPI->CameraGetProjectionMatrix(Camera);
    AxVec3 ViewPos = CameraTransform->Translation;

    // Get scene light count (max 8 lights supported by shader)
    int LightCount = 0;
    if (Engine->LoadedScene->Lights) {
        LightCount = (Engine->LoadedScene->LightCount < 8) ? Engine->LoadedScene->LightCount : 8;
    }

    // Enable blending for transparent objects
    Engine->OpenGLAPI->EnableBlending(true);
    Engine->OpenGLAPI->SetBlendFunction(AX_BLEND_SRC_ALPHA, AX_BLEND_ONE_MINUS_SRC_ALPHA);

    // Get hash table API for looking up loaded models
    struct AxHashTableAPI* HashTableAPI =
        (struct AxHashTableAPI*)Engine->Registry->Get(AXON_HASH_TABLE_API_NAME);
    if (!HashTableAPI || !Engine->LoadedModels) {
        Engine->OpenGLAPI->EnableBlending(false);
        return;
    }

    // Iterate through all scene objects
    for (uint32_t objIdx = 0; objIdx < Engine->LoadedScene->ObjectCount; objIdx++) {
        AxSceneObject* Object = &Engine->LoadedScene->RootObject[objIdx];
        if (!Object || Object->MeshPath[0] == '\0') {
            continue;
        }

        // Look up loaded model from hash table
        AxModel* Model = (AxModel*)HashTableAPI->Find(Engine->LoadedModels, Object->Name);
        if (!Model || !Model->Meshes) {
            continue;
        }

        // Render all meshes in the model
        for (int meshIdx = 0; meshIdx < ArraySize(Model->Meshes); meshIdx++) {
            AxMesh* Mesh = &Model->Meshes[meshIdx];

            // Use MaterialDesc system
            if (!Model->MaterialDescs || Mesh->MaterialIndex >= ArraySize(Model->MaterialDescs)) {
                continue;  // No material available
            }

            AxMaterialDesc* MatDesc = &Model->MaterialDescs[Mesh->MaterialIndex];

            // Get shader data from legacy material array (temporary until shader management is refactored)
            AxShaderData* ShaderData = NULL;
            if (Model->Materials && Mesh->MaterialIndex < ArraySize(Model->Materials)) {
                ShaderData = (AxShaderData*)Model->Materials[Mesh->MaterialIndex].ShaderData;
            }

            if (!ShaderData) {
                continue;
            }

            // Transform matrices
            AxMat4x4 ModelMatrix = Model->Transforms[Mesh->TransformIndex];
            Engine->OpenGLAPI->SetUniform(ShaderData, "model", &ModelMatrix);
            Engine->OpenGLAPI->SetUniform(ShaderData, "view", &ViewMatrix);
            Engine->OpenGLAPI->SetUniform(ShaderData, "projection", &ProjectionMatrix);
            Engine->OpenGLAPI->SetUniform(ShaderData, "viewPos", &ViewPos);

            // Use MaterialDesc PBR system
            if (MatDesc->Type != AX_MATERIAL_TYPE_PBR) {
                continue;  // Only PBR materials supported for now
            }

            const AxPBRMaterial* PBR = &MatDesc->PBR;

            // Bind textures to their respective slots
            // Base color texture (slot 0)
            if (PBR->BaseColorTexture >= 0 && Model->Textures &&
                PBR->BaseColorTexture < (int32_t)ArraySize(Model->Textures)) {
                AxTexture* Tex = &Model->Textures[PBR->BaseColorTexture];
                if (Tex->ID != 0) {
                    Engine->OpenGLAPI->BindTexture(Tex, 0);
                }
            }

            // Normal texture (slot 1)
            if (PBR->NormalTexture >= 0 && Model->Textures &&
                PBR->NormalTexture < (int32_t)ArraySize(Model->Textures)) {
                AxTexture* Tex = &Model->Textures[PBR->NormalTexture];
                if (Tex->ID != 0) {
                    Engine->OpenGLAPI->BindTexture(Tex, 1);
                }
            }

            // Metallic-Roughness texture (slot 2)
            if (PBR->MetallicRoughnessTexture >= 0 && Model->Textures &&
                PBR->MetallicRoughnessTexture < (int32_t)ArraySize(Model->Textures)) {
                AxTexture* Tex = &Model->Textures[PBR->MetallicRoughnessTexture];
                if (Tex->ID != 0) {
                    Engine->OpenGLAPI->BindTexture(Tex, 2);
                }
            }

            // Emissive texture (slot 3)
            if (PBR->EmissiveTexture >= 0 && Model->Textures &&
                PBR->EmissiveTexture < (int32_t)ArraySize(Model->Textures)) {
                AxTexture* Tex = &Model->Textures[PBR->EmissiveTexture];
                if (Tex->ID != 0) {
                    Engine->OpenGLAPI->BindTexture(Tex, 3);
                }
            }

            // Occlusion texture (slot 4)
            if (PBR->OcclusionTexture >= 0 && Model->Textures &&
                PBR->OcclusionTexture < (int32_t)ArraySize(Model->Textures)) {
                AxTexture* Tex = &Model->Textures[PBR->OcclusionTexture];
                if (Tex->ID != 0) {
                    Engine->OpenGLAPI->BindTexture(Tex, 4);
                }
            }

            // Set all PBR material uniforms, texture samplers, and lights in one efficient call
            // This replaces ~25 individual SetUniform calls and eliminates uniform warnings
            Engine->OpenGLAPI->SetPBRMaterialUniforms(ShaderData, PBR, Engine->LoadedScene->Lights, LightCount);

            // Configure depth and blending based on alpha mode
            switch (PBR->AlphaMode) {
                case AX_ALPHA_MODE_OPAQUE:
                    Engine->OpenGLAPI->EnableBlending(false);
                    Engine->OpenGLAPI->SetDepthWrite(true);
                    break;

                case AX_ALPHA_MODE_MASK:
                    Engine->OpenGLAPI->EnableBlending(false);
                    Engine->OpenGLAPI->SetDepthWrite(true);
                    break;

                case AX_ALPHA_MODE_BLEND:
                    Engine->OpenGLAPI->EnableBlending(true);
                    Engine->OpenGLAPI->SetBlendFunction(AX_BLEND_SRC_ALPHA, AX_BLEND_ONE_MINUS_SRC_ALPHA);
                    Engine->OpenGLAPI->SetDepthWrite(false);
                    break;

                default:
                    Engine->OpenGLAPI->EnableBlending(false);
                    Engine->OpenGLAPI->SetDepthWrite(true);
                    break;
            }

            // Double-sided rendering
            if (PBR->DoubleSided) {
                Engine->OpenGLAPI->SetCullMode(false);  // Disable backface culling
            } else {
                Engine->OpenGLAPI->SetCullMode(true);   // Enable backface culling
            }

            // Render this mesh
            Engine->OpenGLAPI->Render((AxViewport*)Viewport, Mesh, ShaderData);

            // Restore defaults for next mesh
            Engine->OpenGLAPI->SetDepthWrite(true);
            Engine->OpenGLAPI->EnableBlending(false);
            Engine->OpenGLAPI->SetCullMode(true);  // Re-enable culling by default
        }
    }

    // Restore blending state
    Engine->OpenGLAPI->EnableBlending(false);
}

// Helper function to recursively compute node transforms with parent hierarchy
static void ComputeNodeTransform(
    cgltf_size NodeIndex,
    cgltf_data* Data,
    AxMat4x4* Transforms,
    bool* Processed
)
{
    if (NodeIndex >= Data->nodes_count || Processed[NodeIndex]) {
        return;
    }

    cgltf_node* Node = &Data->nodes[NodeIndex];
    AxMat4x4 LocalTransform;

    // Build local transform from node
    if (Node->has_matrix) {
        memcpy(&LocalTransform, Node->matrix, sizeof(AxMat4x4));
    } else {
        // Build from TRS components
        AxVec3 Translation = Node->has_translation ?
            (AxVec3){Node->translation[0], Node->translation[1], Node->translation[2]} :
            (AxVec3){0.0f, 0.0f, 0.0f};

        AxQuat Rotation = Node->has_rotation ?
            (AxQuat){Node->rotation[0], Node->rotation[1], Node->rotation[2], Node->rotation[3]} :
            QuatIdentity();

        AxVec3 Scale = Node->has_scale ?
            (AxVec3){Node->scale[0], Node->scale[1], Node->scale[2]} :
            Vec3One();

        // Build transform matrix: R * S, then apply translation
        AxMat4x4 R = QuatToMat4x4(Rotation);
        AxMat4x4 S = Identity();
        S.E[0][0] = Scale.X;
        S.E[1][1] = Scale.Y;
        S.E[2][2] = Scale.Z;

        AxMat4x4 RS = Mat4x4Mul(R, S);
        LocalTransform = RS;
        LocalTransform.E[3][0] = Translation.X;
        LocalTransform.E[3][1] = Translation.Y;
        LocalTransform.E[3][2] = Translation.Z;
        LocalTransform.E[3][3] = 1.0f;
    }

    // Find parent node and apply parent transform
    AxMat4x4 FinalTransform = LocalTransform;
    cgltf_size ParentIndex = (cgltf_size)-1;

    // Search for parent by checking which node has this as a child
    for (cgltf_size i = 0; i < Data->nodes_count; ++i) {
        cgltf_node* PotentialParent = &Data->nodes[i];
        for (cgltf_size childIdx = 0; childIdx < PotentialParent->children_count; ++childIdx) {
            if (PotentialParent->children[childIdx] >= Data->nodes &&
                PotentialParent->children[childIdx] < Data->nodes + Data->nodes_count) {
                cgltf_size ChildNodeIndex = PotentialParent->children[childIdx] - Data->nodes;
                if (ChildNodeIndex == NodeIndex) {
                    ParentIndex = i;
                    break;
                }
            }
        }
        if (ParentIndex != (cgltf_size)-1) break;
    }

    // Apply parent transform if exists and not circular reference
    if (ParentIndex != (cgltf_size)-1 && ParentIndex != NodeIndex) {
        ComputeNodeTransform(ParentIndex, Data, Transforms, Processed);
        FinalTransform = Mat4x4Mul(Transforms[ParentIndex], LocalTransform);
    }

    Transforms[NodeIndex] = FinalTransform;
    Processed[NodeIndex] = true;
}

static bool EngineLoadModel(AxEngineHandle* Engine, const char* Path, AxModel* Model)
{
    if (!Engine || !Engine->OpenGLAPI || !Path || !Model) {
        return (false);
    }

    // Initialize Model arrays to NULL
    memset(Model, 0, sizeof(AxModel));

    // Parse GLTF file
    cgltf_options Options = {0};
    cgltf_data* ModelData = NULL;
    cgltf_result Result = cgltf_parse_file(&Options, Path, &ModelData);

    if (Result != cgltf_result_success) {
        fprintf(stderr, "Failed to parse GLTF file: %s\n", Path);
        return (false);
    }

    Result = cgltf_load_buffers(&Options, ModelData, Path);
    if (Result != cgltf_result_success) {
        fprintf(stderr, "Failed to load GLTF buffers: %s\n", Path);
        cgltf_free(ModelData);
        return (false);
    }

    // Get base path for texture loading
    struct AxPlatformAPI* PlatformAPI = Engine->Registry->Get(AXON_PLATFORM_API_NAME);
    if (!PlatformAPI || !PlatformAPI->PathAPI) {
        fprintf(stderr, "PlatformAPI not available for path operations\n");
        cgltf_free(ModelData);
        return (false);
    }

    const char* BasePath = PlatformAPI->PathAPI->BasePath(Path);

    // Texture sentinel for "no texture"
    const uint32_t kNoTexture = 0xFFFFFFFFu;

    // Process all materials
    for (cgltf_size i = 0; i < ModelData->materials_count; ++i) {
        cgltf_material* GltfMaterial = &ModelData->materials[i];
        cgltf_pbr_metallic_roughness* pbr = &GltfMaterial->pbr_metallic_roughness;

        // Create legacy AxMaterial
        AxMaterial AxMat = {0};
        AxMat.BaseColorFactor[0] = pbr->base_color_factor[0];
        AxMat.BaseColorFactor[1] = pbr->base_color_factor[1];
        AxMat.BaseColorFactor[2] = pbr->base_color_factor[2];
        AxMat.BaseColorFactor[3] = pbr->base_color_factor[3];
        snprintf(AxMat.Name, sizeof(AxMat.Name), "%s", GltfMaterial->name ? GltfMaterial->name : "Unnamed");

        // Read alpha mode
        switch (GltfMaterial->alpha_mode) {
            case cgltf_alpha_mode_opaque:
                AxMat.AlphaMode = AX_ALPHA_MODE_OPAQUE;
                break;
            case cgltf_alpha_mode_mask:
                AxMat.AlphaMode = AX_ALPHA_MODE_MASK;
                break;
            case cgltf_alpha_mode_blend:
                AxMat.AlphaMode = AX_ALPHA_MODE_BLEND;
                break;
            default:
                AxMat.AlphaMode = AX_ALPHA_MODE_OPAQUE;
                break;
        }
        AxMat.AlphaCutoff = (GltfMaterial->alpha_cutoff > 0.0f) ? GltfMaterial->alpha_cutoff : 0.5f;

        // Load Base Color Texture (sRGB)
        if (pbr->base_color_texture.texture) {
            AxTexture BaseTexture = {0};
            if (LoadModelTexture(Engine->OpenGLAPI, &BaseTexture, &pbr->base_color_texture, ModelData, BasePath, true)) {
                ArrayPush(Model->Textures, BaseTexture);
                AxMat.BaseColorTexture = ArraySize(Model->Textures) - 1;
            } else {
                AxMat.BaseColorTexture = kNoTexture;
            }
        } else {
            AxMat.BaseColorTexture = kNoTexture;
        }

        // Load Normal Texture (linear)
        if (GltfMaterial->normal_texture.texture) {
            AxTexture NormalTexture = {0};
            if (LoadModelTexture(Engine->OpenGLAPI, &NormalTexture, (cgltf_texture_view*)&GltfMaterial->normal_texture, ModelData, BasePath, false)) {
                ArrayPush(Model->Textures, NormalTexture);
                AxMat.NormalTexture = ArraySize(Model->Textures) - 1;
            } else {
                AxMat.NormalTexture = kNoTexture;
            }
        } else {
            AxMat.NormalTexture = kNoTexture;
        }

        // Load Metallic-Roughness Texture (linear)
        if (pbr->metallic_roughness_texture.texture) {
            AxTexture MetallicRoughnessTexture = {0};
            if (LoadModelTexture(Engine->OpenGLAPI, &MetallicRoughnessTexture, &pbr->metallic_roughness_texture, ModelData, BasePath, false)) {
                ArrayPush(Model->Textures, MetallicRoughnessTexture);
                AxMat.MetallicRoughnessTexture = ArraySize(Model->Textures) - 1;
            } else {
                AxMat.MetallicRoughnessTexture = kNoTexture;
            }
        } else {
            AxMat.MetallicRoughnessTexture = kNoTexture;
        }

        ArrayPush(Model->Materials, AxMat);

        // Create AxMaterialDesc
        AxMaterialDesc MatDesc = {0};
        snprintf(MatDesc.Name, sizeof(MatDesc.Name), "%s", GltfMaterial->name ? GltfMaterial->name : "Unnamed");
        MatDesc.Type = AX_MATERIAL_TYPE_PBR;

        MatDesc.PBR.BaseColorFactor.X = pbr->base_color_factor[0];
        MatDesc.PBR.BaseColorFactor.Y = pbr->base_color_factor[1];
        MatDesc.PBR.BaseColorFactor.Z = pbr->base_color_factor[2];
        MatDesc.PBR.BaseColorFactor.W = pbr->base_color_factor[3];

        MatDesc.PBR.EmissiveFactor.X = GltfMaterial->emissive_factor[0];
        MatDesc.PBR.EmissiveFactor.Y = GltfMaterial->emissive_factor[1];
        MatDesc.PBR.EmissiveFactor.Z = GltfMaterial->emissive_factor[2];

        MatDesc.PBR.MetallicFactor = pbr->metallic_factor;
        MatDesc.PBR.RoughnessFactor = pbr->roughness_factor;

        MatDesc.PBR.BaseColorTexture = (AxMat.BaseColorTexture == kNoTexture) ? -1 : (int32_t)AxMat.BaseColorTexture;
        MatDesc.PBR.NormalTexture = (AxMat.NormalTexture == kNoTexture) ? -1 : (int32_t)AxMat.NormalTexture;
        MatDesc.PBR.MetallicRoughnessTexture = (AxMat.MetallicRoughnessTexture == kNoTexture) ? -1 : (int32_t)AxMat.MetallicRoughnessTexture;

        // Load emissive texture if present
        if (GltfMaterial->emissive_texture.texture) {
            AxTexture EmissiveTexture = {0};
            if (LoadModelTexture(Engine->OpenGLAPI, &EmissiveTexture, (cgltf_texture_view*)&GltfMaterial->emissive_texture, ModelData, BasePath, true)) {
                ArrayPush(Model->Textures, EmissiveTexture);
                MatDesc.PBR.EmissiveTexture = ArraySize(Model->Textures) - 1;
            } else {
                MatDesc.PBR.EmissiveTexture = -1;
            }
        } else {
            MatDesc.PBR.EmissiveTexture = -1;
        }

        // Load occlusion texture if present
        if (GltfMaterial->occlusion_texture.texture) {
            AxTexture OcclusionTexture = {0};
            if (LoadModelTexture(Engine->OpenGLAPI, &OcclusionTexture, (cgltf_texture_view*)&GltfMaterial->occlusion_texture, ModelData, BasePath, false)) {
                ArrayPush(Model->Textures, OcclusionTexture);
                MatDesc.PBR.OcclusionTexture = ArraySize(Model->Textures) - 1;
            } else {
                MatDesc.PBR.OcclusionTexture = -1;
            }
        } else {
            MatDesc.PBR.OcclusionTexture = -1;
        }

        MatDesc.PBR.AlphaMode = AxMat.AlphaMode;
        MatDesc.PBR.AlphaCutoff = AxMat.AlphaCutoff;
        MatDesc.PBR.DoubleSided = GltfMaterial->double_sided;

        ArrayPush(Model->MaterialDescs, MatDesc);
    }

    // Build node transforms with hierarchy (matching Game.cpp logic)
    // Allocate arrays for node transform computation
    AxMat4x4* NodeTransforms = (AxMat4x4*)malloc(sizeof(AxMat4x4) * ModelData->nodes_count);
    bool* NodeProcessed = (bool*)calloc(ModelData->nodes_count, sizeof(bool));

    if (!NodeTransforms || !NodeProcessed) {
        fprintf(stderr, "Failed to allocate node transform arrays\n");
        cgltf_free(ModelData);
        return (false);
    }

    // Compute transforms for all nodes
    for (cgltf_size i = 0; i < ModelData->nodes_count; ++i) {
        ComputeNodeTransform(i, ModelData, NodeTransforms, NodeProcessed);
        ArrayPush(Model->Transforms, NodeTransforms[i]);
    }

    // Now process meshes - this is where the complexity lies
    // For simplicity, I'll load all meshes as separate mesh objects
    for (cgltf_size meshIdx = 0; meshIdx < ModelData->meshes_count; ++meshIdx) {
        cgltf_mesh* GltfMesh = &ModelData->meshes[meshIdx];

        for (cgltf_size primIdx = 0; primIdx < GltfMesh->primitives_count; ++primIdx) {
            cgltf_primitive* Primitive = &GltfMesh->primitives[primIdx];

            // Get texture transform from material (KHR_texture_transform)
            const cgltf_texture_view* BaseView = NULL;
            if (Primitive->material && Primitive->material->has_pbr_metallic_roughness) {
                BaseView = &Primitive->material->pbr_metallic_roughness.base_color_texture;
            }

            // Extract vertex data
            size_t VertexCount = 0;
            float* PositionData = NULL;
            float* NormalData = NULL;
            float* TexCoordData0 = NULL;
            float* TexCoordData1 = NULL;
            float* TangentData = NULL;

            // Find attributes
            for (cgltf_size attrIdx = 0; attrIdx < Primitive->attributes_count; ++attrIdx) {
                cgltf_attribute* Attr = &Primitive->attributes[attrIdx];

                if (Attr->type == cgltf_attribute_type_position) {
                    LoadFloatAttribute(Attr->data, &PositionData, &VertexCount, 3);
                } else if (Attr->type == cgltf_attribute_type_normal) {
                    size_t Count;
                    LoadFloatAttribute(Attr->data, &NormalData, &Count, 3);
                } else if (Attr->type == cgltf_attribute_type_texcoord) {
                    size_t Count;
                    // Check which TEXCOORD set this is (0, 1, etc.)
                    if (Attr->index == 0) {
                        LoadFloatAttribute(Attr->data, &TexCoordData0, &Count, 2);
                    } else if (Attr->index == 1) {
                        LoadFloatAttribute(Attr->data, &TexCoordData1, &Count, 2);
                    }
                } else if (Attr->type == cgltf_attribute_type_tangent) {
                    size_t Count;
                    LoadFloatAttribute(Attr->data, &TangentData, &Count, 4);
                }
            }

            // Determine which texcoord set to use
            int TexcoordSetToUse = 0;
            if (BaseView && BaseView->texture) {
                TexcoordSetToUse = BaseView->texcoord;

                // KHR_texture_transform can override the UV set
                if (BaseView->has_transform && BaseView->transform.has_texcoord) {
                    TexcoordSetToUse = BaseView->transform.texcoord;
                }
            }

            // Select the appropriate texture coordinate data
            float* TexCoordData = NULL;
            if (TexcoordSetToUse == 0 && TexCoordData0) {
                TexCoordData = TexCoordData0;
            } else if (TexcoordSetToUse == 1 && TexCoordData1) {
                TexCoordData = TexCoordData1;
            } else {
                TexCoordData = TexCoordData0;  // Fallback
            }

            if (!PositionData || VertexCount == 0) {
                free(PositionData);
                free(NormalData);
                free(TexCoordData0);
                free(TexCoordData1);
                free(TangentData);
                continue;
            }

            // Build vertex array
            AxVertex* Vertices = (AxVertex*)malloc(sizeof(AxVertex) * VertexCount);
            if (!Vertices) {
                free(PositionData);
                free(NormalData);
                free(TexCoordData0);
                free(TexCoordData1);
                free(TangentData);
                continue;
            }

            for (size_t v = 0; v < VertexCount; ++v) {
                AxVertex* Vert = &Vertices[v];

                Vert->Position.X = PositionData[v * 3 + 0];
                Vert->Position.Y = PositionData[v * 3 + 1];
                Vert->Position.Z = PositionData[v * 3 + 2];

                if (NormalData) {
                    Vert->Normal.X = NormalData[v * 3 + 0];
                    Vert->Normal.Y = NormalData[v * 3 + 1];
                    Vert->Normal.Z = NormalData[v * 3 + 2];
                } else {
                    Vert->Normal = (AxVec3){0, 1, 0};
                }

                // TexCoord (if present) - apply KHR_texture_transform
                if (TexCoordData) {
                    AxVec2 Uv = {TexCoordData[v * 2 + 0], TexCoordData[v * 2 + 1]};

                    if (BaseView && BaseView->has_transform) {
                        const cgltf_texture_transform* Tt = &BaseView->transform;

                        // Use glTF defaults: offset=[0,0], scale=[1,1], rotation=0
                        float OffX = Tt->offset[0];
                        float OffY = Tt->offset[1];
                        float SclX = (Tt->scale[0] > 0.0f) ? Tt->scale[0] : 1.0f;
                        float SclY = (Tt->scale[1] > 0.0f) ? Tt->scale[1] : 1.0f;
                        float Rot = Tt->rotation; // radians

                        // SCALE -> ROTATE -> OFFSET (per KHR_texture_transform spec)
                        // This order is critical for non-uniform atlas scales
                        float Us = Uv.X * SclX;
                        float Vs = Uv.Y * SclY;

                        float C = Cos(Rot), S = Sin(Rot);
                        float Ur = C * Us - S * Vs;
                        float Vr = S * Us + C * Vs;

                        Uv.X = OffX + Ur;
                        Uv.Y = OffY + Vr;
                    }

                    Vert->TexCoord = Uv;
                } else {
                    Vert->TexCoord = (AxVec2){0, 0};
                }

                // Initialize tangent vector (XYZ = tangent, W = handedness)
                Vert->Tangent = (AxVec4){0, 0, 0, 1};
            }

            free(PositionData);
            free(NormalData);
            free(TexCoordData0);
            free(TexCoordData1);
            free(TangentData);

            // Extract indices
            uint32_t* Indices = NULL;
            size_t IndexCount = 0;

            if (Primitive->indices) {
                IndexCount = Primitive->indices->count;
                Indices = (uint32_t*)malloc(sizeof(uint32_t) * IndexCount);
                if (Indices) {
                    for (cgltf_size i = 0; i < IndexCount; ++i) {
                        Indices[i] = (uint32_t)cgltf_accessor_read_index(Primitive->indices, i);
                    }
                }
            } else {
                // If no indices, create sequential indices
                IndexCount = VertexCount;
                Indices = (uint32_t*)malloc(sizeof(uint32_t) * IndexCount);
                if (Indices) {
                    for (size_t i = 0; i < IndexCount; ++i) {
                        Indices[i] = (uint32_t)i;
                    }
                }
            }

            // Calculate tangent and bitangent vectors for normal mapping
            if (!TangentData && TexCoordData && IndexCount >= 3) {
                // Process triangles to calculate tangent space
                for (size_t i = 0; i < IndexCount; i += 3) {
                    uint32_t i0 = Indices[i];
                    uint32_t i1 = Indices[i + 1];
                    uint32_t i2 = Indices[i + 2];

                    if (i0 < VertexCount && i1 < VertexCount && i2 < VertexCount) {
                        AxVec3 Tangent, Bitangent;
                        CalculateTangentBitangent(
                            Vertices[i0].Position, Vertices[i1].Position, Vertices[i2].Position,
                            Vertices[i0].TexCoord, Vertices[i1].TexCoord, Vertices[i2].TexCoord,
                            &Tangent, &Bitangent
                        );

                        // Add tangent to all vertices of this triangle
                        // (We'll average them later for smooth results)
                        AxVec3 CurrentTangent0 = {Vertices[i0].Tangent.X, Vertices[i0].Tangent.Y, Vertices[i0].Tangent.Z};
                        AxVec3 CurrentTangent1 = {Vertices[i1].Tangent.X, Vertices[i1].Tangent.Y, Vertices[i1].Tangent.Z};
                        AxVec3 CurrentTangent2 = {Vertices[i2].Tangent.X, Vertices[i2].Tangent.Y, Vertices[i2].Tangent.Z};

                        AxVec3 NewTangent0 = Vec3Add(CurrentTangent0, Tangent);
                        AxVec3 NewTangent1 = Vec3Add(CurrentTangent1, Tangent);
                        AxVec3 NewTangent2 = Vec3Add(CurrentTangent2, Tangent);

                        Vertices[i0].Tangent.X = NewTangent0.X;
                        Vertices[i0].Tangent.Y = NewTangent0.Y;
                        Vertices[i0].Tangent.Z = NewTangent0.Z;

                        Vertices[i1].Tangent.X = NewTangent1.X;
                        Vertices[i1].Tangent.Y = NewTangent1.Y;
                        Vertices[i1].Tangent.Z = NewTangent1.Z;

                        Vertices[i2].Tangent.X = NewTangent2.X;
                        Vertices[i2].Tangent.Y = NewTangent2.Y;
                        Vertices[i2].Tangent.Z = NewTangent2.Z;
                    }
                }

                // Normalize tangent vectors and calculate handedness
                for (size_t i = 0; i < VertexCount; ++i) {
                    AxVec3 Tangent = {Vertices[i].Tangent.X, Vertices[i].Tangent.Y, Vertices[i].Tangent.Z};
                    AxVec3 Normal = Vertices[i].Normal;

                    if (Vec3Length(Tangent) > 0.0f) {
                        // Normalize the tangent
                        Tangent = Vec3Normalize(Tangent);

                        // Store normalized tangent with handedness
                        // (handedness is already initialized to 1.0)
                        Vertices[i].Tangent.X = Tangent.X;
                        Vertices[i].Tangent.Y = Tangent.Y;
                        Vertices[i].Tangent.Z = Tangent.Z;
                    }
                }
            } else if (TangentData) {
                // Use tangents from GLTF file
                for (size_t v = 0; v < VertexCount; ++v) {
                    Vertices[v].Tangent.X = TangentData[v * 4 + 0];
                    Vertices[v].Tangent.Y = TangentData[v * 4 + 1];
                    Vertices[v].Tangent.Z = TangentData[v * 4 + 2];
                    Vertices[v].Tangent.W = TangentData[v * 4 + 3];
                }
            }

            // Create mesh structure
            AxMesh Mesh = {0};
            Engine->OpenGLAPI->InitMesh(&Mesh, Vertices, Indices, (uint32_t)VertexCount, (uint32_t)IndexCount);
            snprintf(Mesh.Name, sizeof(Mesh.Name), "%s", GltfMesh->name ? GltfMesh->name : "Mesh");

            // Assign material index
            if (Primitive->material) {
                cgltf_size MaterialIndex = Primitive->material - ModelData->materials;
                Mesh.MaterialIndex = (uint32_t)MaterialIndex;
            } else {
                Mesh.MaterialIndex = 0;
            }

            // Find which node references this mesh for transform
            bool FoundNode = false;
            for (cgltf_size NodeIndex = 0; NodeIndex < ModelData->nodes_count; ++NodeIndex) {
                cgltf_node* Node = &ModelData->nodes[NodeIndex];
                if (Node->mesh && Node->mesh == GltfMesh) {
                    Mesh.TransformIndex = (uint32_t)NodeIndex;
                    FoundNode = true;
                    break;
                }
            }

            // If no node references this mesh, add identity transform
            if (!FoundNode) {
                AxMat4x4 IdentityTransform = Identity();
                ArrayPush(Model->Transforms, IdentityTransform);
                Mesh.TransformIndex = ArraySize(Model->Transforms) - 1;
            }

            ArrayPush(Model->Meshes, Mesh);

            free(Vertices);
            free(Indices);
        }
    }

    // Cleanup temporary arrays
    free(NodeTransforms);
    free(NodeProcessed);

    cgltf_free(ModelData);

    printf("Loaded model: %zu meshes, %zu materials, %zu textures, %zu transforms\n",
           ArraySize(Model->Meshes), ArraySize(Model->Materials), ArraySize(Model->Textures), ArraySize(Model->Transforms));

    return (true);
}

///////////////////////////////////////////////////////////////
// API Structure and Plugin Interface
///////////////////////////////////////////////////////////////

static struct AxEngineAPI *EngineAPI = &(struct AxEngineAPI) {
    .Create = EngineCreate,
    .Initialize = EngineInitialize,
    .Update = EngineUpdate,
    .Destroy = EngineDestroy,
    .LoadScene = EngineLoadScene,
    .GetScene = EngineGetScene,
    .GetSceneCamera = EngineGetSceneCamera,
    .UpdateSystems = EngineUpdateSystems,
    .LoadTexture = EngineLoadTexture,
    .LoadMesh = EngineLoadMesh,
    .LoadModel = EngineLoadModel,
    .LoadShader = EngineLoadShader,
    .RenderScene = EngineRenderScene
};

AXON_DLL_EXPORT void LoadPlugin(struct AxAPIRegistry* APIRegistry)
{
    if (APIRegistry) {
        APIRegistry->Set(AX_ENGINE_API_NAME, EngineAPI, sizeof(struct AxEngineAPI));
    }
}

AXON_DLL_EXPORT void UnloadPlugin(struct AxAPIRegistry* APIRegistry)
{
    if (APIRegistry) {
        APIRegistry->Set(AX_ENGINE_API_NAME, NULL, 0);
    }
}
