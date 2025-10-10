#include "Game.h"

#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxPlatform.h"
#include "Foundation/AxPlugin.h"
#include "Foundation/AxHashTable.h"
#include "Foundation/AxMath.h"

#define AXARRAY_IMPLEMENTATION
#include "Foundation/AxArray.h"

#include "AxWindow/AxWindow.h"
#include "AxOpenGL/AxOpenGL.h"
#include "AxScene/AxScene.h"
#include "AxResource/AxResource.h"
#include "AxResource/AxShaderManager.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include <stdio.h>
#include <unordered_map>
#include <string>
#include <string.h>
#include <vector>
#include <cmath>
#include <functional>

// Texture sentinel for "no texture" case
constexpr uint32_t kNoTexture = 0xFFFFFFFFu;

// Fallback checker texture for missing textures (diagnostic)
static AxTexture gCheckerTexture = {};
static bool gCheckerTextureInitialized = false;

// Camera controls state
static float CameraSpeed = 5.0f;
static float MouseSensitivity = 0.001f;
static AxVec2 LastMousePos = { 0.0f, 0.0f };

static std::unordered_map<std::string, AxModel> Models;

// Scene Event Adapter for automatic scene construction
static Game* GameInstance = nullptr;  // Global reference for callbacks

// SceneEvents callback functions
static void OnLightParsed(const AxLight* Light, void* UserData)
{
    Game* GameRef = static_cast<Game*>(UserData);
    printf("SceneAdapter: Light parsed - %s (Type: %d, Intensity: %.2f)\n",
           Light->Name, Light->Type, Light->Intensity);

    // Lights are automatically added to the scene by SceneAPI
    // We just log their presence for debugging
}

static void OnMaterialParsed(const AxMaterial* Material, void* UserData)
{
    Game* GameRef = static_cast<Game*>(UserData);
    printf("SceneAdapter: Material parsed - %s (Vertex: %s, Fragment: %s)\n",
           Material->Name, Material->VertexShaderPath, Material->FragmentShaderPath);

    // Load and compile shaders immediately when material is parsed
    AxShaderHandle ShaderHandle = GameRef->ConstructShader(Material->VertexShaderPath, Material->FragmentShaderPath);
    if (!GameRef->ShaderManagerAPI_->IsValid(ShaderHandle)) {
        fprintf(stderr, "SceneAdapter: Failed to load shaders for material '%s'\n", Material->Name);
        return;
    }

    // Store compiled shader handle for later assignment
    // We can't modify the scene materials during parsing, so store temporarily
    GameRef->CompiledShaders[std::string(Material->Name)] = ShaderHandle;
    printf("SceneAdapter: Shaders compiled and stored for material '%s'\n", Material->Name);
}

static void OnObjectParsed(const AxSceneObject* Object, void* UserData)
{
    Game* GameRef = static_cast<Game*>(UserData);
    printf("SceneAdapter: Object parsed - %s (Mesh: %s, Material: %s)\n",
           Object->Name, Object->MeshPath, Object->MaterialPath);

    // Ensure the object's material is compiled (if it references one)
    if (Object->MaterialPath[0] != '\0') {
        std::string materialName(Object->MaterialPath);
        if (GameRef->CompiledShaders.find(materialName) == GameRef->CompiledShaders.end()) {
            printf("SceneAdapter: Warning - Object '%s' references material '%s' that hasn't been parsed yet\n",
                   Object->Name, Object->MaterialPath);
            // Material should have been parsed before object, but scenes can have different orders
            // For now, we'll trust that it will be compiled soon and assigned during post-processing
        }
    }

    // Automatically load models for objects with mesh paths
    if (Object->MeshPath[0] != '\0') {
        GameRef->LoadObjectModel(Object);
    }
}

static void OnTransformParsed(const AxTransform* Transform, void* UserData)
{
    Game* GameRef = static_cast<Game*>(UserData);
    printf("SceneAdapter: Transform parsed - Position(%.2f, %.2f, %.2f)\n",
           Transform->Translation.X, Transform->Translation.Y, Transform->Translation.Z);

    // Transforms are automatically handled by the scene object hierarchy
}

static void OnSceneParsed(const AxScene* Scene, void* UserData)
{
    Game* GameRef = static_cast<Game*>(UserData);
    printf("SceneAdapter: Scene parsing complete - %s (%d objects, %d materials, %d lights)\n",
           Scene->Name, Scene->ObjectCount, Scene->MaterialCount, Scene->LightCount);

    // All scene elements have been parsed, now assign compiled shaders to scene materials
    GameRef->AssignCompiledShadersToScene(Scene);

    // Find and set camera from scene
    GameRef->SetupSceneCamera(Scene);
}

// Static callback functions
static void KeyCallback(AxWindow *Window, int Key, int ScanCode, int Action, int Mods, void *UserData)
{
    Game* App = static_cast<Game*>(UserData);

    // Game now has direct access to WindowAPI, no need to query registry
    AxWindowAPI *WindowAPI = App->WindowAPI_;


    if (Key == AX_KEY_ESCAPE && Action == AX_PRESS) {
        App->IsRequestingExit(true);
    }

    if (Action == AX_PRESS) {
        if (Key == AX_KEY_F1) {
            // Show a message box
            WindowAPI->CreateMessageBox(
                Window, "Help", "This is a demo of the message box feature!",
                (enum AxMessageBoxFlags)(AX_MESSAGEBOX_TYPE_OK | AX_MESSAGEBOX_ICON_INFORMATION)
            );
        }

        if (Key == AX_KEY_F2) {
            // Open a file dialog
            AxFileDialogResult Result = WindowAPI->OpenFileDialog(
                Window, "Open Model", "GLTF Files (*.gltf;*.glb)\0*.gltf;*.glb\0All Files (*.*)\0*.*\0",
                nullptr
            );
            if (Result.Success) {
                printf("Selected file: %s\n", Result.FilePath);
            } else {
                printf("File dialog canceled or failed\n");
            }
        }
    }
}

static void MousePosCallback(AxWindow *Window, double X, double Y, void *UserData)
{
    Game* App = static_cast<Game*>(UserData);

    AxVec2 MouseDelta = {
        (float)X - LastMousePos.X,
        (float)Y - LastMousePos.Y
    };

    AxTransform *Transform = App->GetTransform();
    TransformRotateFromMouseDelta(Transform, MouseDelta, MouseSensitivity);
    LastMousePos = { (float)X, (float)Y };
}

static void MouseButtonCallback(AxWindow *Window, int Button, int Action, int Mods, void *UserData)
{
    if (Action == AX_PRESS) {
        printf("Mouse button pressed: %d\n", Button);
    }
}

static void ScrollCallback(AxWindow *Window, AxVec2 Offset, void *UserData)
{
    printf("Scroll: %.2f, %.2f\n", Offset.X, Offset.Y);
}

static void StateChangedCallback(AxWindow *Window, enum AxWindowState OldState, enum AxWindowState NewState, void *UserData)
{
    const char* OldStateStr = "Unknown";
    const char* NewStateStr = "Unknown";

    switch (OldState) {
        case AX_WINDOW_STATE_NORMAL: OldStateStr = "Normal"; break;
        case AX_WINDOW_STATE_MINIMIZED: OldStateStr = "Minimized"; break;
        case AX_WINDOW_STATE_MAXIMIZED: OldStateStr = "Maximized"; break;
        case AX_WINDOW_STATE_FULLSCREEN: OldStateStr = "Fullscreen"; break;
    }

    switch (NewState) {
        case AX_WINDOW_STATE_NORMAL: NewStateStr = "Normal"; break;
        case AX_WINDOW_STATE_MINIMIZED: NewStateStr = "Minimized"; break;
        case AX_WINDOW_STATE_MAXIMIZED: NewStateStr = "Maximized"; break;
        case AX_WINDOW_STATE_FULLSCREEN: NewStateStr = "Fullscreen"; break;
    }

    printf("Window state changed from %s to %s\n", OldStateStr, NewStateStr);
}

static AxTextureWrapMode ConvertGLTFWrapMode(int32_t gltfWrapMode) {
    switch (gltfWrapMode) {
        case 33071: return AX_TEXTURE_WRAP_CLAMP_TO_EDGE;
        case 33648: return AX_TEXTURE_WRAP_MIRRORED_REPEAT;
        case 10497: return AX_TEXTURE_WRAP_REPEAT;
        default: return AX_TEXTURE_WRAP_REPEAT;
    }
}

static AxTextureFilter ConvertGLTFFilter(int32_t gltfFilter) {
    switch (gltfFilter) {
        case 9728: return AX_TEXTURE_FILTER_NEAREST;
        case 9729: return AX_TEXTURE_FILTER_LINEAR;
        case 9984: return AX_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST;
        case 9985: return AX_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST;
        case 9986: return AX_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR;
        case 9987: return AX_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR;
        default: return AX_TEXTURE_FILTER_LINEAR;
    }
}

void LoadFloatAttribute(cgltf_accessor* Accessor, std::vector<float>& AttributeData, const size_t Components) {
    size_t TotalElements = Accessor->count * Components;
    AttributeData.clear();
    AttributeData.reserve(TotalElements);

    for (cgltf_size i = 0; i < Accessor->count; ++i)
    {
        float Values[4] = {0};
        cgltf_accessor_read_float(Accessor, i, Values, Components);

        // Push each component of the attribute into the vector
        for (size_t j = 0; j < Components; ++j) {
            AttributeData.push_back(Values[j]);
        }
    }
}

bool Game::LoadModelTexture(AxTexture *Texture, cgltf_texture_view *TextureView, cgltf_data *ModelData, const char *BasePath, bool IsSRGB)
{
    if (!TextureView->texture) {
        printf("Texture view has no texture pointer!");
        return (false);
    }

    printf("Loading texture: ");

    // Handle KHR_texture_basisu (KTX2) explicitly
    if (!TextureView->texture->image && TextureView->texture->basisu_image) {
        auto* Img = TextureView->texture->basisu_image;
        const char* Name = Img->name ? Img->name : (Img->uri ? Img->uri : "(unnamed)");
        fprintf(stderr,
            "WARNING: Unsupported KTX2/BasisU image '%s' — texture will be missing.\n"
            "         Add a KTX2 loader or re-export as PNG/JPG.\n", Name);
        return (false); // keep sentinel so shader uses the factor
    }

    if (!TextureView->texture->image) {
        printf("Texture has no image data\n");
        return (false);
    }

    cgltf_image *Image = TextureView->texture->image;
    if (Image->uri) {
        printf("URI: %s\n", Image->uri);
    } else if (Image->buffer_view) {
        printf("Embedded buffer (size: %zu)\n", Image->buffer_view->size);
    } else {
        printf("❌ No URI or buffer view\n");
        return false;
    }

    cgltf_sampler *Sampler = TextureView->texture->sampler;

    stbi_uc *Pixels = nullptr;
    int32_t Width, Height, Channels;
    if (Image->uri)
    {
        char ImagePath[1024];
        snprintf(ImagePath, sizeof(ImagePath), "%s%s", BasePath, Image->uri);

        Pixels = stbi_load(ImagePath, &Width, &Height, &Channels, 0);
        if (!Pixels) {
            fprintf(stderr, "Error: Failed to load image '%s'\n", ImagePath);
            return (false);
        }
    } else if (Image->buffer_view) {
        cgltf_buffer_view* BufferView = Image->buffer_view;
        cgltf_buffer* Buffer = BufferView->buffer;

        // Pointer to the image data within the buffer
        const stbi_uc* ImagePtr = (const stbi_uc*)Buffer->data + BufferView->offset;

        Pixels = stbi_load_from_memory(ImagePtr, (int32_t)BufferView->size, &Width, &Height, &Channels, 0);
        if (!Pixels) {
            const char* StbiError = stbi_failure_reason();
            fprintf(stderr, "Error: Failed to load embedded image: %s (size: %zu bytes)\n",
                    StbiError ? StbiError : "unknown error", BufferView->size);
            return (false);
        }
    } else {
        fprintf(stderr, "Warning: Image '%s' has neither URI nor buffer view.\n", Image->name ? Image->name : "Unnamed");
        return (false);
    }

    Texture->Width = Width;
    Texture->Height = Height;
    Texture->Channels = Channels;
    
    // Diffuse/albedo textures should be sRGB, everything else should be linear
    if (IsSRGB) {
        printf("  Marking as sRGB (diffuse/albedo texture)\n");
    } else {
        printf("  Marking as linear (non-color data)\n");
    }
    Texture->IsSRGB = IsSRGB;

    RenderAPI_->InitTexture(Texture, Pixels);
    // Bind to texture unit 0 for setting sampler properties
    RenderAPI_->BindTexture(Texture, 0);

    SetSamplerPropertiesFromGLTF(Texture, Sampler);
    stbi_image_free(Pixels);

    return (true);
}

void Game::EnsureCheckerTexture() {
    if (gCheckerTextureInitialized) {
        return;
    }

    // Create a 2x2 magenta/black checker pattern for missing textures
    unsigned char pixels[16] = {
        255, 0, 255, 255,    0, 0, 0, 255,
        0, 0, 0, 255,        255, 0, 255, 255
    };

    gCheckerTexture.Width = 2;
    gCheckerTexture.Height = 2;
    gCheckerTexture.Channels = 4;
    gCheckerTexture.IsSRGB = true;

    RenderAPI_->InitTexture(&gCheckerTexture, pixels);
    RenderAPI_->BindTexture(&gCheckerTexture, 0);
    SetSamplerPropertiesFromGLTF(&gCheckerTexture, nullptr);

    gCheckerTextureInitialized = true;
    printf("Initialized fallback checker texture for missing textures\n");
}

void Game::SetSamplerPropertiesFromGLTF(AxTexture* Texture, cgltf_sampler* Sampler) {
    // Default filtering: LINEAR with mipmaps for minification
    AxTextureWrapMode WrapS = AX_TEXTURE_WRAP_REPEAT;
    AxTextureWrapMode WrapT = AX_TEXTURE_WRAP_REPEAT;
    AxTextureFilter MagFilter = AX_TEXTURE_FILTER_LINEAR;
    AxTextureFilter MinFilter = AX_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR;  // Use mipmaps by default

    if (Sampler) {
        // Convert GLTF sampler values to AxOpenGL enums
        WrapS = ConvertGLTFWrapMode(Sampler->wrap_s);
        WrapT = ConvertGLTFWrapMode(Sampler->wrap_t);
        MagFilter = ConvertGLTFFilter(Sampler->mag_filter);
        MinFilter = ConvertGLTFFilter(Sampler->min_filter);
    } else {
        printf("No sampler specified, using defaults (LINEAR with mipmaps)\n");
    }

    // Use the proper AxOpenGL API to set sampler properties
    RenderAPI_->SetTextureWrapMode(Texture, WrapS, WrapT);
    RenderAPI_->SetTextureFilterMode(Texture, MagFilter, MinFilter);
}

void Game::RegisterCallbacks()
{
    // Use the new callback system with error handling
    AxWindowCallbacks Callbacks = {0};
    enum AxWindowError Error = AX_WINDOW_ERROR_NONE;

    // Set up all callbacks at once
    Callbacks.Key = KeyCallback;
    Callbacks.MousePos = MousePosCallback;
    Callbacks.MouseButton = MouseButtonCallback;
    Callbacks.Scroll = ScrollCallback;
    Callbacks.StateChanged = StateChangedCallback;

    // Set all callbacks at once with user data
    if (!WindowAPI_->SetCallbacks(Window_, &Callbacks, this, &Error)) {
        fprintf(stderr, "Failed to set callbacks: %s\n", WindowAPI_->GetErrorString(Error));
    }
}

bool Game::Create()
{
    // Create scene first, then load shaders from materials
    if (!CreateScene()) {
        fprintf(stderr, "Failed to create scene!\n");
        return false;
    }

    printf("Game: Scene created successfully\n");
    return true;
}

bool Game::Tick(float DeltaT)
{
    // Update camera with input
    UpdateCamera(DeltaT);

    // Debug camera position on first frame only
    static bool DebuggedCamera = false;
    if (!DebuggedCamera) {
        DebuggedCamera = true;
        printf("Camera: Position(%.2f, %.2f, %.2f), FOV=%.2f°\n",
               Transform_->Translation.X, Transform_->Translation.Y, Transform_->Translation.Z,
               Camera_->FieldOfView * 180.0f / 3.14159f);
    }

    static int FrameCount = 0;
    FrameCount++;

    for (auto& [ModelName, Model] : Models) {
        if (ArraySize(Model.Meshes) > 0) {
            if (FrameCount <= 5) {
                printf("Rendering model '%s' (frame %d)\n", ModelName.c_str(), FrameCount);
            }
            RenderModelWithMeshes(&Model, Camera_, Transform_, Viewport_);
        }
    }

    return true;
}

void Game::Destroy()
{
    // Unregister scene event handler
    UnregisterSceneEventHandler();

    // Clean up scene
    if (Scene_) {
        SceneAPI_->DestroyScene(Scene_);
        Scene_ = nullptr;
    }

    // Shutdown resource managers (automatically cleans up all shaders)
    if (ResourceAPI_) {
        ResourceAPI_->Shutdown();
    }

    // Clean up camera
    if (Camera_) {
        delete Camera_;
        Camera_ = nullptr;
    }
}

void Game::Initialize(AxAPIRegistry *APIRegistry, AxWindow *Window, const AxViewport *Viewport)
{
    // Store provided infrastructure
    APIRegistry_ = APIRegistry;
    Window_ = Window;
    Viewport_ = Viewport;

    // Get APIs from the registry
    WindowAPI_ = (struct AxWindowAPI *)APIRegistry_->Get(AXON_WINDOW_API_NAME);
    RenderAPI_ = (struct AxOpenGLAPI *)APIRegistry_->Get(AXON_OPENGL_API_NAME);
    PlatformAPI_ = (struct AxPlatformAPI *)APIRegistry_->Get(AXON_PLATFORM_API_NAME);
    SceneAPI_ = (struct AxSceneAPI *)APIRegistry_->Get(AXON_SCENE_API_NAME);
    FileAPI_ = PlatformAPI_->FileAPI;

    ResourceAPI_ = (AxResourceAPI*)APIRegistry_->Get(AXON_RESOURCE_API_NAME);
    ShaderManagerAPI_ = (AxShaderManagerAPI*)APIRegistry_->Get(AXON_SHADER_MANAGER_API_NAME);

    // Initialize resource managers
    if (ResourceAPI_) {
        ResourceAPI_->Initialize(APIRegistry_);
    }

    printf("Game: APIs initialized and infrastructure received\n");

    // Register input callbacks now that we have everything
    RegisterCallbacks();
}

AxShaderHandle Game::ConstructShader(const char *VertexShaderPath, const char *FragmentShaderPath)
{
    // Use ShaderManagerAPI to create and manage the shader
    AxShaderHandle Handle = ShaderManagerAPI_->CreateShader(VertexShaderPath, FragmentShaderPath);

    if (!ShaderManagerAPI_->IsValid(Handle)) {
        fprintf(stderr, "Failed to create shader: %s, %s\n", VertexShaderPath, FragmentShaderPath);
        return AX_INVALID_SHADER_HANDLE;
    }

    return Handle;
}

bool Game::LoadShadersForMaterials()
{
    return LoadShadersForMaterials(Scene_);
}

bool Game::LoadShadersForMaterials(const AxScene* Scene)
{
    if (!Scene || !Scene->Materials || Scene->MaterialCount == 0) {
        fprintf(stderr, "Error: No materials found in scene! Cannot load shaders.\n");
        return (false);
    }

    printf("Loading shaders for %d scene materials\n", Scene->MaterialCount);

    for (size_t i = 0; i < Scene->MaterialCount; ++i) {
        AxMaterial* Material = &Scene->Materials[i];

        if (Material->VertexShaderPath[0] == '\0' || Material->FragmentShaderPath[0] == '\0') {
            fprintf(stderr, "Warning: Material '%s' has empty shader paths, skipping.\n", Material->Name);
            continue;
        }

        // Create shader using ShaderManagerAPI
        AxShaderHandle ShaderHandle = ConstructShader(Material->VertexShaderPath, Material->FragmentShaderPath);
        if (!ShaderManagerAPI_->IsValid(ShaderHandle)) {
            fprintf(stderr, "Error: Failed to load shaders for material '%s'\n", Material->Name);
            return (false);
        }

        // Get shader data and program from handle
        const AxShaderData* ShaderData = ShaderManagerAPI_->GetShaderData(ShaderHandle);
        uint32_t ShaderProgram = ShaderManagerAPI_->GetShaderProgram(ShaderHandle);

        // Assign shader program to material
        Material->ShaderProgram = ShaderProgram;
        Material->ShaderData = (AxShaderData*)ShaderData;  // Safe cast - manager owns the data

        // Also assign shader to corresponding model materials
        for (auto& [ModelName, Model] : Models) {
            if (Model.Materials && i < ArraySize(Model.Materials)) {
                AxMaterial* ModelMaterial = &Model.Materials[i];
                // Copy shader paths and data to model material
                strcpy(ModelMaterial->VertexShaderPath, Material->VertexShaderPath);
                strcpy(ModelMaterial->FragmentShaderPath, Material->FragmentShaderPath);
                ModelMaterial->ShaderProgram = Material->ShaderProgram;
                ModelMaterial->ShaderData = Material->ShaderData;
            }
        }
    }

    return (true);
}

bool Game::LoadTexture(AxTexture *Texture, const char *Path)
{
    int32_t Width, Height, Channels;
    stbi_uc *Pixels = stbi_load(Path, &Width, &Height, &Channels, 0);
    if (!Pixels) {
        return (false);
    }

    Texture->Width = Width;
    Texture->Height = Height;
    Texture->Channels = Channels;

    uint32_t OriginalId = Texture->ID;
    RenderAPI_->InitTexture(Texture, Pixels);
    // Bind to texture unit 0 (not GL id!) for setting sampler properties
    RenderAPI_->BindTexture(Texture, 0);

    // Set default sampler properties after Init and Bind
    SetSamplerPropertiesFromGLTF(Texture, nullptr);

    stbi_image_free(Pixels);

    return (true);
}


bool Game::LoadModel(const char *File, AxModel *Model)
{
    cgltf_options Options = {};
    cgltf_data* ModelData = nullptr;
    cgltf_result Result = cgltf_parse_file(&Options, File, &ModelData);
    if (Result != cgltf_result_success) {
        return false;
    }

    Result = cgltf_load_buffers(&Options, ModelData, File);
    if (Result != cgltf_result_success) {
        cgltf_free(ModelData);
        return false;
    }

    const char *BasePath = PlatformAPI_->PathAPI->BasePath(File);
    size_t NumMaterials = ModelData->materials_count;

    std::unordered_map<std::string, size_t> TextureIDs;
    std::vector<AxVertex> VertexData;
    std::vector<uint32_t> IndexData;

    // Create a mapping from material index to texture array index
    std::unordered_map<size_t, size_t> MaterialToTextureMap;

    // Process all materials
    for (cgltf_size i = 0; i < ModelData->materials_count; ++i)
    {
        AxTexture BaseTexture = {0};
        AxTexture NormalTexture = {0};
        AxTexture MetallicRoughnessTexture = {0};
        const auto& Material = ModelData->materials[i];
        const cgltf_pbr_metallic_roughness* pbr = &Material.pbr_metallic_roughness;

        // Store material data for rendering
        AxMaterial AxMat = {0};
        AxMat.BaseColorFactor[0] = pbr->base_color_factor[0];
        AxMat.BaseColorFactor[1] = pbr->base_color_factor[1];
        AxMat.BaseColorFactor[2] = pbr->base_color_factor[2];
        AxMat.BaseColorFactor[3] = pbr->base_color_factor[3];
        snprintf(AxMat.Name, sizeof(AxMat.Name), "%s", Material.name ? Material.name : "Unnamed");

        // Read alpha mode and cutoff from GLTF material
        switch (Material.alpha_mode) {
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

        // Set alpha cutoff (default to 0.5 if not specified, per GLTF spec)
        AxMat.AlphaCutoff = (Material.alpha_cutoff > 0.0f) ? Material.alpha_cutoff : 0.5f;

        // Load Base Color Texture as sRGB
        if (pbr->base_color_texture.texture) {
            if (LoadModelTexture(&BaseTexture, (cgltf_texture_view*)&pbr->base_color_texture, ModelData, BasePath, true)) { // true = sRGB
                size_t TextureIndex = ArraySize(Model->Textures);
                ArrayPush(Model->Textures, BaseTexture);
                MaterialToTextureMap[i] = TextureIndex;
                AxMat.BaseColorTexture = (uint32_t)TextureIndex;
            } else {
                printf("Failed to load base color texture for material %zu ('%s') - continuing without this texture\n",
                    i, Material.name ? Material.name : "unnamed");
                AxMat.BaseColorTexture = kNoTexture;
            }
        } else {
            printf("Material %zu ('%s') has no base_color_texture\n",
                i, Material.name ? Material.name : "unnamed");
            AxMat.BaseColorTexture = kNoTexture;
        }

        // Load Normal Texture as Linear (non-color data)
        if (Material.normal_texture.texture) {
            if (LoadModelTexture(&NormalTexture, (cgltf_texture_view*)&Material.normal_texture, ModelData, BasePath, false)) { // false = linear
                size_t NormalTextureIndex = ArraySize(Model->Textures);
                ArrayPush(Model->Textures, NormalTexture);
                AxMat.NormalTexture = (uint32_t)NormalTextureIndex;
            } else {
                printf("Failed to load normal texture for material %zu - continuing without normal mapping\n", i);
                AxMat.NormalTexture = kNoTexture;
            }
        } else {
            AxMat.NormalTexture = kNoTexture;
        }

        // Load Metallic-Roughness as Linear (non-color data)
        if (pbr->metallic_roughness_texture.texture) {
            if (LoadModelTexture(&MetallicRoughnessTexture, (cgltf_texture_view*)&pbr->metallic_roughness_texture, ModelData, BasePath, false)) { // false = linear
                size_t MrTextureIndex = ArraySize(Model->Textures);
                ArrayPush(Model->Textures, MetallicRoughnessTexture);
                AxMat.MetallicRoughnessTexture = (uint32_t)MrTextureIndex;
            }
        }

        ArrayPush(Model->Materials, AxMat);

        // New Material Description System - populate AxMaterialDesc alongside legacy AxMaterial
        AxMaterialDesc MatDesc = {};
        snprintf(MatDesc.Name, sizeof(MatDesc.Name), "%s", Material.name ? Material.name : "Unnamed");
        MatDesc.Type = AX_MATERIAL_TYPE_PBR;

        // Map base color factor (RGBA)
        MatDesc.PBR.BaseColorFactor.X = pbr->base_color_factor[0];
        MatDesc.PBR.BaseColorFactor.Y = pbr->base_color_factor[1];
        MatDesc.PBR.BaseColorFactor.Z = pbr->base_color_factor[2];
        MatDesc.PBR.BaseColorFactor.W = pbr->base_color_factor[3];

        // Map emissive factor (RGB)
        MatDesc.PBR.EmissiveFactor.X = Material.emissive_factor[0];
        MatDesc.PBR.EmissiveFactor.Y = Material.emissive_factor[1];
        MatDesc.PBR.EmissiveFactor.Z = Material.emissive_factor[2];

        // Map metallic and roughness factors (default to 1.0 per glTF spec)
        MatDesc.PBR.MetallicFactor = pbr->metallic_factor;
        MatDesc.PBR.RoughnessFactor = pbr->roughness_factor;

        // Map texture indices (use -1 for no texture as per renderer-agnostic design)
        MatDesc.PBR.BaseColorTexture = (AxMat.BaseColorTexture == kNoTexture) ? -1 : (int32_t)AxMat.BaseColorTexture;
        MatDesc.PBR.NormalTexture = (AxMat.NormalTexture == kNoTexture) ? -1 : (int32_t)AxMat.NormalTexture;
        MatDesc.PBR.MetallicRoughnessTexture = (AxMat.MetallicRoughnessTexture == kNoTexture) ? -1 : (int32_t)AxMat.MetallicRoughnessTexture;

        // Load emissive texture if present
        if (Material.emissive_texture.texture) {
            AxTexture EmissiveTexture = {0};
            if (LoadModelTexture(&EmissiveTexture, (cgltf_texture_view*)&Material.emissive_texture, ModelData, BasePath, true)) { // true = sRGB
                size_t EmissiveTextureIndex = ArraySize(Model->Textures);
                ArrayPush(Model->Textures, EmissiveTexture);
                MatDesc.PBR.EmissiveTexture = (int32_t)EmissiveTextureIndex;
            } else {
                MatDesc.PBR.EmissiveTexture = -1;
            }
        } else {
            MatDesc.PBR.EmissiveTexture = -1;
        }

        // Load occlusion texture if present
        if (Material.occlusion_texture.texture) {
            AxTexture OcclusionTexture = {0};
            if (LoadModelTexture(&OcclusionTexture, (cgltf_texture_view*)&Material.occlusion_texture, ModelData, BasePath, false)) { // false = linear
                size_t OcclusionTextureIndex = ArraySize(Model->Textures);
                ArrayPush(Model->Textures, OcclusionTexture);
                MatDesc.PBR.OcclusionTexture = (int32_t)OcclusionTextureIndex;
            } else {
                MatDesc.PBR.OcclusionTexture = -1;
            }
        } else {
            MatDesc.PBR.OcclusionTexture = -1;
        }

        // Map alpha mode
        switch (Material.alpha_mode) {
            case cgltf_alpha_mode_opaque:
                MatDesc.PBR.AlphaMode = AX_ALPHA_MODE_OPAQUE;
                break;
            case cgltf_alpha_mode_mask:
                MatDesc.PBR.AlphaMode = AX_ALPHA_MODE_MASK;
                break;
            case cgltf_alpha_mode_blend:
                MatDesc.PBR.AlphaMode = AX_ALPHA_MODE_BLEND;
                break;
            default:
                MatDesc.PBR.AlphaMode = AX_ALPHA_MODE_OPAQUE;
                break;
        }

        // Map alpha cutoff (default to 0.5 per glTF spec)
        MatDesc.PBR.AlphaCutoff = (Material.alpha_cutoff > 0.0f) ? Material.alpha_cutoff : 0.5f;

        // Map double-sided flag
        MatDesc.PBR.DoubleSided = Material.double_sided;

        ArrayPush(Model->MaterialDescs, MatDesc);
    }

    // Diagnostic: Report texture loading success rate
    printf("Loaded %zu base color textures for %zu materials\n",
           ArraySize(Model->Textures), ModelData->materials_count);

    // First, build node transforms accounting for parent-child hierarchy
    std::vector<AxMat4x4> NodeTransforms(ModelData->nodes_count);
    std::vector<bool> NodeProcessed(ModelData->nodes_count, false);

    // Function to recursively compute node transform with parent chain
    std::function<AxMat4x4(cgltf_size)> ComputeNodeTransform = [&](cgltf_size NodeIndex) -> AxMat4x4 {
        if (NodeIndex >= ModelData->nodes_count) {
            printf("ERROR: Invalid nodeIndex %d (max %d)\n", (int)NodeIndex, (int)ModelData->nodes_count);
            return Identity();
        }

        if (NodeProcessed[NodeIndex]) {
            return NodeTransforms[NodeIndex];
        }

        cgltf_node* Node = &ModelData->nodes[NodeIndex];
        const char* NodeName = Node->name ? Node->name : "";
        AxMat4x4 LocalTransform;

        // Build local transform
        if (Node->has_matrix) {
            memcpy(&LocalTransform, Node->matrix, sizeof(AxMat4x4));
        } else {
            // Build from TRS
            AxVec3 Translation = Node->has_translation ?
                (AxVec3){Node->translation[0], Node->translation[1], Node->translation[2]} :
                (AxVec3){0.0f, 0.0f, 0.0f};

            AxQuat Rotation = Node->has_rotation ?
                (AxQuat){Node->rotation[0], Node->rotation[1], Node->rotation[2], Node->rotation[3]} :
                QuatIdentity();

            AxVec3 Scale = Node->has_scale ?
                (AxVec3){Node->scale[0], Node->scale[1], Node->scale[2]} :
                Vec3One();


            // R * S + direct translation
            AxMat4x4 R = QuatToMat4x4(Rotation);
            AxMat4x4 S = Identity();
            S.E[0][0] = Scale.X;
            S.E[1][1] = Scale.Y;
            S.E[2][2] = Scale.Z;

            AxMat4x4 RS = Mat4x4Mul(R, S);
            LocalTransform = RS;

            // Apply translation directly
            LocalTransform.E[3][0] = Translation.X;
            LocalTransform.E[3][1] = Translation.Y;
            LocalTransform.E[3][2] = Translation.Z;
            LocalTransform.E[3][3] = 1.0f;
        }

        // Find parent and multiply: ParentMatrix * LocalMatrix
        AxMat4x4 FinalTransform = LocalTransform;
        cgltf_size ParentIndex = (cgltf_size)-1;

        // Search for parent node (node that has this nodeIndex in its children)
        for (cgltf_size i = 0; i < ModelData->nodes_count; ++i) {
            cgltf_node* PotentialParent = &ModelData->nodes[i];
            for (cgltf_size ChildIdx = 0; ChildIdx < PotentialParent->children_count; ++ChildIdx) {
                // CGLTF children array contains pointers to child nodes
                // Calculate the index by subtracting base address
                if (PotentialParent->children[ChildIdx] >= ModelData->nodes &&
                    PotentialParent->children[ChildIdx] < ModelData->nodes + ModelData->nodes_count) {
                    cgltf_size ChildNodeIndex = PotentialParent->children[ChildIdx] - ModelData->nodes;
                    if (ChildNodeIndex == NodeIndex) {
                        ParentIndex = i;
                        break;
                    }
                }
            }

            if (ParentIndex != (cgltf_size)-1)
                break;
        }

        if (ParentIndex != (cgltf_size)-1) {
            if (ParentIndex == NodeIndex) {
                // Node trying to be its own parent
                FinalTransform = LocalTransform;
            } else {
                AxMat4x4 ParentTransform = ComputeNodeTransform(ParentIndex);
                FinalTransform = Mat4x4Mul(ParentTransform, LocalTransform);
            }
        }

        NodeTransforms[NodeIndex] = FinalTransform;
        NodeProcessed[NodeIndex] = true;
        return FinalTransform;
    };

    // Process all nodes to build final transforms with hierarchy
    for (cgltf_size NodeIndex = 0; NodeIndex < ModelData->nodes_count; ++NodeIndex) {
        AxMat4x4 Transform = ComputeNodeTransform(NodeIndex);
        ArrayPush(Model->Transforms, Transform);
    }

    // Iterate over meshes and primitives
    for (cgltf_size i = 0; i < ModelData->meshes_count; ++i) {
        cgltf_mesh* Mesh = &ModelData->meshes[i];
        const char* MeshName = Mesh->name ? Mesh->name : "unnamed";

        for (cgltf_size j = 0; j < Mesh->primitives_count; ++j) {
            cgltf_primitive* Primitive = &Mesh->primitives[j];

            // Attribute data
            std::vector<float> PositionData;
            std::vector<float> NormalData;
            std::vector<float> TexCoordData0;  // TEXCOORD_0
            std::vector<float> TexCoordData1;  // TEXCOORD_1

            // Load all attributes first
            for (cgltf_size k = 0; k < Primitive->attributes_count; ++k) {
                cgltf_attribute* Attribute = &Primitive->attributes[k];
                cgltf_accessor* Accessor = Attribute->data;

                if (Attribute->type == cgltf_attribute_type_position) {
                    size_t ElementCount = Accessor->count * 3;
                    PositionData.clear();
                    PositionData.reserve(ElementCount);

                    for (cgltf_size idx = 0; idx < Accessor->count; ++idx) {
                        float Values[3];
                        cgltf_accessor_read_float(Accessor, idx, Values, 3);
                        PositionData.push_back(Values[0]);
                        PositionData.push_back(Values[1]);
                        PositionData.push_back(Values[2]);
                    }
                } else if (Attribute->type == cgltf_attribute_type_normal) {
                    size_t ElementCount = Accessor->count * 3;
                    NormalData.clear();
                    NormalData.reserve(ElementCount);

                    for (cgltf_size idx = 0; idx < Accessor->count; ++idx) {
                        float Values[4] = {0};
                        cgltf_accessor_read_float(Accessor, idx, Values, 3);

                        NormalData.push_back(Values[0]);
                        NormalData.push_back(Values[1]);
                        NormalData.push_back(Values[2]);
                    }
                } else if (Attribute->type == cgltf_attribute_type_texcoord) {
                    // Check which TEXCOORD set this is (0, 1, etc.)
                    std::vector<float>* TargetData = nullptr;

                    if (Attribute->index == 0) {
                        TargetData = &TexCoordData0;
                    } else if (Attribute->index == 1) {
                        TargetData = &TexCoordData1;
                    } else {
                        continue;
                    }

                    size_t ElementCount = Accessor->count * 2;
                    TargetData->clear();
                    TargetData->reserve(ElementCount);

                    for (cgltf_size idx = 0; idx < Accessor->count; ++idx) {
                        float Values[2];
                        cgltf_accessor_read_float(Accessor, idx, Values, 2);
                        TargetData->push_back(Values[0]);
                        TargetData->push_back(Values[1]);
                    }
                }
            }

            // Determine which TEXCOORD set to use and capture texture transform
            const cgltf_texture_view* BaseView = nullptr;
            int TexcoordSetToUse = 0;

            if (Primitive->material) {
                const cgltf_pbr_metallic_roughness* Pbr = &Primitive->material->pbr_metallic_roughness;
                BaseView = &Pbr->base_color_texture;

                // default UV set
                TexcoordSetToUse = BaseView->texcoord;

                // KHR_texture_transform can override the UV set
                if (BaseView->has_transform && BaseView->transform.has_texcoord) {
                    TexcoordSetToUse = BaseView->transform.texcoord;
                }
            }

            // Select the appropriate texture coordinate data
            const std::vector<float>* TexCoordDataToUse = nullptr;
            if (TexcoordSetToUse == 0 && !TexCoordData0.empty()) {
                TexCoordDataToUse = &TexCoordData0;
            } else if (TexcoordSetToUse == 1 && !TexCoordData1.empty()) {
                TexCoordDataToUse = &TexCoordData1;
            } else {
                TexCoordDataToUse = &TexCoordData0;  // Fallback
            }

            // Create vertices combining all attributes
            size_t VertexCount = PositionData.size() / 3;

            VertexData.clear();
            VertexData.resize(VertexCount);

            // First pass: create basic vertices
            for (size_t i = 0; i < VertexCount; ++i) {
                AxVertex Vertex = {};

                // Position (always present)
                if (!PositionData.empty()) {
                    Vertex.Position = {
                        PositionData[i * 3],
                        PositionData[i * 3 + 1],
                        PositionData[i * 3 + 2]
                    };
                }

                // Normal (if present)
                if (!NormalData.empty()) {
                    Vertex.Normal = {
                        NormalData[i * 3],
                        NormalData[i * 3 + 1],
                        NormalData[i * 3 + 2]
                    };
                }

                // TexCoord (if present) - apply KHR_texture_transform
                if (TexCoordDataToUse && !TexCoordDataToUse->empty()) {
                    AxVec2 Uv = { (*TexCoordDataToUse)[i*2 + 0],
                                  (*TexCoordDataToUse)[i*2 + 1] };

                    if (BaseView && BaseView->has_transform) {
                        const auto& Tt = BaseView->transform;

                        // Use glTF defaults: offset=[0,0], scale=[1,1], rotation=0
                        // cgltf doesn't have has_* flags, so use defaults if values look wrong
                        float OffX = Tt.offset[0];
                        float OffY = Tt.offset[1];
                        float SclX = (Tt.scale[0] > 0.0f) ? Tt.scale[0] : 1.0f;
                        float SclY = (Tt.scale[1] > 0.0f) ? Tt.scale[1] : 1.0f;
                        float Rot  = Tt.rotation; // radians

                        // SCALE -> ROTATE -> OFFSET (per KHR_texture_transform spec)
                        // This order is critical for non-uniform atlas scales
                        float Us = Uv.X * SclX;
                        float Vs = Uv.Y * SclY;

                        float C = cosf(Rot), S = sinf(Rot);
                        float Ur =  C*Us - S*Vs;
                        float Vr =  S*Us + C*Vs;

                        Uv.X = OffX + Ur;
                        Uv.Y = OffY + Vr;
                    }

                    Vertex.TexCoord = Uv;
                } else {
                    // No texture coordinates available - set invalid coordinates to trigger debug color
                    Vertex.TexCoord = { -1.0f, -1.0f };
                }

                // Initialize tangent vector (XYZ = tangent, W = handedness)
                Vertex.Tangent = { 0.0f, 0.0f, 0.0f, 1.0f };

                VertexData[i] = Vertex;
            }

            // Load indices
            IndexData.clear();
            if (Primitive->indices) {
                cgltf_accessor* Accessor = Primitive->indices;
                for (cgltf_size i = 0; i < Accessor->count; ++i) {
                    uint32_t Index = (uint32_t)cgltf_accessor_read_index(Accessor, i);
                    IndexData.push_back(Index);
                }
            } else {
                // If no indices, create sequential indices
                for (size_t i = 0; i < VertexCount; ++i) {
                    IndexData.push_back((uint32_t)i);
                }
            }

            // Calculate tangent and bitangent vectors for normal mapping
            if (TexCoordDataToUse && !TexCoordDataToUse->empty() && IndexData.size() >= 3) {
                // Process triangles to calculate tangent space
                for (size_t i = 0; i < IndexData.size(); i += 3) {
                    uint32_t i0 = IndexData[i];
                    uint32_t i1 = IndexData[i + 1];
                    uint32_t i2 = IndexData[i + 2];

                    if (i0 < VertexCount && i1 < VertexCount && i2 < VertexCount) {
                        AxVec3 Tangent, Bitangent;
                        CalculateTangentBitangent(
                            VertexData[i0].Position, VertexData[i1].Position, VertexData[i2].Position,
                            VertexData[i0].TexCoord, VertexData[i1].TexCoord, VertexData[i2].TexCoord,
                            &Tangent, &Bitangent
                        );

                        // Add tangent to all vertices of this triangle
                        // (We'll average them later for smooth results and calculate handedness)
                        AxVec3 CurrentTangent0 = { VertexData[i0].Tangent.X, VertexData[i0].Tangent.Y, VertexData[i0].Tangent.Z };
                        AxVec3 CurrentTangent1 = { VertexData[i1].Tangent.X, VertexData[i1].Tangent.Y, VertexData[i1].Tangent.Z };
                        AxVec3 CurrentTangent2 = { VertexData[i2].Tangent.X, VertexData[i2].Tangent.Y, VertexData[i2].Tangent.Z };

                        AxVec3 NewTangent0 = Vec3Add(CurrentTangent0, Tangent);
                        AxVec3 NewTangent1 = Vec3Add(CurrentTangent1, Tangent);
                        AxVec3 NewTangent2 = Vec3Add(CurrentTangent2, Tangent);

                        VertexData[i0].Tangent = { NewTangent0.X, NewTangent0.Y, NewTangent0.Z, VertexData[i0].Tangent.W };
                        VertexData[i1].Tangent = { NewTangent1.X, NewTangent1.Y, NewTangent1.Z, VertexData[i1].Tangent.W };
                        VertexData[i2].Tangent = { NewTangent2.X, NewTangent2.Y, NewTangent2.Z, VertexData[i2].Tangent.W };
                    }
                }

                // Normalize tangent vectors and calculate handedness
                for (size_t i = 0; i < VertexCount; ++i) {
                    AxVec3 Tangent = { VertexData[i].Tangent.X, VertexData[i].Tangent.Y, VertexData[i].Tangent.Z };
                    AxVec3 Normal = VertexData[i].Normal;

                    if (Vec3Length(Tangent) > 0.0f) {
                        // Normalize the tangent
                        Tangent = Vec3Normalize(Tangent);

                        // Calculate the bitangent using cross product
                        AxVec3 ComputedBitangent = Vec3Cross(Normal, Tangent);

                        // Calculate handedness by comparing with the original bitangent
                        // For this implementation, we'll assume right-handed coordinate system
                        // In a full implementation, you'd calculate this based on UV mapping direction
                        float Handedness = 1.0f; // Default to right-handed

                        // Store normalized tangent with handedness
                        VertexData[i].Tangent = { Tangent.X, Tangent.Y, Tangent.Z, Handedness };
                    }
                }
            }

            // Create the mesh
            AxMesh Mesh = {0};
            RenderAPI_->InitMesh(&Mesh, VertexData.data(), IndexData.data(),
                                (uint32_t)VertexData.size(), (uint32_t)IndexData.size());

            // Store the mesh name for debugging
            snprintf(Mesh.Name, sizeof(Mesh.Name), "%s_p%d", MeshName, static_cast<int32_t>(j));

            // Store material reference for later use during rendering
            Mesh.MaterialIndex = 0; // Default material index
            Mesh.BaseColorTexture = kNoTexture;
            Mesh.NormalTexture = kNoTexture;

            if (Primitive->material) {
                // Find the material index in the materials array
                for (cgltf_size MatIndex = 0; MatIndex < ModelData->materials_count; ++MatIndex) {
                    if (Primitive->material == &ModelData->materials[MatIndex]) {
                        Mesh.MaterialIndex = (uint32_t)MatIndex;

                        // Set base color texture
                        if (MaterialToTextureMap.find(MatIndex) != MaterialToTextureMap.end()) {
                            Mesh.BaseColorTexture = (uint32_t)MaterialToTextureMap[MatIndex];
                        }

                        // Set normal texture from the material
                        if (MatIndex < ArraySize(Model->Materials)) {
                            Mesh.NormalTexture = Model->Materials[MatIndex].NormalTexture;
                        }
                        break;
                    }
                }
            }

            // Find which node references this mesh and set the transform index
            bool FoundNode = false;
            for (cgltf_size NodeIndex = 0; NodeIndex < ModelData->nodes_count; ++NodeIndex) {
                cgltf_node* Node = &ModelData->nodes[NodeIndex];
                if (Node->mesh && Node->mesh == &ModelData->meshes[i]) {
                    if (!FoundNode) {
                        Mesh.TransformIndex = (uint32_t)NodeIndex;
                        FoundNode = true;
                    }
                }
            }

            // If no node references this mesh, add an identity transform
            if (!FoundNode) {
                AxMat4x4 IdentityTransform = Identity();
                ArrayPush(Model->Transforms, IdentityTransform);
                Mesh.TransformIndex = ArraySize(Model->Transforms) - 1;
            }

            ArrayPush(Model->Meshes, Mesh);
        }
    }

    cgltf_free(ModelData);

    printf("Model loaded: %zu meshes, VertexBuffer=%u, IndexBuffer=%u\n",
           ArraySize(Model->Meshes), Model->VertexBuffer, Model->IndexBuffer);

    return true;
}

void Game::ProcessSceneObject(AxSceneObject* Object)
{
    if (!Object) return;


    // Load model if mesh path is specified
    if (Object->MeshPath[0] != '\0') {
        AxModel ObjectModel = {};
        if (LoadModel(Object->MeshPath, &ObjectModel)) {
            // Apply scene material to all meshes in the model if specified
            if (Object->MaterialPath[0] != '\0' && Scene_) {
                AxMaterial* SceneMaterial = SceneAPI_->FindMaterial(Scene_, Object->MaterialPath);
                if (SceneMaterial && SceneMaterial->ShaderData) {
                    // Apply the scene material's shader data to all model meshes
                    if (ObjectModel.Materials) {
                        for (int i = 0; i < ArraySize(ObjectModel.Materials); i++) {
                            AxMaterial* ModelMaterial = &ObjectModel.Materials[i];
                            ModelMaterial->ShaderData = SceneMaterial->ShaderData;
                            ModelMaterial->ShaderProgram = SceneMaterial->ShaderProgram;
                            // Copy shader paths for reference
                            strcpy(ModelMaterial->VertexShaderPath, SceneMaterial->VertexShaderPath);
                            strcpy(ModelMaterial->FragmentShaderPath, SceneMaterial->FragmentShaderPath);
                        }
                        printf("Applied material '%s' to model '%s'\n", 
                               Object->MaterialPath, Object->Name);
                    }
                } else if (Object->MaterialPath[0] != '\0') {
                    fprintf(stderr, "Warning: Scene material '%s' not found or has no shader data\n", Object->MaterialPath);
                }
            }

            // Use object name as model key
            Models[Object->Name] = ObjectModel;
            printf("Loaded model '%s'\n", Object->Name);
        } else {
            fprintf(stderr, "Failed to load model for object '%s': %s\n", Object->Name, Object->MeshPath);
        }
    }

    // Process child objects recursively
    for (AxSceneObject* Child = Object->FirstChild; Child != nullptr; Child = Child->NextSibling) {
        ProcessSceneObject(Child);
    }
}

void Game::LoadObjectModel(const AxSceneObject* Object)
{
    if (!Object || Object->MeshPath[0] == '\0') {
        return;
    }

    printf("Loading model: %s...\n", Object->MeshPath);
    AxModel ObjectModel = {};
    if (LoadModel(Object->MeshPath, &ObjectModel)) {
        // Store model - material assignment happens in post-processing
        Models[Object->Name] = ObjectModel;
        printf("Loaded model '%s' (%zu meshes)\n", Object->Name, ArraySize(ObjectModel.Meshes));
    } else {
        fprintf(stderr, "Failed to load model for object '%s': %s\n", Object->Name, Object->MeshPath);
    }
}

void Game::ApplySceneMaterialsToModel(AxModel* Model, const char* ObjectName)
{
    // Find the object in the scene to get its material assignment
    AxSceneObject* Object = SceneAPI_->FindObject(Scene_, ObjectName);
    if (!Object) {
        printf("Could not find object '%s' in scene\n", ObjectName);
        return;
    }

    if (Object->MaterialPath[0] == '\0') {
        printf("Object '%s' has no material assignment\n", ObjectName);
        return;
    }

    // Find the scene material with shader data
    AxMaterial* SceneMaterial = SceneAPI_->FindMaterial(Scene_, Object->MaterialPath);
    if (!SceneMaterial || !SceneMaterial->ShaderData) {
        printf("Scene material '%s' not found or has no shader data\n", Object->MaterialPath);
        return;
    }

    printf("Applying scene material '%s' to model '%s'\n", Object->MaterialPath, ObjectName);

    // Apply scene material to all model materials
    if (Model->Materials) {
        for (int i = 0; i < ArraySize(Model->Materials); i++) {
            AxMaterial* ModelMaterial = &Model->Materials[i];
            printf("Material %d before: ShaderData=%p\n", i, ModelMaterial->ShaderData);
            ModelMaterial->ShaderData = SceneMaterial->ShaderData;
            ModelMaterial->ShaderProgram = SceneMaterial->ShaderProgram;
            strcpy(ModelMaterial->VertexShaderPath, SceneMaterial->VertexShaderPath);
            strcpy(ModelMaterial->FragmentShaderPath, SceneMaterial->FragmentShaderPath);
            printf("Material %d after: ShaderData=%p\n", i, ModelMaterial->ShaderData);
        }
        printf("Applied shader data to %zu materials\n", ArraySize(Model->Materials));
    } else {
        printf("Model has no materials array!\n");
    }
}

void Game::AssignCompiledShadersToScene(const AxScene* Scene)
{
    printf("Assigning compiled shaders to scene materials...\n");

    // Assign compiled shaders to scene materials
    for (int i = 0; i < Scene->MaterialCount; i++) {
        AxMaterial* SceneMaterial = &Scene->Materials[i];

        auto ShaderIt = CompiledShaders.find(std::string(SceneMaterial->Name));
        if (ShaderIt != CompiledShaders.end()) {
            AxShaderHandle ShaderHandle = ShaderIt->second;

            // Get shader data and program ID from the handle
            const AxShaderData* ShaderData = ShaderManagerAPI_->GetShaderData(ShaderHandle);
            uint32_t ShaderProgram = ShaderManagerAPI_->GetShaderProgram(ShaderHandle);

            // Assign to scene material (note: ShaderData is const, but AxMaterial expects non-const)
            // This is safe because the ShaderManager owns the data and won't delete it while refcount > 0
            SceneMaterial->ShaderData = (AxShaderData*)ShaderData;
            SceneMaterial->ShaderProgram = ShaderProgram;

            printf("Assigned shader to scene material '%s'\n", SceneMaterial->Name);
        } else {
            printf("Warning: No compiled shader found for material '%s'\n", SceneMaterial->Name);
        }
    }

    // NOTE: We don't clear CompiledShaders here because we need to keep the handles alive
    // The handles will be released when ResourceAPI_->Shutdown() is called
    printf("Shader assignment complete\n");
}

void Game::SetupSceneCamera(const AxScene* Scene)
{
    if (!Scene) return;

    // Find camera object in scene for initial camera positioning
    AxSceneObject* CameraObject = SceneAPI_->FindObject(Scene, "DefaultCamera");
    if (CameraObject) {
        Transform_ = &CameraObject->Transform;
        printf("SceneAdapter: Using scene camera 'DefaultCamera'\n");
    } else {
        // Use default camera transform if no camera object found
        static AxTransform defaultTransform = {
            .Translation = {0.0f, 2.0f, 5.0f},  // Position camera away from origin
            .Rotation = {0.0f, 0.0f, 0.0f, 1.0f},
            .Scale = {1.0f, 1.0f, 1.0f},
            .Up = {0.0f, 1.0f, 0.0f}
        };
        Transform_ = &defaultTransform;
        printf("SceneAdapter: Using default camera transform at (0, 2, 5)\n");
    }
}

void Game::RegisterSceneEventHandler()
{
    AxSceneEvents Events = {0};
    Events.OnLightParsed = OnLightParsed;
    Events.OnMaterialParsed = OnMaterialParsed;
    Events.OnObjectParsed = OnObjectParsed;
    Events.OnTransformParsed = OnTransformParsed;
    Events.OnSceneParsed = OnSceneParsed;
    Events.UserData = this;

    AxSceneResult Result = SceneAPI_->RegisterEventHandler(&Events);
    if (Result != AX_SCENE_SUCCESS) {
        fprintf(stderr, "Failed to register scene event handler: %s\n", SceneAPI_->GetLastError());
    } else {
        printf("SceneAdapter: Event handler registered successfully\n");
    }
}

void Game::UnregisterSceneEventHandler()
{
    AxSceneEvents Events = {0};
    Events.OnLightParsed = OnLightParsed;
    Events.OnMaterialParsed = OnMaterialParsed;
    Events.OnObjectParsed = OnObjectParsed;
    Events.OnTransformParsed = OnTransformParsed;
    Events.OnSceneParsed = OnSceneParsed;
    Events.UserData = this;

    AxSceneResult Result = SceneAPI_->UnregisterEventHandler(&Events);
    if (Result != AX_SCENE_SUCCESS) {
        fprintf(stderr, "Failed to unregister scene event handler: %s\n", SceneAPI_->GetLastError());
    } else {
        printf("SceneAdapter: Event handler unregistered successfully\n");
    }
}

bool Game::CreateScene()
{
    // Register our scene event handler before loading
    RegisterSceneEventHandler();

    // Load scene from file - callbacks will handle all processing automatically
    Scene_ = SceneAPI_->LoadSceneFromFile("examples/graphics/scenes/sponza_atrium.ats");
    if (!Scene_) {
        const char* Error = SceneAPI_->GetLastError();
        fprintf(stderr, "Failed to load scene: %s\n", Error ? Error : "Unknown error");
        UnregisterSceneEventHandler();
        return (false);
    } else {
        // Scene loaded successfully - all processing was handled by callbacks
        printf("Scene loaded via callback system: %s\n", Scene_->Name);
        printf("Models loaded: %zu, Camera transform set: %s\n", Models.size(), Transform_ ? "Yes" : "No");

        // Post-process material assignment to ensure all meshes have shader data
        for (auto& [modelName, model] : Models) {
            ApplySceneMaterialsToModel(&model, modelName.c_str());
        }
        printf("Applied materials to all loaded models\n");
    }

    // Create the camera
    Camera_ = new AxCamera();
   RenderAPI_->CreateCamera(Camera_);
   RenderAPI_->CameraSetFOV(Camera_, 60.0f * (AX_PI / 180.0f));  // Wider FOV
   RenderAPI_->CameraSetAspectRatio(Camera_, Viewport_->Size.X / Viewport_->Size.Y);
   RenderAPI_->CameraSetNearClipPlane(Camera_, 0.1f);   // Near clip plane
   RenderAPI_->CameraSetFarClipPlane(Camera_, 100.0f);  // Far clip plane

    return (true);
}

void Game::RenderModelWithMeshes(AxModel* Model, AxCamera* Camera, AxTransform* CameraTransform, const AxViewport* Viewport)
{
    // Basic matrices
    AxMat4x4 ViewMatrix = CreateViewMatrix(CameraTransform);
    AxMat4x4 ProjectionMatrix = RenderAPI_->CameraGetProjectionMatrix(Camera);
    AxVec3 ViewPos = CameraTransform->Translation;

    // Get scene light count (max 8 lights supported by shader)
    int LightCount = 0;
    if (Scene_ && Scene_->Lights) {
        LightCount = (Scene_->LightCount < 8) ? Scene_->LightCount : 8;
    }

    // Enable blending for transparent objects
    RenderAPI_->EnableBlending(true);
    RenderAPI_->SetBlendFunction(AX_BLEND_SRC_ALPHA, AX_BLEND_ONE_MINUS_SRC_ALPHA);

    for (int i = 0; i < ArraySize(Model->Meshes); i++) {
        AxMesh* Mesh = &Model->Meshes[i];

        // Use MaterialDesc system
        if (!Model->MaterialDescs || Mesh->MaterialIndex >= ArraySize(Model->MaterialDescs)) {
            continue;  // No material available
        }

        AxMaterialDesc* MatDesc = &Model->MaterialDescs[Mesh->MaterialIndex];

        // Get shader data from legacy material array (temporary until shader management is refactored)
        AxShaderData* ShaderData = nullptr;
        if (Model->Materials && Mesh->MaterialIndex < ArraySize(Model->Materials)) {
            ShaderData = (AxShaderData*)Model->Materials[Mesh->MaterialIndex].ShaderData;
        }

        if (!ShaderData) {
            continue;
        }

        // Transform matrices
        AxMat4x4 ModelMatrix = Model->Transforms[Mesh->TransformIndex];
        RenderAPI_->SetUniform(ShaderData, "model", &ModelMatrix);
        RenderAPI_->SetUniform(ShaderData, "view", &ViewMatrix);
        RenderAPI_->SetUniform(ShaderData, "projection", &ProjectionMatrix);
        RenderAPI_->SetUniform(ShaderData, "viewPos", &ViewPos);

        // Use MaterialDesc PBR system
        if (MatDesc->Type != AX_MATERIAL_TYPE_PBR) {
            continue;  // Only PBR materials supported for now
        }

        const AxPBRMaterial* PBR = &MatDesc->PBR;

        // Bind textures to their respective slots
        // Base color texture (slot 0)
        if (PBR->BaseColorTexture >= 0 && Model->Textures && PBR->BaseColorTexture < (int32_t)ArraySize(Model->Textures)) {
            AxTexture* Tex = &Model->Textures[PBR->BaseColorTexture];
            if (Tex->ID != 0) {
                RenderAPI_->BindTexture(Tex, 0);
            }
        }

        // Normal texture (slot 1)
        if (PBR->NormalTexture >= 0 && Model->Textures && PBR->NormalTexture < (int32_t)ArraySize(Model->Textures)) {
            AxTexture* Tex = &Model->Textures[PBR->NormalTexture];
            if (Tex->ID != 0) {
                RenderAPI_->BindTexture(Tex, 1);
            }
        }

        // Metallic-Roughness texture (slot 2)
        if (PBR->MetallicRoughnessTexture >= 0 && Model->Textures && PBR->MetallicRoughnessTexture < (int32_t)ArraySize(Model->Textures)) {
            AxTexture* Tex = &Model->Textures[PBR->MetallicRoughnessTexture];
            if (Tex->ID != 0) {
                RenderAPI_->BindTexture(Tex, 2);
            }
        }

        // Emissive texture (slot 3)
        if (PBR->EmissiveTexture >= 0 && Model->Textures && PBR->EmissiveTexture < (int32_t)ArraySize(Model->Textures)) {
            AxTexture* Tex = &Model->Textures[PBR->EmissiveTexture];
            if (Tex->ID != 0) {
                RenderAPI_->BindTexture(Tex, 3);
            }
        }

        // Occlusion texture (slot 4)
        if (PBR->OcclusionTexture >= 0 && Model->Textures && PBR->OcclusionTexture < (int32_t)ArraySize(Model->Textures)) {
            AxTexture* Tex = &Model->Textures[PBR->OcclusionTexture];
            if (Tex->ID != 0) {
                RenderAPI_->BindTexture(Tex, 4);
            }
        }

        // Set all PBR material uniforms, texture samplers, and lights in one efficient call
        // This replaces ~25 individual SetUniform calls and eliminates uniform warnings
        RenderAPI_->SetPBRMaterialUniforms(ShaderData, PBR, Scene_->Lights, LightCount);

        // Configure depth and blending based on alpha mode
        switch (PBR->AlphaMode) {
            case AX_ALPHA_MODE_OPAQUE:
                RenderAPI_->EnableBlending(false);
                RenderAPI_->SetDepthWrite(true);
                break;

            case AX_ALPHA_MODE_MASK:
                RenderAPI_->EnableBlending(false);
                RenderAPI_->SetDepthWrite(true);
                break;

            case AX_ALPHA_MODE_BLEND:
                RenderAPI_->EnableBlending(true);
                RenderAPI_->SetBlendFunction(AX_BLEND_SRC_ALPHA, AX_BLEND_ONE_MINUS_SRC_ALPHA);
                RenderAPI_->SetDepthWrite(false);
                break;

            default:
                RenderAPI_->EnableBlending(false);
                RenderAPI_->SetDepthWrite(true);
                break;
        }

        // Double-sided rendering
        if (PBR->DoubleSided) {
            RenderAPI_->SetCullMode(false);  // Disable backface culling
        } else {
            RenderAPI_->SetCullMode(true);   // Enable backface culling
        }

        // Render this mesh
        RenderAPI_->Render((AxViewport*)Viewport, Mesh, ShaderData);

        // Restore defaults for next mesh
        RenderAPI_->SetDepthWrite(true);
        RenderAPI_->EnableBlending(false);
        RenderAPI_->SetCullMode(true);  // Re-enable culling by default
    }

    // Restore blending state
    RenderAPI_->EnableBlending(false);
}

void Game::UpdateCamera(float DeltaTime)
{
    // Get input state
    enum AxWindowError Error = AX_WINDOW_ERROR_NONE;
    AxInputState InputState = {0};
    if (!WindowAPI_->GetWindowInputState(Window_, &InputState, &Error)) {
        return;
    }

    float Horizontal = GetAxis(InputState.Keys[AX_KEY_D], InputState.Keys[AX_KEY_A]);
    float Vertical = GetAxis(InputState.Keys[AX_KEY_W], InputState.Keys[AX_KEY_S]);
    float VerticalUpDown = GetAxis(InputState.Keys[AX_KEY_E], InputState.Keys[AX_KEY_Q]);

    // Calculate movement in local space (relative to camera rotation)
    // Note: In OpenGL/3D graphics, -Z is forward, so negate the vertical movement
    AxVec3 LocalMovement = {
        Horizontal * CameraSpeed * DeltaTime,        // X: right/left
        VerticalUpDown * CameraSpeed * DeltaTime,    // Y: up/down
        -Vertical * CameraSpeed * DeltaTime          // Z: forward/back (negated for OpenGL)
    };

    TransformTranslate(Transform_, LocalMovement, false); // false = local space

    // Print camera position and rotation when P key is pressed
    if (InputState.Keys[AX_KEY_P]) {
        AxVec3 EulerAngles = QuatToEuler(Transform_->Rotation);
        printf("Camera Position: X=%.2f, Y=%.2f, Z=%.2f\n",
               Transform_->Translation.X,
               Transform_->Translation.Y,
               Transform_->Translation.Z);
        printf("Camera Rotation (Euler): Pitch=%.2f, Yaw=%.2f, Roll=%.2f\n",
               EulerAngles.X * (180.0f / AX_PI),
               EulerAngles.Y * (180.0f / AX_PI),
               EulerAngles.Z * (180.0f / AX_PI));
    }
}

void Game::IsRequestingExit(bool Value)
{
    IsRequestingExit_ = Value;
}

