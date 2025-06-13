#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <unordered_map>

#include "AxApplication.h"

#define AXARRAY_IMPLEMENTATION
#include "Foundation/AxArray.h"
#include "Foundation/AxPlugin.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxApplication.h"
#include "Foundation/AxPlatform.h"
#include "Foundation/AxCamera.h"
#include "Foundation/AxMath.h"

#include "AxWindow/AxWindow.h"
#include "AxOpenGL/AxOpenGL.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static constexpr AxVec2 WINDOW_SIZE = { 1920, 1080 };

// NOTE(mdeforge): These get assigned to in LoadPlugin().
struct AxAPIRegistry *RegistryAPI;
struct AxPluginAPI *PluginAPI;
struct AxWindowAPI *WindowAPI;
struct AxOpenGLAPI *RenderAPI;
struct AxPlatformAPI *PlatformAPI;
struct AxPlatformFileAPI *FileAPI;

static AxCamera PerspCamera;
static AxCamera DiffuseCamera;

static AxShaderData *ModelShaderData;
static AxShaderData *DiffuseShaderData;

static AxViewport DiffuseViewport;
static AxViewport Viewport;
static AxDrawData *DrawData;

std::unordered_map<std::string, AxMesh> Meshes;
std::unordered_map<std::string, AxModel> Models;

static AxApplication *TheApp;

static void RegisterCallbacks()
{
    WindowAPI->SetKeyCallback(TheApp->Window, [](AxWindow *Window, int Key, int ScanCode, int Action, int Mods) {
        if (Key == AX_KEY_ESCAPE) {
            TheApp->IsRequestingExit = true;
        }
    });
}

static void CreatePerspectiveCamera(AxVec3 Position, AxVec3 Target, AxVec3 Up)
{
    float FOVRadians = 45.0f * (AX_PI / 180.0f);
    float AspectRatio = Viewport.Size.X / Viewport.Size.Y;

    // Create projection matrix - note we're using the Forward matrix
    AxMat4x4Inv PerspectiveProjection = CameraAPI->CalcPerspectiveProjection(
        FOVRadians, AspectRatio, 0.1f, 100.0f);

    PerspCamera.ViewMatrix = LookAt(Position, Target, Up);
    PerspCamera.ProjectionMatrix = PerspectiveProjection;
}

static void CreateOrthographicCamera(AxVec3 Position, AxVec3 Target, AxVec3 Up, float ZoomFactor = 1.0f)
{
    // Calculate the aspect ratio of our viewport
    float aspect = DiffuseViewport.Size.X / DiffuseViewport.Size.Y;

    // Base size of the view volume (before zoom)
    float baseHeight = 2.0f;  // This gives us a 2x2 unit view volume at zoom = 1
    float baseWidth = baseHeight * aspect;

    // Apply zoom factor - larger zoom factor means smaller view volume
    float height = baseHeight / ZoomFactor;
    float width = baseWidth / ZoomFactor;

    // Set up the orthographic bounds
    float Left   = -width * 0.5f;
    float Right  =  width * 0.5f;
    float Bottom = -height * 0.5f;
    float Top    =  height * 0.5f;
    float Near   =  0.1f;
    float Far    =  10.0f;

    AxMat4x4Inv OrthographicProjection = CameraAPI->CalcOrthographicProjection(Left, Right, Bottom, Top, Near, Far);

    DiffuseCamera.ViewMatrix = LookAt(Position, Target, Up);
    DiffuseCamera.ProjectionMatrix = OrthographicProjection;
}

static AxWindow *CreateWindow()
{
    int32_t StyleFlags = AX_WINDOW_STYLE_VISIBLE | AX_WINDOW_STYLE_DECORATED | AX_WINDOW_STYLE_RESIZABLE;
    AxWindow *Window = WindowAPI->CreateWindow(
        "AxonSDK OpenGL Example",
        100,
        100,
        static_cast<int32_t>(WINDOW_SIZE.Width),
        static_cast<int32_t>(WINDOW_SIZE.Height),
        (AxWindowStyle)StyleFlags
    );

    enum AxCursorMode CursorMode = AX_CURSOR_NORMAL;
    WindowAPI->SetCursorMode(Window, CursorMode);

    return (Window);
}

static AxShaderData *ConstructShader(const char *VertexShaderPath, const char *FragmentShaderPath)
{
    // Load shaders
    AxFile VertexShader = FileAPI->OpenForRead(VertexShaderPath);
    AXON_ASSERT(FileAPI->IsValid(VertexShader) && "Vertex shader file size is invalid!");
    AxFile FragmentShader = FileAPI->OpenForRead(FragmentShaderPath);
    AXON_ASSERT(FileAPI->IsValid(FragmentShader) && "Fragment shader file size is invalid!");

    uint64_t VertexShaderFileSize = FileAPI->Size(VertexShader);
    uint64_t FragmentShaderFileSize = FileAPI->Size(FragmentShader);

    // Allocate text buffers
    char *VertexShaderBuffer = (char *)malloc(VertexShaderFileSize + 1);
    char *FragmentShaderBuffer = (char *)malloc(FragmentShaderFileSize + 1);

    // Read shader text
    FileAPI->Read(VertexShader, VertexShaderBuffer, (uint32_t)VertexShaderFileSize);
    FileAPI->Read(FragmentShader, FragmentShaderBuffer, (uint32_t)FragmentShaderFileSize);

    // Null-terminate
    VertexShaderBuffer[VertexShaderFileSize] = '\0';
    FragmentShaderBuffer[FragmentShaderFileSize] = '\0';

    // Create shader program
    uint32_t ShaderProgramID = RenderAPI->CreateProgram("", (char *)VertexShaderBuffer, (char *)FragmentShaderBuffer);
    AXON_ASSERT(ShaderProgramID && "ShaderProgram is NULL!");

    // Free text buffers
    free(VertexShaderBuffer);
    free(FragmentShaderBuffer);

    // Setup the shader data
    AxShaderData *ShaderData = (struct AxShaderData *)calloc(1, sizeof(AxShaderData));
    ShaderData->ShaderHandle = ShaderProgramID;

    // This will go away after I make the changes found in the TODO in GetAttribLocations
    bool Result = RenderAPI->GetAttributeLocations(ShaderProgramID, ShaderData);
    AXON_ASSERT(Result && "Failed to get shader attribute locations!");

    return ShaderData;
}

static bool LoadTexture(AxTexture *Texture, cgltf_texture_view TextureView, cgltf_data *ModelData, const char *BasePath)
{
    if (!TextureView.texture || !TextureView.texture->image) {
        return (false);
    }

    cgltf_image *Image = TextureView.texture->image;

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
        const stbi_uc* image_ptr = (const stbi_uc*)Buffer->data + BufferView->offset;

        // Use stb_image to load from memory
        stbi_set_flip_vertically_on_load(true);
        Pixels = stbi_load_from_memory(image_ptr, (int32_t)BufferView->size, &Width, &Height, &Channels, 0);
        if (!Pixels) {
            fprintf(stderr, "Error: Failed to load embedded image");
            return (false);
        }
    } else {
        fprintf(stderr, "Warning: Image '%s' has neither URI nor buffer view.\n", Image->name ? Image->name : "Unnamed");
        return (false);
    }

    Texture->Width = Width;
    Texture->Height = Height;
    Texture->Channels = Channels;

    RenderAPI->InitTexture(Texture, Pixels);
    RenderAPI->BindTexture(Texture, Texture->ID);

    free(Pixels);

    return (true);
}

static bool LoadTexture2(AxTexture *Texture, const char *Path)
{
    int32_t Width, Height, Channels;
    stbi_uc *Pixels = stbi_load(Path, &Width, &Height, &Channels, 0);
    if (!Pixels) {
        return (false);
    }

    Texture->Width = Width;
    Texture->Height = Height;
    Texture->Channels = Channels;

    RenderAPI->InitTexture(Texture, Pixels);
    RenderAPI->BindTexture(Texture, Texture->ID);

    free(Pixels);

    return (true);
}

void LoadFloatAttribute(cgltf_accessor* Accessor, std::vector<float>& AttributeData, const size_t Components) {
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

bool LoadModel(std::string_view File, AxModel *Model, AxMesh *Mesh)
{
    cgltf_options Options = {};
    cgltf_data* ModelData = nullptr;
    cgltf_result Result = cgltf_parse_file(&Options, File.data(), &ModelData);
    if (Result != cgltf_result_success) {
        return false;
    }

    Result = cgltf_load_buffers(&Options, ModelData, File.data());
    if (Result != cgltf_result_success) {
        cgltf_free(ModelData);
        return false;
    }

    const char *BasePath = PlatformAPI->PathAPI->BasePath(File.data());
    size_t NumMaterials = ModelData->materials_count;

    std::unordered_map<std::string, size_t> TextureIDs;
    std::vector<AxVertex> VertexData;
    std::vector<uint32_t> IndexData;

    for (cgltf_size i = 0; i < ModelData->materials_count; ++i)
    {
        AxTexture Texture = {.ID = 1};  // Create temporary texture
        const auto& Material = ModelData->materials[i];
        const cgltf_pbr_metallic_roughness* pbr = &Material.pbr_metallic_roughness;

        // Load Base Color Texture
        if (pbr->base_color_texture.texture) {
            if (LoadTexture(&Texture, pbr->base_color_texture, ModelData, BasePath)) {
                ArrayPush(Model->Textures, Texture);  // Only push if load successful
            } else {
                return (false);
            }
        }
    }

    // Iterate over meshes and primitives
    for (int i = 0; i < ModelData->meshes_count; ++i) {
        cgltf_mesh* mesh = &ModelData->meshes[i];
        for (int j = 0; j < mesh->primitives_count; ++j) {
            cgltf_primitive* primitive = &mesh->primitives[j];

            // Temporary vectors to hold attribute data
            std::vector<float> PositionData;
            std::vector<float> NormalData;
            std::vector<float> TexCoordData;

            // Load all attributes first
            for (int k = 0; k < primitive->attributes_count; ++k) {
                cgltf_attribute* attribute = &primitive->attributes[k];
                cgltf_accessor* accessor = attribute->data;

                if (attribute->type == cgltf_attribute_type_position) {
                    LoadFloatAttribute(accessor, PositionData, 3);
                } else if (attribute->type == cgltf_attribute_type_normal) {
                    LoadFloatAttribute(accessor, NormalData, 3);
                } else if (attribute->type == cgltf_attribute_type_texcoord) {
                    LoadFloatAttribute(accessor, TexCoordData, 2);
                }
            }

            // Create vertices combining all attributes
            size_t vertexCount = PositionData.size() / 3;
            VertexData.clear();
            for (size_t i = 0; i < vertexCount; ++i) {
                AxVertex Vertex = {};

                // Position (always present)
                Vertex.Position = {
                    PositionData[i * 3],
                    PositionData[i * 3 + 1],
                    PositionData[i * 3 + 2]
                };

                // Normal (if present)
                if (!NormalData.empty()) {
                    Vertex.Normal = {
                        NormalData[i * 3],
                        NormalData[i * 3 + 1],
                        NormalData[i * 3 + 2]
                    };
                }

                // TexCoord (if present)
                if (!TexCoordData.empty()) {
                    Vertex.TexCoord = {
                        TexCoordData[i * 2],
                        TexCoordData[i * 2 + 1]
                    };
                }

                VertexData.push_back(Vertex);
            }

            // Load indices
            IndexData.clear();
            if (primitive->indices) {
                cgltf_accessor* accessor = primitive->indices;
                for (cgltf_size i = 0; i < accessor->count; ++i) {
                    uint32_t index = (uint32_t)cgltf_accessor_read_index(accessor, i);
                    IndexData.push_back(index);
                }
            } else {
                // If no indices, create sequential indices
                for (size_t i = 0; i < vertexCount; ++i) {
                    IndexData.push_back((uint32_t)i);
                }
            }

            // Debug print
            printf("Loaded mesh with %zu vertices and %zu indices\n", VertexData.size(), IndexData.size());

            // Create the mesh
            AxMesh Mesh;
            RenderAPI->InitMesh(&Mesh, VertexData.data(), IndexData.data(),
                                (uint32_t)VertexData.size(), (uint32_t)IndexData.size());
            ArrayPush(Model->Meshes, Mesh);
        }
    }

    cgltf_free(ModelData);
    return true;
}

static bool ConstructObject()
{
    // Create a simple monkey model
    AxMesh MonkeyMesh = {};
    AxModel MonkeyModel = {};
    if (!LoadModel("models/suzanne.glb", &MonkeyModel, &MonkeyMesh)) {
        fprintf(stderr, "Failed to load monkey model!\n");
        return (false);
    }
    // Create a simple rectangle mesh
    AxMesh DiffuseMesh = {};
    AxModel DiffuseModel = {};
    AxTexture DiffuseTexture = { .ID = 2 };
    if (!LoadTexture2(&DiffuseTexture, "textures/TCom_Rock_CliffVolcanic_1K_albedo.png")) {
        fprintf(stderr, "Failed to load diffuse texture!\n");
        return (false);
    }

    // Create a simple rectangle mesh
    AxVertex RectVertices[] = {
        // Position                     Normal                  TexCoord
        {{-0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}}, // Bottom left
        {{ 0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}}, // Bottom right
        {{ 0.5f, 0.0f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}}, // Top right
        {{-0.5f, 0.0f,  0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}}  // Top left
    };

    AxDrawIndex RectIndices[] = {
        0, 1, 2,  // First triangle
        2, 3, 0   // Second triangle
    };

    RenderAPI->InitMesh(&DiffuseMesh, RectVertices, RectIndices, 4, 6);
    ArrayPush(DiffuseModel.Textures, DiffuseTexture);
    ArrayPush(DiffuseModel.Meshes, DiffuseMesh);

    Meshes["Diffuse"] = DiffuseMesh;
    Models["Diffuse"] = DiffuseModel;
    Meshes["Monkey"] = MonkeyMesh;
    Models["Monkey"] = MonkeyModel;

    return (true);
}

static void ConstructViewport()
{
    Viewport.Position = { 0.0f, 0.0f };
    Viewport.Size = { WINDOW_SIZE.Width, WINDOW_SIZE.Height};
    Viewport.Scale = { 1.0f, 1.0f };
    Viewport.Depth = { 0.0f, 1.0f };
    Viewport.IsActive = true;
    Viewport.ClearColor = { 0.42f, 0.51f, 0.54f, 0.0f };

    float ViewportSize = std::min(WINDOW_SIZE.Width / 5.0f, WINDOW_SIZE.Height / 5.0f);
    DiffuseViewport.Position = { 0.0f, WINDOW_SIZE.Height - ViewportSize };
    DiffuseViewport.Size = { ViewportSize, ViewportSize };
    DiffuseViewport.Scale = { 1.0f, 1.0f };
    DiffuseViewport.Depth = { 0.0f, 1.0f };
    DiffuseViewport.IsActive = true;
    DiffuseViewport.ClearColor = { 0.2f, 0.2f, 0.2f, 1.0f };
}

static void ConstructScene()
{
    // Create and initialize the draw data
    DrawData = (struct AxDrawData *)calloc(1, sizeof(AxDrawData));
    DrawData->Valid = true;
}

static AxApplication *Create(int argc, char **argv)
{
    // Attempt to load plugins
    AxWallClock DLLLoadStartTime = PlatformAPI->TimeAPI->WallTime();
    if (!PluginAPI->Load("libAxWindow.dll", false)) {
        fprintf(stderr, "Failed to load AxWindow!\n");
        return (nullptr);
    }

    if (!PluginAPI->Load("libAxOpenGL.dll", false)) {
        fprintf(stderr, "Failed to load AxOpenGL!\n");
        return (nullptr);
    }

    AxWallClock DLLLoadEndTime = PlatformAPI->TimeAPI->WallTime();
    float DLLLoadElapsedTime = PlatformAPI->TimeAPI->ElapsedWallTime(DLLLoadStartTime, DLLLoadEndTime);
    printf("DLL Load %f\n", DLLLoadElapsedTime);

    // Create application
    TheApp = new AxApplication();
    TheApp->APIRegistry = RegistryAPI;
    TheApp->WindowAPI = WindowAPI;
    TheApp->PluginAPI = PluginAPI;

    // Create default window
    WindowAPI->Init();

    TheApp->Window = CreateWindow();
    AXON_ASSERT(TheApp->Window && "Window is NULL!");

    RegisterCallbacks();

    // Initialize the renderer and setup backend
    RenderAPI->CreateContext(TheApp->Window);

    ConstructViewport();

    auto Info = RenderAPI->GetInfo(true);
    printf("Vendor: %s\n", Info.Vendor);
    printf("Renderer: %s\n", Info.Renderer);
    printf("GL Version: %s\n", Info.GLVersion);
    printf("GLSL Version: %s\n", Info.GLSLVersion);

    ModelShaderData = ConstructShader("shaders/vert.glsl", "shaders/frag.glsl");
    DiffuseShaderData = ConstructShader("shaders/ortho.glsl", "shaders/frag.glsl");

    ConstructScene();

    AxVec3 CameraPos = { 0.0f, 0.0f, 8.0f };
    AxVec3 CameraTarget = { 0.0f, 0.0f, 0.0f };
    AxVec3 Up = { 0.0f, 1.0f, 0.0f };
    CreatePerspectiveCamera(CameraPos, CameraTarget, Up);

    // Position the orthographic camera directly above looking straight down
    AxVec3 OrthoCameraPos = { 0.0f, 1.0f, 0.0f };
    AxVec3 OrthoCameraTarget = { 0.0f, 0.0f, 0.0f };
    AxVec3 OrthoUp = { 0.0f, 0.0f, -1.0f };  // Up vector points towards negative Z for proper top-down orientation
    CreateOrthographicCamera(OrthoCameraPos, OrthoCameraTarget, OrthoUp, 1.0f);  // Start with default 1.0 zoom

    // Create scene
    if (!ConstructObject()) {
        fprintf(stderr, "Failed to construct object!\n");
        return (nullptr);
    }

    WindowAPI->SetWindowVisible(TheApp->Window, true);

    return (TheApp);
}

static void RenderModel(AxModel* Model, AxMesh* Mesh, AxCamera* Camera, struct AxShaderData* ShaderData, struct AxViewport* Viewport) 
{
    // Set shader uniforms for this specific model
    RenderAPI->SetUniform(ShaderData, "view", &Camera->ViewMatrix);
    RenderAPI->SetUniform(ShaderData, "projection", &Camera->ProjectionMatrix.Forward);

    // Use the model's transform if available, otherwise use identity
    AxMat4x4 ModelMatrix = Model->Transforms ? Model->Transforms[0] : Identity();
    RenderAPI->SetUniform(ShaderData, "model", &ModelMatrix);

    // Set lighting uniforms (these could also be moved to a global lighting state)
    AxVec3 LightPos = { 5.0f, 5.0f, 5.0f };
    AxVec3 LightColor = { 1.0f, 1.0f, 1.0f };
    RenderAPI->SetUniform(ShaderData, "LightPos", &LightPos);
    RenderAPI->SetUniform(ShaderData, "LightColor", &LightColor);

    // Set material properties
    AxVec3 Color = { 1.0f, 1.0f, 1.0f };
    RenderAPI->SetUniform(ShaderData, "color", &Color);

    // Bind texture if available
    if (Model->Textures) {
        RenderAPI->BindTexture(&Model->Textures[0], 0);
        RenderAPI->SetUniform(ShaderData, "Texture", 0);
    }

    // Render the mesh
    RenderAPI->Render(Viewport, Mesh, ShaderData);
}

static bool Tick(struct AxApplication *App)
{
    // Update frame time
    // NOTE(mdeforge): VSync is currently on

    AxWallClock Previous = PlatformAPI->TimeAPI->WallTime();
    float DeltaT = PlatformAPI->TimeAPI->ElapsedWallTime(TheApp->Timing.WallClock, Previous);

    TheApp->Timing.DeltaT = DeltaT;
    TheApp->Timing.WallClock = Previous;
    TheApp->Timing.ElapsedWallTime += DeltaT;

    // Run messsage pump for window
    WindowAPI->PollEvents(TheApp->Window);

    // Check for close
    if (WindowAPI->HasRequestedClose(TheApp->Window) || App->IsRequestingExit) {
        return(true);
    }

    // Start the frame
    RenderAPI->NewFrame();

    // We're not rending the other viewport here yet, questions about ordering -- textures then render?
    // What to do with update()?
    auto MonkeyModel = Models["Monkey"];
    auto DiffuseModel = Models["Diffuse"];

    RenderModel(&MonkeyModel, &MonkeyModel.Meshes[0], &PerspCamera, ModelShaderData, &Viewport);
    RenderModel(&DiffuseModel, &DiffuseModel.Meshes[0], &DiffuseCamera, DiffuseShaderData, &DiffuseViewport);

    RenderAPI->SwapBuffers();

    return(App->IsRequestingExit);
}

static bool Destroy(struct AxApplication *App)
{
    // Clean up resources
    RenderAPI->DestroyProgram(ModelShaderData->ShaderHandle);
    RenderAPI->DestroyProgram(DiffuseShaderData->ShaderHandle);

    // Shutdown renderer
    RenderAPI->DestroyContext();
    WindowAPI->DestroyWindow(TheApp->Window);

    free(TheApp);

    return (true);
}

static struct AxApplicationAPI AxApplicationAPI_ {
    .Create = Create,
    .Tick = Tick,
    .Destroy = Destroy,
};

static struct AxApplicationAPI *AxApplicationAPI = &AxApplicationAPI_;

extern "C" AXON_DLL_EXPORT void LoadPlugin(struct AxAPIRegistry *APIRegistry, bool Load)
{
    if (APIRegistry)
    {
        PluginAPI = (struct AxPluginAPI *)APIRegistry->Get(AXON_PLUGIN_API_NAME);
        WindowAPI = (struct AxWindowAPI *)APIRegistry->Get(AXON_WINDOW_API_NAME);
        RenderAPI = (struct AxOpenGLAPI *)APIRegistry->Get(AXON_OPENGL_API_NAME);
        PlatformAPI = (struct AxPlatformAPI *)APIRegistry->Get(AXON_PLATFORM_API_NAME);
        FileAPI = PlatformAPI->FileAPI;
        RegistryAPI = APIRegistry;

        APIRegistry->Set(AXON_APPLICATION_API_NAME, AxApplicationAPI, sizeof(struct AxApplicationAPI));
    }
}