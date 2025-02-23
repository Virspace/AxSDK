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
#include "Foundation/AxEditorPlugin.h"
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

static constexpr AxVec2 DISPLAY_SIZE = { 1920, 1080 };

// NOTE(mdeforge): These get assigned to in LoadPlugin().
struct AxAPIRegistry *RegistryAPI;
struct AxPluginAPI *PluginAPI;
struct AxWindowAPI *WindowAPI;
struct AxOpenGLAPI *RenderAPI;
struct AxPlatformAPI *PlatformAPI;
struct AxPlatformFileAPI *FileAPI;
static AxVertex *VertexBuffer;
static AxDrawIndex *IndexBuffer;
static AxShaderData *ShaderData;
static AxDrawData *DrawData;
static AxMesh Mesh;

static AxApplication *TheApp;

// TODO(mdeforge): In lieu of having a scene to manage...
static AxModel GlobalModel;
static uint32_t ShaderProgramID;

static void ResetState()
{
    TheApp->Viewer.CameraTransform = {
        .Position = { 0, 0, 0 },
        .Rotation = { 0, 0, 0 },
        .Scale = { 1, 1, 1 }
    };
}

static AxWindow *CreateWindow()
{
    int32_t StyleFlags = AX_WINDOW_STYLE_VISIBLE | AX_WINDOW_STYLE_DECORATED | AX_WINDOW_STYLE_RESIZABLE;
    AxWindow *Window = WindowAPI->CreateWindow(
        "AxonSDK OpenGL Example",
        100,
        100,
        static_cast<int32_t>(DISPLAY_SIZE.Width),
        static_cast<int32_t>(DISPLAY_SIZE.Height),
        (AxWindowStyle)StyleFlags
    );

    enum AxCursorMode CursorMode = AX_CURSOR_NORMAL;
    WindowAPI->SetCursorMode(Window, CursorMode);

    return (Window);
}

static void ConstructShader()
{
    // Load shaders
    AxFile VertShader = FileAPI->OpenForRead("shaders/vert.glsl");
    AXON_ASSERT(FileAPI->IsValid(VertShader) && "Vertex shader file size is invalid!");
    AxFile FragShader = FileAPI->OpenForRead("shaders/frag.glsl");
    AXON_ASSERT(FileAPI->IsValid(FragShader) && "Fragment shader file size is invalid!");

    uint64_t VertFileSize = FileAPI->Size(VertShader);
    uint64_t FragFileSize = FileAPI->Size(FragShader);

    // Allocate text buffers
    char *VertexShaderBuffer = (char *)malloc(VertFileSize + 1);
    char *FragShaderBuffer = (char *)malloc(FragFileSize + 1);

    // Read shader text
    FileAPI->Read(VertShader, VertexShaderBuffer, (uint32_t)VertFileSize);
    FileAPI->Read(FragShader, FragShaderBuffer, (uint32_t)FragFileSize);

    // Null-terminate
    VertexShaderBuffer[VertFileSize] = '\0';
    FragShaderBuffer[FragFileSize] = '\0';

    // Create shader program
    ShaderProgramID = RenderAPI->CreateProgram("", (char *)VertexShaderBuffer, (char *)FragShaderBuffer);
    AXON_ASSERT(ShaderProgramID && "ShaderProgram is NULL!");

    // Free text buffers
    free(VertexShaderBuffer);
    free(FragShaderBuffer);

    // Setup the shader data
    ShaderData = (struct AxShaderData *)calloc(1, sizeof(AxShaderData));
    ShaderData->ShaderHandle = ShaderProgramID;

    // This will go away after I make the changes found in the TODO in GetAttribLocations
    bool Result = RenderAPI->GetAttributeLocations(ShaderProgramID, ShaderData);
    AXON_ASSERT(Result && "Failed to get shader attribute locations!");
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

    if (Pixels) {
        printf("Texture loaded successfully: %dx%d with %d channels\n", 
               Width, Height, Channels);
        printf("First few pixels (RGBA): ");
        for (int i = 0; i < std::min(4, Width * Height * Channels); i += Channels) {
            for (int c = 0; c < Channels; c++) {
                printf("%d ", Pixels[i + c]);
            }
            printf(", ");
        }
        printf("\n");
    } else {
        printf("Failed to load texture pixels!\n");
        return false;
    }

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

bool LoadModel(std::string_view File, AxModel *Model)
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
        AxTexture Texture = {};  // Create temporary texture
        const auto& Material = ModelData->materials[i];
        const cgltf_pbr_metallic_roughness* pbr = &Material.pbr_metallic_roughness;

        // Load Base Color Texture
        if (pbr->base_color_texture.texture) {
            if (LoadTexture(&Texture, pbr->base_color_texture, ModelData, BasePath)) {
                ArrayPush(Model->Textures, Texture);  // Only push if load successful
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
    return (LoadModel("models/suzanne.glb", &GlobalModel));

    //ArrayPush(Model.Meshes, Mesh);
}

static void ConstructScene()
{
    // Create and initialize the draw data
    AxVec2 DisplayPos = { 0.0f, 0.0f };
    AxVec2 DisplaySize = DISPLAY_SIZE;
    AxVec2 FramebufferScale = { 1.0f, 1.0f };
    DrawData = (struct AxDrawData *)calloc(1, sizeof(AxDrawData));
    DrawData->Valid = true;
    DrawData->DisplayPos = DisplayPos;
    DrawData->DisplaySize = DisplaySize;
    DrawData->FramebufferScale = FramebufferScale;
}

static AxApplication *Create(int argc, char **argv)
{
    // bool ShouldPrintUsage = false;
    // for (int32_t i = 1; i < argc; ++i)
    // {
    //     if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
    //         ShouldPrintUsage = true;
    //     }
    // }

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

    ResetState();

    // Create default window
    WindowAPI->Init();

    TheApp->Window = CreateWindow();
    AXON_ASSERT(TheApp->Window && "Window is NULL!");

    // Initialize the renderer and setup backend
    RenderAPI->CreateContext(TheApp->Window);

    auto Info = RenderAPI->GetInfo(true);
    printf("Vendor: %s\n", Info.Vendor);
    printf("Renderer: %s\n", Info.Renderer);
    printf("GL Version: %s\n", Info.GLVersion);
    printf("GLSL Version: %s\n", Info.GLSLVersion);

    ConstructShader();

    ConstructScene();

    // Create scene
    if (!ConstructObject()) {
        fprintf(stderr, "Failed to construct object!\n");
        return (nullptr);
    }

    WindowAPI->SetWindowVisible(TheApp->Window, true);

    return (TheApp);
}

static void Update()
{
    // Create matrices
    float FOVRadians = 45.0f * (AX_PI / 180.0f);
    float AspectRatio = DrawData->DisplaySize.X / DrawData->DisplaySize.Y;

    // Create projection matrix - note we're using the Forward matrix
    AxMat4x4Inv PerspectiveProjection = CameraAPI->CalcPerspectiveProjection(
        FOVRadians, AspectRatio, 0.1f, 100.0f);

    // Create view matrix - move camera back to see object
    AxVec3 CameraPos = { 0.0f, 0.0f, 8.0f };
    AxVec3 CameraTarget = { 0.0f, 0.0f, 0.0f };
    AxVec3 Up = { 0.0f, 1.0f, 0.0f };
    AxMat4x4 ViewMatrix = LookAt(CameraPos, CameraTarget, Up);

    // Create model matrix
    AxMat4x4 ModelMatrix = Identity();
    Translate(ModelMatrix, (AxVec3){ 0.0f, 0.0f, 0.0f });
    //ModelMatrix = MatZRotation(ModelMatrix, 90.0f);
    ModelMatrix = MatXRotation(ModelMatrix, -90.0f);

    // Create color
    AxVec3 Color = { 1.0f, 1.0f, 1.0f };

    // Debug texture binding
    if (GlobalModel.Textures && ArraySize(GlobalModel.Textures) > 0) {
        RenderAPI->BindTexture(&GlobalModel.Textures[0], 0);
        RenderAPI->SetUniform(ShaderData, "Texture", 0);
    } else {
        printf("No textures available to bind!\n");
    }

    RenderAPI->SetUniform(ShaderData, "model", &ModelMatrix);
    RenderAPI->SetUniform(ShaderData, "view", &ViewMatrix);
    RenderAPI->SetUniform(ShaderData, "projection", &PerspectiveProjection.Forward);
    RenderAPI->SetUniform(ShaderData, "color", &Color);

    // Set light properties
    AxVec3 LightPos = { 5.0f, 5.0f, 5.0f };
    AxVec3 LightColor = { 1.0f, 1.0f, 1.0f };
    RenderAPI->SetUniform(ShaderData, "LightPos", &LightPos);
    RenderAPI->SetUniform(ShaderData, "LightColor", &LightColor);
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
    if (WindowAPI->HasRequestedClose(TheApp->Window)) {
        return (false);
    }

    // Start the frame
    RenderAPI->NewFrame();
    Update();

    RenderAPI->Render(DrawData, &Mesh, ShaderData);
    RenderAPI->SwapBuffers();

    return(true);
}

static bool Destroy(struct AxApplication *App)
{
    // Clean up resources
    RenderAPI->DestroyProgram(ShaderProgramID);

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

        // TODO(mdeforge): Can we do this elsewhere? Because if not, we may need a list of plugins to load
        // prior to getting here so we can do this here. I think we may just need to cache the APIRegistry
        // and then we should be good to maintain a list of EnginePlugins *
        //DialogEditorAPI = (struct EditorPlugin *)APIRegistry->Get("AxDialogEditorAPI");

        APIRegistry->Set(AXON_APPLICATION_API_NAME, AxApplicationAPI, sizeof(struct AxApplicationAPI));
    }
}