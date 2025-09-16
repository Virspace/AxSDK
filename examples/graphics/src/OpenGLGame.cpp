#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

#include "AxApplication.h"

#define AXARRAY_IMPLEMENTATION
#include "Foundation/AxArray.h"
#include "Foundation/AxPlugin.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxApplication.h"
#include "Foundation/AxPlatform.h"
#include "Foundation/AxMath.h"
#include "Foundation/AxHashTable.h"

#include "AxWindow/AxWindow.h"
#include "AxOpenGL/AxOpenGL.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Texture wrap modes
#ifndef GL_REPEAT
#define GL_REPEAT 0x2901
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_MIRRORED_REPEAT
#define GL_MIRRORED_REPEAT 0x8370
#endif

// Texture filter modes
#ifndef GL_NEAREST
#define GL_NEAREST 0x2600
#endif
#ifndef GL_LINEAR
#define GL_LINEAR 0x2601
#endif
#ifndef GL_NEAREST_MIPMAP_NEAREST
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#endif
#ifndef GL_LINEAR_MIPMAP_NEAREST
#define GL_LINEAR_MIPMAP_NEAREST 0x2701
#endif
#ifndef GL_NEAREST_MIPMAP_LINEAR
#define GL_NEAREST_MIPMAP_LINEAR 0x2702
#endif
#ifndef GL_LINEAR_MIPMAP_LINEAR
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#endif

static constexpr AxVec2 WINDOW_SIZE = { 1920, 1080 };

// APIs (these get assigned to in LoadPlugin())
struct AxAPIRegistry *RegistryAPI;
struct AxPluginAPI *PluginAPI;
struct AxWindowAPI *WindowAPI;
struct AxOpenGLAPI *RenderAPI;
struct AxPlatformAPI *PlatformAPI;
struct AxPlatformFileAPI *FileAPI;
struct AxHashTableAPI *HashTableAPI;

// Scene
static AxCamera PerspCamera;
static AxTransform PerspTransform;
static AxShaderData *ModelShaderData;

static AxViewport DiffuseViewport;
static AxViewport Viewport;

std::unordered_map<std::string, AxModel> Models;

static AxApplication *TheApp;
static uint64_t FrameCount = 0;


// Camera controls state
static float CameraSpeed = 5.0f;
static float MouseSensitivity = 0.001f;
static AxVec2 LastMousePos = { 0.0f, 0.0f };

static void RegisterCallbacks()
{
    // Use the new callback system with error handling
    AxWindowCallbacks Callbacks = {0};
    enum AxWindowError Error = AX_WINDOW_ERROR_NONE;

    // Set up all callbacks at once
    Callbacks.Key = [](AxWindow *Window, int Key, int ScanCode, int Action, int Mods) {
        if (Key == AX_KEY_ESCAPE && Action == AX_PRESS) {
            TheApp->IsRequestingExit = true;
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
    };

    Callbacks.MousePos = [](AxWindow *Window, double X, double Y) {
        AxVec2 MouseDelta = {
            (float)X - LastMousePos.X,
            (float)Y - LastMousePos.Y
        };

        TransformRotateFromMouseDelta(&PerspTransform, MouseDelta, MouseSensitivity);
        LastMousePos = { (float)X, (float)Y };
    };

    Callbacks.MouseButton = [](AxWindow *Window, int Button, int Action, int Mods) {
        if (Action == AX_PRESS) {
            printf("Mouse button pressed: %d\n", Button);
        }
    };

    Callbacks.Scroll = [](AxWindow *Window, AxVec2 Offset) {
        printf("Scroll: %.2f, %.2f\n", Offset.X, Offset.Y);
    };

    Callbacks.StateChanged = [](AxWindow *Window, enum AxWindowState OldState, enum AxWindowState NewState) {
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
    };

    // Set all callbacks at once
    if (!WindowAPI->SetCallbacks(TheApp->Window, &Callbacks, &Error)) {
        fprintf(stderr, "Failed to set callbacks: %s\n", WindowAPI->GetErrorString(Error));
    }
}

static AxWindow *CreateGameWindow()
{
    // Use the new window config system
    AxWindowConfig Config = {
        .Title = "AxonSDK OpenGL Example",
        .X = 100,
        .Y = 100,
        .Width = static_cast<int32_t>(WINDOW_SIZE.Width),
        .Height = static_cast<int32_t>(WINDOW_SIZE.Height),
        .Style = (AxWindowStyle)(AX_WINDOW_STYLE_VISIBLE | AX_WINDOW_STYLE_DECORATED | AX_WINDOW_STYLE_RESIZABLE)
    };

    enum AxWindowError Error = AX_WINDOW_ERROR_NONE;
    AxWindow *Window = WindowAPI->CreateWindowWithConfig(&Config, &Error);

    if (Error != AX_WINDOW_ERROR_NONE) {
        fprintf(stderr, "Failed to create window: %s\n", WindowAPI->GetErrorString(Error));
        return (nullptr);
    }

    // Set cursor mode to disabled for FPS-style mouse look
    enum AxCursorMode CursorMode = AX_CURSOR_DISABLED;
    if (!WindowAPI->SetCursorMode(Window, CursorMode, &Error)) {
        fprintf(stderr, "Failed to set cursor mode: %s\n", WindowAPI->GetErrorString(Error));
    }

    return (Window);
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
    char *VertexShaderBuffer = new char[VertexShaderFileSize + 1];
    char *FragmentShaderBuffer = new char[FragmentShaderFileSize + 1];

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
    delete[] VertexShaderBuffer;
    delete[] FragmentShaderBuffer;

    // Setup the shader data
    AxShaderData *ShaderData = new AxShaderData{};
    ShaderData->ShaderHandle = ShaderProgramID;

    // This will go away after I make the changes found in the TODO in GetAttribLocations
    bool Result = RenderAPI->GetAttributeLocations(ShaderProgramID, ShaderData);
    AXON_ASSERT(Result && "Failed to get shader attribute locations!");

    return ShaderData;
}

// Helper function to convert GLTF sampler properties to OpenGL constants
static void SetSamplerProperties(AxTexture* texture, cgltf_sampler* sampler) {
    // Default values
    texture->WrapS = GL_REPEAT;
    texture->WrapT = GL_REPEAT;
    texture->MagFilter = GL_LINEAR;
    texture->MinFilter = GL_LINEAR_MIPMAP_LINEAR;

    if (sampler) {
        // Convert GLTF wrap modes to OpenGL constants
        switch (sampler->wrap_s) {
            case 33071: texture->WrapS = GL_CLAMP_TO_EDGE; break;
            case 33648: texture->WrapS = GL_MIRRORED_REPEAT; break;
            case 10497: texture->WrapS = GL_REPEAT; break;
            default: texture->WrapS = GL_REPEAT; break;
        }

        switch (sampler->wrap_t) {
            case 33071: texture->WrapT = GL_CLAMP_TO_EDGE; break;
            case 33648: texture->WrapT = GL_MIRRORED_REPEAT; break;
            case 10497: texture->WrapT = GL_REPEAT; break;
            default: texture->WrapT = GL_REPEAT; break;
        }

        // Convert GLTF filter modes to OpenGL constants
        switch (sampler->mag_filter) {
            case 9728: texture->MagFilter = GL_NEAREST; break;
            case 9729: texture->MagFilter = GL_LINEAR; break;
            default: texture->MagFilter = GL_LINEAR; break;
        }

        switch (sampler->min_filter) {
            case 9728: texture->MinFilter = GL_NEAREST; break;
            case 9729: texture->MinFilter = GL_LINEAR; break;
            case 9984: texture->MinFilter = GL_NEAREST_MIPMAP_NEAREST; break;
            case 9985: texture->MinFilter = GL_LINEAR_MIPMAP_NEAREST; break;
            case 9986: texture->MinFilter = GL_NEAREST_MIPMAP_LINEAR; break;
            case 9987: texture->MinFilter = GL_LINEAR_MIPMAP_LINEAR; break;
            default: texture->MinFilter = GL_LINEAR_MIPMAP_LINEAR; break;
        }
    } else {
        printf("No sampler specified, using defaults\n");
    }
}

static bool LoadModelTexture(AxTexture *Texture, cgltf_texture_view TextureView, cgltf_data *ModelData, const char *BasePath)
{
    if (!TextureView.texture || !TextureView.texture->image) {
        return (false);
    }

    cgltf_image *Image = TextureView.texture->image;
    cgltf_sampler *Sampler = TextureView.texture->sampler;

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

        Pixels = stbi_load_from_memory(image_ptr, (int32_t)BufferView->size, &Width, &Height, &Channels, 0);
        if (!Pixels) {
            const char* stbi_error = stbi_failure_reason();
            fprintf(stderr, "Error: Failed to load embedded image: %s (size: %zu bytes)\n", 
                    stbi_error ? stbi_error : "unknown error", BufferView->size);
            return (false);
        }
    } else {
        fprintf(stderr, "Warning: Image '%s' has neither URI nor buffer view.\n", Image->name ? Image->name : "Unnamed");
        return (false);
    }

    Texture->Width = Width;
    Texture->Height = Height;
    Texture->Channels = Channels;

    // Set sampler properties from GLTF
    SetSamplerProperties(Texture, Sampler);

    uint32_t original_id = Texture->ID;
    RenderAPI->InitTexture(Texture, Pixels);
    RenderAPI->BindTexture(Texture, Texture->ID);

    stbi_image_free(Pixels);  // Use stbi_image_free instead of free for stb_image allocated memory

    return (true);
}

static bool LoadTexture(AxTexture *Texture, const char *Path)
{
    int32_t Width, Height, Channels;
    stbi_uc *Pixels = stbi_load(Path, &Width, &Height, &Channels, 0);
    if (!Pixels) {
        return (false);
    }

    Texture->Width = Width;
    Texture->Height = Height;
    Texture->Channels = Channels;

    // Set default sampler properties (no GLTF sampler available)
    SetSamplerProperties(Texture, nullptr);

    uint32_t original_id = Texture->ID;
    RenderAPI->InitTexture(Texture, Pixels);
    RenderAPI->BindTexture(Texture, Texture->ID);

    stbi_image_free(Pixels);

    return (true);
}

void LoadFloatAttribute(cgltf_accessor* Accessor, std::vector<float>& AttributeData, const size_t Components) {
    size_t total_elements = Accessor->count * Components;
    AttributeData.clear();
    AttributeData.reserve(total_elements);

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

    // Create a mapping from material index to texture array index
    std::unordered_map<size_t, size_t> MaterialToTextureMap;

    // Process all materials
    for (cgltf_size i = 0; i < ModelData->materials_count; ++i)
    {
        AxTexture BaseTexture = {0};
        AxTexture NormalTexture = {0};
        const auto& Material = ModelData->materials[i];
        const cgltf_pbr_metallic_roughness* pbr = &Material.pbr_metallic_roughness;

        // Store material data for rendering
        AxMaterial AxMat = {0};
        AxMat.BaseColorFactor[0] = pbr->base_color_factor[0];
        AxMat.BaseColorFactor[1] = pbr->base_color_factor[1];
        AxMat.BaseColorFactor[2] = pbr->base_color_factor[2];
        AxMat.BaseColorFactor[3] = pbr->base_color_factor[3];
        snprintf(AxMat.Name, sizeof(AxMat.Name), "%s", Material.name ? Material.name : "Unnamed");

        // Load Base Color Texture
        if (pbr->base_color_texture.texture) {
            if (LoadModelTexture(&BaseTexture, pbr->base_color_texture, ModelData, BasePath)) {
                size_t textureIndex = ArraySize(Model->Textures);
                ArrayPush(Model->Textures, BaseTexture);  // Only push if load successful
                MaterialToTextureMap[i] = textureIndex; // Map material index to texture array index
                AxMat.BaseColorTexture = (uint32_t)textureIndex;
            } else {
                printf("Failed to load base color texture for material %zu - continuing without this texture\n", i);
                AxMat.BaseColorTexture = 0;
            }
        } else {
            AxMat.BaseColorTexture = 0;
        }

        // Load Normal Texture
        if (Material.normal_texture.texture) {
            if (LoadModelTexture(&NormalTexture, Material.normal_texture, ModelData, BasePath)) {
                size_t normalTextureIndex = ArraySize(Model->Textures);
                ArrayPush(Model->Textures, NormalTexture);
                AxMat.NormalTexture = (uint32_t)normalTextureIndex;
            } else {
                printf("Failed to load normal texture for material %zu - continuing without normal mapping\n", i);
                AxMat.NormalTexture = 0;
            }
        } else {
            AxMat.NormalTexture = 0;
        }

        ArrayPush(Model->Materials, AxMat);
    }

    // First, build node transforms accounting for parent-child hierarchy
    std::vector<AxMat4x4> nodeTransforms(ModelData->nodes_count);
    std::vector<bool> nodeProcessed(ModelData->nodes_count, false);

    // Function to recursively compute node transform with parent chain
    std::function<AxMat4x4(cgltf_size)> computeNodeTransform = [&](cgltf_size nodeIndex) -> AxMat4x4 {
        if (nodeIndex >= ModelData->nodes_count) {
            printf("ERROR: Invalid nodeIndex %d (max %d)\n", (int)nodeIndex, (int)ModelData->nodes_count);
            return Identity();
        }

        if (nodeProcessed[nodeIndex]) {
            return nodeTransforms[nodeIndex];
        }

        cgltf_node* node = &ModelData->nodes[nodeIndex];
        const char* nodeName = node->name ? node->name : "";
        AxMat4x4 localTransform;

        // Build local transform
        if (node->has_matrix) {
            memcpy(&localTransform, node->matrix, sizeof(AxMat4x4));
        } else {
            // Build from TRS
            AxVec3 translation = node->has_translation ?
                (AxVec3){node->translation[0], node->translation[1], node->translation[2]} :
                (AxVec3){0.0f, 0.0f, 0.0f};

            AxQuat rotation = node->has_rotation ?
                (AxQuat){node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3]} :
                QuatIdentity();

            AxVec3 scale = node->has_scale ?
                (AxVec3){node->scale[0], node->scale[1], node->scale[2]} :
                Vec3One();


            // R * S + direct translation
            AxMat4x4 R = QuatToMat4x4(rotation);
            AxMat4x4 S = Identity();
            S.E[0][0] = scale.X;
            S.E[1][1] = scale.Y;
            S.E[2][2] = scale.Z;

            AxMat4x4 RS = Mat4x4Mul(R, S);
            localTransform = RS;

            // Apply translation directly
            localTransform.E[3][0] = translation.X;
            localTransform.E[3][1] = translation.Y;
            localTransform.E[3][2] = translation.Z;
            localTransform.E[3][3] = 1.0f;
        }

        // Find parent and multiply: ParentMatrix * LocalMatrix
        AxMat4x4 finalTransform = localTransform;
        cgltf_size parentIndex = (cgltf_size)-1;

        // Search for parent node (node that has this nodeIndex in its children)
        for (cgltf_size i = 0; i < ModelData->nodes_count; ++i) {
            cgltf_node* potentialParent = &ModelData->nodes[i];
            for (cgltf_size childIdx = 0; childIdx < potentialParent->children_count; ++childIdx) {
                // CGLTF children array contains pointers to child nodes
                // Calculate the index by subtracting base address
                if (potentialParent->children[childIdx] >= ModelData->nodes && 
                    potentialParent->children[childIdx] < ModelData->nodes + ModelData->nodes_count) {
                    cgltf_size childNodeIndex = potentialParent->children[childIdx] - ModelData->nodes;
                    if (childNodeIndex == nodeIndex) {
                        parentIndex = i;
                        break;
                    }
                }
            }

            if (parentIndex != (cgltf_size)-1)
                break;
        }

        if (parentIndex != (cgltf_size)-1) {
            if (parentIndex == nodeIndex) {
                // Node trying to be its own parent
                finalTransform = localTransform;
            } else {
                AxMat4x4 parentTransform = computeNodeTransform(parentIndex);
                finalTransform = Mat4x4Mul(parentTransform, localTransform);
            }
        }

        nodeTransforms[nodeIndex] = finalTransform;
        nodeProcessed[nodeIndex] = true;
        return finalTransform;
    };

    // Process all nodes to build final transforms with hierarchy
    for (cgltf_size nodeIndex = 0; nodeIndex < ModelData->nodes_count; ++nodeIndex) {
        AxMat4x4 transform = computeNodeTransform(nodeIndex);
        ArrayPush(Model->Transforms, transform);
    }

    // Iterate over meshes and primitives
    for (cgltf_size i = 0; i < ModelData->meshes_count; ++i) {
        cgltf_mesh* mesh = &ModelData->meshes[i];
        const char* meshName = mesh->name ? mesh->name : "unnamed";

        for (cgltf_size j = 0; j < mesh->primitives_count; ++j) {
            cgltf_primitive* primitive = &mesh->primitives[j];

            // Modern C++ containers for attribute data
            std::vector<float> PositionData;
            std::vector<float> NormalData;
            std::vector<float> TexCoordData0;  // TEXCOORD_0
            std::vector<float> TexCoordData1;  // TEXCOORD_1

            // Load all attributes first
            for (cgltf_size k = 0; k < primitive->attributes_count; ++k) {
                cgltf_attribute* attribute = &primitive->attributes[k];
                cgltf_accessor* accessor = attribute->data;

                if (attribute->type == cgltf_attribute_type_position) {
                    size_t elementCount = accessor->count * 3;
                    PositionData.clear();
                    PositionData.reserve(elementCount);

                    for (cgltf_size idx = 0; idx < accessor->count; ++idx) {
                        float values[3];
                        cgltf_accessor_read_float(accessor, idx, values, 3);
                        PositionData.push_back(values[0]);
                        PositionData.push_back(values[1]);
                        PositionData.push_back(values[2]);
                    }
                } else if (attribute->type == cgltf_attribute_type_normal) {
                    size_t elementCount = accessor->count * 3;
                    NormalData.clear();
                    NormalData.reserve(elementCount);

                    for (cgltf_size idx = 0; idx < accessor->count; ++idx) {
                        float values[4] = {0};
                        cgltf_accessor_read_float(accessor, idx, values, 3);

                        NormalData.push_back(values[0]);
                        NormalData.push_back(values[1]);
                        NormalData.push_back(values[2]);
                    }
                } else if (attribute->type == cgltf_attribute_type_texcoord) {
                    // Check which TEXCOORD set this is (0, 1, etc.)
                    std::vector<float>* targetData = nullptr;

                    if (attribute->index == 0) {
                        targetData = &TexCoordData0;
                    } else if (attribute->index == 1) {
                        targetData = &TexCoordData1;
                    } else {
                        continue;
                    }

                    size_t elementCount = accessor->count * 2;
                    targetData->clear();
                    targetData->reserve(elementCount);

                    for (cgltf_size idx = 0; idx < accessor->count; ++idx) {
                        float values[2];
                        cgltf_accessor_read_float(accessor, idx, values, 2);
                        targetData->push_back(values[0]);
                        targetData->push_back(values[1]);
                    }
                }
            }

            // Determine which TEXCOORD set to use based on material
            int texcoordSetToUse = 0;  // Default to TEXCOORD_0
            if (primitive->material) {
                const cgltf_pbr_metallic_roughness* pbr = &primitive->material->pbr_metallic_roughness;
                if (pbr->base_color_texture.texture) {
                    texcoordSetToUse = pbr->base_color_texture.texcoord;
                }
            }

            // Select the appropriate texture coordinate data
            const std::vector<float>* TexCoordDataToUse = nullptr;
            if (texcoordSetToUse == 0 && !TexCoordData0.empty()) {
                TexCoordDataToUse = &TexCoordData0;
            } else if (texcoordSetToUse == 1 && !TexCoordData1.empty()) {
                TexCoordDataToUse = &TexCoordData1;
            } else {
                TexCoordDataToUse = &TexCoordData0;  // Fallback
                if (TexCoordData0.empty()) {
                    printf("WARNING: No texture coordinates available for mesh %s!\n", meshName);
                }
            }

            // Create vertices combining all attributes
            size_t vertexCount = PositionData.size() / 3;

            VertexData.clear();
            VertexData.resize(vertexCount);

            // First pass: create basic vertices
            for (size_t i = 0; i < vertexCount; ++i) {
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

                // TexCoord (if present)
                if (TexCoordDataToUse && !TexCoordDataToUse->empty()) {
                    Vertex.TexCoord = {
                        (*TexCoordDataToUse)[i * 2],
                        (*TexCoordDataToUse)[i * 2 + 1]
                    };
                }

                // Initialize tangent vector (XYZ = tangent, W = handedness)
                Vertex.Tangent = { 0.0f, 0.0f, 0.0f, 1.0f };

                VertexData[i] = Vertex;
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

            // Calculate tangent and bitangent vectors for normal mapping
            if (TexCoordDataToUse && !TexCoordDataToUse->empty() && IndexData.size() >= 3) {
                // Process triangles to calculate tangent space
                for (size_t i = 0; i < IndexData.size(); i += 3) {
                    uint32_t i0 = IndexData[i];
                    uint32_t i1 = IndexData[i + 1];
                    uint32_t i2 = IndexData[i + 2];

                    if (i0 < vertexCount && i1 < vertexCount && i2 < vertexCount) {
                        AxVec3 tangent, bitangent;
                        CalculateTangentBitangent(
                            VertexData[i0].Position, VertexData[i1].Position, VertexData[i2].Position,
                            VertexData[i0].TexCoord, VertexData[i1].TexCoord, VertexData[i2].TexCoord,
                            &tangent, &bitangent
                        );

                        // Add tangent to all vertices of this triangle
                        // (We'll average them later for smooth results and calculate handedness)
                        AxVec3 currentTangent0 = { VertexData[i0].Tangent.X, VertexData[i0].Tangent.Y, VertexData[i0].Tangent.Z };
                        AxVec3 currentTangent1 = { VertexData[i1].Tangent.X, VertexData[i1].Tangent.Y, VertexData[i1].Tangent.Z };
                        AxVec3 currentTangent2 = { VertexData[i2].Tangent.X, VertexData[i2].Tangent.Y, VertexData[i2].Tangent.Z };

                        AxVec3 newTangent0 = Vec3Add(currentTangent0, tangent);
                        AxVec3 newTangent1 = Vec3Add(currentTangent1, tangent);
                        AxVec3 newTangent2 = Vec3Add(currentTangent2, tangent);

                        VertexData[i0].Tangent = { newTangent0.X, newTangent0.Y, newTangent0.Z, VertexData[i0].Tangent.W };
                        VertexData[i1].Tangent = { newTangent1.X, newTangent1.Y, newTangent1.Z, VertexData[i1].Tangent.W };
                        VertexData[i2].Tangent = { newTangent2.X, newTangent2.Y, newTangent2.Z, VertexData[i2].Tangent.W };
                    }
                }

                // Normalize tangent vectors and calculate handedness
                for (size_t i = 0; i < vertexCount; ++i) {
                    AxVec3 tangent = { VertexData[i].Tangent.X, VertexData[i].Tangent.Y, VertexData[i].Tangent.Z };
                    AxVec3 normal = VertexData[i].Normal;

                    if (Vec3Length(tangent) > 0.0f) {
                        // Normalize the tangent
                        tangent = Vec3Normalize(tangent);

                        // Calculate the bitangent using cross product
                        AxVec3 computedBitangent = Vec3Cross(normal, tangent);

                        // Calculate handedness by comparing with the original bitangent
                        // For this implementation, we'll assume right-handed coordinate system
                        // In a full implementation, you'd calculate this based on UV mapping direction
                        float handedness = 1.0f; // Default to right-handed

                        // Store normalized tangent with handedness
                        VertexData[i].Tangent = { tangent.X, tangent.Y, tangent.Z, handedness };
                    }
                }
            }

            // Create the mesh
            AxMesh Mesh = {0};
            RenderAPI->InitMesh(&Mesh, VertexData.data(), IndexData.data(),
                                (uint32_t)VertexData.size(), (uint32_t)IndexData.size());

            // Store the mesh name for debugging
            snprintf(Mesh.Name, sizeof(Mesh.Name), "%s_p%d", meshName, static_cast<int32_t>(j));

            // Store material reference for later use during rendering
            Mesh.MaterialIndex = 0; // Default material index
            Mesh.BaseColorTexture = 0;
            Mesh.NormalTexture = 0;

            if (primitive->material) {
                // Find the material index in the materials array
                for (cgltf_size matIndex = 0; matIndex < ModelData->materials_count; ++matIndex) {
                    if (primitive->material == &ModelData->materials[matIndex]) {
                        Mesh.MaterialIndex = (uint32_t)matIndex;

                        // Set base color texture
                        if (MaterialToTextureMap.find(matIndex) != MaterialToTextureMap.end()) {
                            Mesh.BaseColorTexture = (uint32_t)MaterialToTextureMap[matIndex];
                        }

                        // Set normal texture from the material
                        if (matIndex < ArraySize(Model->Materials)) {
                            Mesh.NormalTexture = Model->Materials[matIndex].NormalTexture;
                        }
                        break;
                    }
                }
            }

            // Find which node references this mesh and set the transform index
            Mesh.TransformIndex = 0;
            bool foundNode = false;
            for (cgltf_size nodeIndex = 0; nodeIndex < ModelData->nodes_count; ++nodeIndex) {
                cgltf_node* node = &ModelData->nodes[nodeIndex];
                if (node->mesh && node->mesh == &ModelData->meshes[i]) {
                    if (!foundNode) {
                        Mesh.TransformIndex = (uint32_t)nodeIndex;
                        foundNode = true;
                    }
                }
            }

            ArrayPush(Model->Meshes, Mesh);
        }
    }

    cgltf_free(ModelData);
    return true;
}

static bool CreateScene()
{
    // Create a scene model
    AxModel SceneModel = {};
    if (!LoadModel("models/SponzaAtrium.glb", &SceneModel)) {
        fprintf(stderr, "Failed to load model!\n");
        return (false);
    }

    Models["Scene"] = SceneModel;

    // Position and rotate the camera to see the scene
    PerspTransform = {
        .Translation = (AxVec3) { 11.12f, 1.7f, 1.83f },
        .Rotation = QuatFromEuler((AxVec3) {
            -2.06 * (AX_PI / 180.0f),
            76.10 * (AX_PI / 180.0f),
            0.0
        }),
        .Scale = Vec3One(),
        .Up = Vec3Up()
    };

    // Create the camera
    RenderAPI->CreateCamera(&PerspCamera);
    RenderAPI->CameraSetFOV(&PerspCamera, 60.0f * (AX_PI / 180.0f));  // Wider FOV
    RenderAPI->CameraSetAspectRatio(&PerspCamera, Viewport.Size.X / Viewport.Size.Y);
    RenderAPI->CameraSetNearClipPlane(&PerspCamera, 0.1f);   // Near clip plane
    RenderAPI->CameraSetFarClipPlane(&PerspCamera, 100.0f);  // Far clip plane

    return (true);
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

    // Validate platform before proceeding
    enum AxWindowError Error;

    Error = AX_WINDOW_ERROR_NONE;
    if (!WindowAPI->ValidatePlatform(&Error)) {
        fprintf(stderr, "Platform validation failed: %s\n", WindowAPI->GetErrorString(Error));
        return (nullptr);
    }

    // Get platform info
    AxPlatformInfo PlatformInfo;
    memset(&PlatformInfo, 0, sizeof(PlatformInfo));
    if (WindowAPI->GetPlatformInfo(&PlatformInfo, &Error)) {
        printf("Platform: %s %s\n", PlatformInfo.Name, PlatformInfo.Version);
        printf("Features: 0x%08X\n", PlatformInfo.Features);
    }

    // Set platform hints for better performance
    AxPlatformHints Hints = {0};
    Hints.Windows.EnableCompositor = true;
    Hints.Windows.UseImmersiveDarkMode = false;

    if (!WindowAPI->SetPlatformHints(&Hints, &Error)) {
        fprintf(stderr, "Failed to set platform hints: %s\n", WindowAPI->GetErrorString(Error));
        // Continue anyway, this is not critical
    }

    TheApp->Window = CreateGameWindow();
    AXON_ASSERT(TheApp->Window && "Window is NULL!");
    RegisterCallbacks();
    ConstructViewport();

    // Initialize the renderer and setup backend
    RenderAPI->CreateContext(TheApp->Window);

    // Print render info
    auto Info = RenderAPI->GetInfo(true);
    printf("Vendor: %s\n", Info.Vendor);
    printf("Renderer: %s\n", Info.Renderer);
    printf("GL Version: %s\n", Info.GLVersion);
    printf("GLSL Version: %s\n", Info.GLSLVersion);

    // Create shaders
    ModelShaderData = ConstructShader("shaders/vert.glsl", "shaders/frag.glsl");

    // Create scene
    if (!CreateScene()) {
        fprintf(stderr, "Failed to construct object!\n");
        return (nullptr);
    }

    WindowAPI->SetWindowVisible(TheApp->Window, true, NULL);

    return (TheApp);
}

static void RenderModelWithMeshes(AxModel* Model, AxCamera* Camera, AxTransform *CameraTransform, struct AxShaderData* ShaderData, struct AxViewport* Viewport)
{
    // Set up shared state once
    AxMat4x4 ViewMatrix = CreateViewMatrix(CameraTransform);
    AxMat4x4 ProjectionMatrix = RenderAPI->CameraGetProjectionMatrix(Camera);
    RenderAPI->SetUniform(ShaderData, "view", &ViewMatrix);
    RenderAPI->SetUniform(ShaderData, "projection", &ProjectionMatrix);

    // Set lighting uniforms once
    AxVec3 LightPos = { 50.0f, 80.0f, 30.0f };
    AxVec3 LightColor = { 1.0f, 1.0f, 1.0f };
    // Extract camera position from view matrix (inverse of view matrix translation)
    AxVec3 ViewPos = { -Camera->ViewMatrix.E[3][0], -Camera->ViewMatrix.E[3][1], -Camera->ViewMatrix.E[3][2] };
    RenderAPI->SetUniform(ShaderData, "lightPos", &LightPos);
    RenderAPI->SetUniform(ShaderData, "lightColor", &LightColor);
    RenderAPI->SetUniform(ShaderData, "viewPos", &ViewPos);

    // Set default material properties once (per-mesh properties set later)
    AxVec3 Color = { 1.0f, 0.95f, 0.8f };
    float MaterialAlpha = 1.0f;
    RenderAPI->SetUniform(ShaderData, "color", &Color);
    RenderAPI->SetUniform(ShaderData, "materialAlpha", &MaterialAlpha);

    // Render all meshes
    for (int i = 0; i < ArraySize(Model->Meshes); i++) {
        AxMesh* mesh = &Model->Meshes[i];

        // Set the model matrix for this specific mesh
        AxMat4x4 ModelMatrix;
        if (Model->Transforms && mesh->TransformIndex < ArraySize(Model->Transforms)) {
            ModelMatrix = Model->Transforms[mesh->TransformIndex];
        } else {
            ModelMatrix = Identity();
        }
        RenderAPI->SetUniform(ShaderData, "model", &ModelMatrix);

        // Set per-mesh material properties
        AxVec3 MeshColor = { 1.0f, 1.0f, 1.0f };
        float MeshAlpha = 1.0f;
        if (Model->Materials && mesh->MaterialIndex < ArraySize(Model->Materials)) {
            AxMaterial* material = &Model->Materials[mesh->MaterialIndex];
            MeshColor.X = material->BaseColorFactor[0];
            MeshColor.Y = material->BaseColorFactor[1];
            MeshColor.Z = material->BaseColorFactor[2];
            MeshAlpha = material->BaseColorFactor[3];
        }
        RenderAPI->SetUniform(ShaderData, "color", &MeshColor);
        RenderAPI->SetUniform(ShaderData, "materialAlpha", &MeshAlpha);

        // Bind the appropriate textures for this mesh
        bool hasAlpha = false;
        bool hasNormalMap = false;

        // Bind base color texture
        if (Model->Textures && ArraySize(Model->Textures) > 0) {
            // Use the mesh's material texture, or fallback to first texture
            int textureIndex = (mesh->BaseColorTexture < ArraySize(Model->Textures)) ?
                              mesh->BaseColorTexture : 0;
            AxTexture* texture = &Model->Textures[textureIndex];
            hasAlpha = RenderAPI->TextureHasAlpha(texture);

            RenderAPI->BindTexture(texture, 0);
            int baseTextureSlot = 0;
            RenderAPI->SetUniform(ShaderData, "diffuseTexture", &baseTextureSlot);
        }

        // Bind normal texture if available
        if (Model->Textures && mesh->NormalTexture > 0 && mesh->NormalTexture < ArraySize(Model->Textures)) {
            AxTexture* normalTexture = &Model->Textures[mesh->NormalTexture];
            RenderAPI->BindTexture(normalTexture, 1);
            int normalTextureSlot = 1;
            RenderAPI->SetUniform(ShaderData, "normalTexture", &normalTextureSlot);
            hasNormalMap = true;
        }

        // Set the hasNormalMap uniform
        RenderAPI->SetUniform(ShaderData, "hasNormalMap", &hasNormalMap);

        // Configure depth writing based on transparency
        RenderAPI->SetDepthWrite(!hasAlpha);

        // Render this mesh
        RenderAPI->Render(Viewport, mesh, ShaderData);

        // Restore depth writing for next object
        RenderAPI->SetDepthWrite(true);
    }
}

static void RenderModel(AxModel* Model, AxMesh* Mesh, AxCamera* Camera, AxTransform *CameraTransform, struct AxShaderData* ShaderData, struct AxViewport* Viewport)
{
    AxMat4x4 ViewMatrix = CreateViewMatrix(CameraTransform);
    AxMat4x4 ProjectionMatrix = RenderAPI->CameraGetProjectionMatrix(Camera);
    RenderAPI->SetUniform(ShaderData, "view", &ViewMatrix);
    RenderAPI->SetUniform(ShaderData, "projection", &ProjectionMatrix);

    // Use the mesh's specific transform if available, otherwise use identity
    AxMat4x4 ModelMatrix;
    if (Model->Transforms && Mesh->TransformIndex < ArraySize(Model->Transforms)) {
        ModelMatrix = Model->Transforms[Mesh->TransformIndex];
    } else {
        ModelMatrix = Identity();
    }
    RenderAPI->SetUniform(ShaderData, "model", &ModelMatrix);

    // Set lighting uniforms (these could also be moved to a global lighting state)
    AxVec3 LightPos = { 5.0f, 5.0f, 5.0f };
    AxVec3 LightColor = { 1.0f, 1.0f, 1.0f };
    // Extract camera position from view matrix (inverse of view matrix translation)
    AxVec3 ViewPos = { -Camera->ViewMatrix.E[3][0], -Camera->ViewMatrix.E[3][1], -Camera->ViewMatrix.E[3][2] };
    RenderAPI->SetUniform(ShaderData, "lightPos", &LightPos);
    RenderAPI->SetUniform(ShaderData, "lightColor", &LightColor);
    RenderAPI->SetUniform(ShaderData, "viewPos", &ViewPos);

    // Set material properties
    AxVec3 Color = { 1.0f, 1.0f, 1.0f };
    RenderAPI->SetUniform(ShaderData, "color", &Color);

    // TODO: Fix per-mesh texture binding
    if (Model->Textures && Mesh->BaseColorTexture < ArraySize(Model->Textures)) {
        RenderAPI->BindTexture(&Model->Textures[Mesh->BaseColorTexture], 0);
        RenderAPI->SetUniform(ShaderData, "diffuseTexture", 0);
    }

    // Render the mesh
    RenderAPI->Render(Viewport, Mesh, ShaderData);
}

static void UpdateCamera(float DeltaTime)
{
    // Get input state
    enum AxWindowError Error = AX_WINDOW_ERROR_NONE;
    AxInputState InputState = {0};
    if (!WindowAPI->GetWindowInputState(TheApp->Window, &InputState, &Error)) {
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

    TransformTranslate(&PerspTransform, LocalMovement, false); // false = local space

    // Print camera position and rotation when P key is pressed
    if (InputState.Keys[AX_KEY_P]) {
        AxVec3 EulerAngles = QuatToEuler(PerspTransform.Rotation);
        printf("Camera Position: X=%.2f, Y=%.2f, Z=%.2f\n",
               PerspTransform.Translation.X,
               PerspTransform.Translation.Y,
               PerspTransform.Translation.Z);
        printf("Camera Rotation (Euler): Pitch=%.2f, Yaw=%.2f, Roll=%.2f\n",
               EulerAngles.X * (180.0f / AX_PI),
               EulerAngles.Y * (180.0f / AX_PI),
               EulerAngles.Z * (180.0f / AX_PI));
    }
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

    // Update camera with input
    UpdateCamera(DeltaT);

    // Start the frame
    RenderAPI->NewFrame();
    FrameCount++;

    // Set up the main viewport once per frame
    RenderAPI->SetActiveViewport(&Viewport);

    auto& SceneModel = Models["Scene"];  // Use reference to avoid copying
    auto& DiffuseModel = Models["Diffuse"];

    // Render all meshes in the model
    RenderModelWithMeshes(&SceneModel, &PerspCamera, &PerspTransform, ModelShaderData, &Viewport);

    RenderAPI->SwapBuffers();

    return(App->IsRequestingExit);
}

static bool Destroy(struct AxApplication *App)
{
    // Clean up resources
    RenderAPI->DestroyProgram(ModelShaderData->ShaderHandle);

    // Shutdown renderer
    RenderAPI->DestroyContext();
    WindowAPI->DestroyWindow(TheApp->Window);

    delete TheApp;

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
        HashTableAPI = (struct AxHashTableAPI *)APIRegistry->Get(AXON_HASH_TABLE_API_NAME);
        FileAPI = PlatformAPI->FileAPI;
        RegistryAPI = APIRegistry;

        APIRegistry->Set(AXON_APPLICATION_API_NAME, AxApplicationAPI, sizeof(struct AxApplicationAPI));
    }
}