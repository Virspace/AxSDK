/**
 * AxRender.cpp - Rendering system implementation
 *
 * Handles viewport, camera, shaders, and scene rendering.
 */

#include "AxEngine/AxRenderer.h"

#include "AxOpenGL/AxOpenGL.h"
#include "AxOpenGL/AxOpenGLTypes.h"
#include "AxScene/AxScene.h"
#include "AxResource/AxResource.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxMath.h"

#include <stdio.h>

AxRenderer::~AxRenderer()
{
    if (Viewport_) {
        Shutdown();
    }
}

bool AxRenderer::Initialize(AxAPIRegistry* Registry, uint64_t WindowHandle, int32_t Width, int32_t Height)
{
    if (!Registry || !WindowHandle) {
        return (false);
    }

    RenderAPI_ = static_cast<AxOpenGLAPI*>(Registry->Get(AXON_OPENGL_API_NAME));
    ResourceAPI_ = static_cast<AxResourceAPI*>(Registry->Get(AXON_RESOURCE_API_NAME));

    if (!RenderAPI_ || !ResourceAPI_) {
        fprintf(stderr, "AxRender: Failed to get required APIs\n");
        return (false);
    }

    // Create OpenGL context
    RenderAPI_->CreateContext(WindowHandle);

    // Create viewport
    Viewport_ = new AxViewport();
    Viewport_->Position = { 0.0f, 0.0f };
    Viewport_->Size = { static_cast<float>(Width), static_cast<float>(Height) };
    Viewport_->Scale = { 1.0f, 1.0f };
    Viewport_->Depth = { 0.0f, 1.0f };
    Viewport_->IsActive = true;
    Viewport_->ClearColor = { 0.42f, 0.51f, 0.54f, 0.0f };

    // Create and configure main camera
    MainCamera_ = new AxCamera();
    RenderAPI_->CreateCamera(MainCamera_);
    RenderAPI_->CameraSetFOV(MainCamera_, 60.0f * (AX_PI / 180.0f));
    RenderAPI_->CameraSetAspectRatio(MainCamera_, Viewport_->Size.X / Viewport_->Size.Y);
    RenderAPI_->CameraSetNearClipPlane(MainCamera_, 0.1f);
    RenderAPI_->CameraSetFarClipPlane(MainCamera_, 200.0f);

    // Initialize camera transform
    MainCamera_->Transform = {
        .Translation = { 0.0f, 2.0f, 5.0f },
        .Rotation = QuatIdentity(),
        .Scale = Vec3One(),
        .Up = Vec3Up()
    };

    // Load default shaders
    AxShaderHandle ShaderHandle = ResourceAPI_->LoadShader(
        "scenes/shaders/vert.glsl",
        "scenes/shaders/frag.glsl",
        nullptr);
    if (AX_HANDLE_IS_VALID(ShaderHandle)) {
        ShaderData_ = const_cast<AxShaderData*>(ResourceAPI_->GetShader(ShaderHandle));
        printf("AxRender: Loaded shader program %u\n", ShaderData_->ShaderHandle);
    } else {
        fprintf(stderr, "AxRender: Failed to load shaders\n");
        return (false);
    }

    printf("AxRender: Initialized (%dx%d)\n", Width, Height);
    return (true);
}

void AxRenderer::Shutdown()
{
    // Clear shader pointer (ResourceAPI handles cleanup)
    ShaderData_ = nullptr;

    // Delete camera
    if (MainCamera_) {
        delete MainCamera_;
        MainCamera_ = nullptr;
    }

    // Destroy renderer context
    if (RenderAPI_) {
        RenderAPI_->DestroyContext();
    }

    // Destroy viewport
    if (Viewport_) {
        delete Viewport_;
        Viewport_ = nullptr;
    }

    RenderAPI_ = nullptr;
    ResourceAPI_ = nullptr;
}

void AxRenderer::BeginFrame()
{
    RenderAPI_->NewFrame();
    RenderAPI_->SetActiveViewport(Viewport_);
}

void AxRenderer::EndFrame()
{
    RenderAPI_->SwapBuffers();
}

void AxRenderer::RenderScene(AxScene* Scene)
{
    if (!Scene || !ShaderData_) {
        return;
    }

    // Set up shared state once
    AxMat4x4 ViewMatrix = CreateViewMatrix(&MainCamera_->Transform);
    AxMat4x4 ProjectionMatrix = RenderAPI_->CameraGetProjectionMatrix(MainCamera_);
    RenderAPI_->SetUniform(ShaderData_, "view", &ViewMatrix);
    RenderAPI_->SetUniform(ShaderData_, "projection", &ProjectionMatrix);

    // Set lighting uniforms from scene lights
    AxVec3 ViewPos = MainCamera_->Transform.Translation;
    RenderAPI_->SetUniform(ShaderData_, "viewPos", &ViewPos);
    RenderAPI_->SetSceneLights(ShaderData_, Scene->Lights, Scene->LightCount);

    // Render scene objects
    RenderSceneObject(Scene->RootObject, nullptr);
}

void AxRenderer::RenderSceneObject(AxSceneObject* Obj, const AxMat4x4* ParentTransform)
{
    if (!Obj) return;

    // Compute local transform matrix (TRS: Translation * Rotation * Scale)
    AxMat4x4 ScaleMatrix = Mat4x4Scale(Obj->Transform.Scale);
    AxMat4x4 RotMatrix = QuatToMat4x4(Obj->Transform.Rotation);
    AxMat4x4 LocalTransform = Mat4x4Mul(RotMatrix, ScaleMatrix);
    LocalTransform.E[3][0] = Obj->Transform.Translation.X;
    LocalTransform.E[3][1] = Obj->Transform.Translation.Y;
    LocalTransform.E[3][2] = Obj->Transform.Translation.Z;

    AxMat4x4 WorldTransform;
    if (ParentTransform) {
        WorldTransform = Mat4x4Mul(*ParentTransform, LocalTransform);
    } else {
        WorldTransform = LocalTransform;
    }

    // Render this object's model if loaded
    if (Obj->LoadedModelIndex != 0) {
        AxModelHandle Handle;
        Handle.Index = Obj->LoadedModelIndex;
        Handle.Generation = Obj->LoadedModelGeneration;

        const AxModelData* Model = ResourceAPI_->GetModel(Handle);
        if (Model) {
            RenderModel(Model, &WorldTransform);
        }
    }

    // Render children with this object's world transform as parent
    RenderSceneObject(Obj->FirstChild, &WorldTransform);

    // Render siblings with the same parent transform
    RenderSceneObject(Obj->NextSibling, ParentTransform);
}

void AxRenderer::RenderModel(const AxModelData* Model, const AxMat4x4* BaseTransform)
{
    if (!Model || !ShaderData_) return;

    for (uint32_t i = 0; i < Model->MeshCount; i++)
    {
        AxMesh* Mesh = const_cast<AxMesh*>(ResourceAPI_->GetMesh(Model->Meshes[i]));
        if (!Mesh || Mesh->VAO == 0) {
            continue;
        }

        // Build the model matrix: BaseTransform * MeshTransform
        AxMat4x4 MeshTransform;
        if (Model->TransformCount > 0 && Mesh->TransformIndex < Model->TransformCount) {
            MeshTransform = Model->Transforms[Mesh->TransformIndex];
        } else {
            MeshTransform = Identity();
        }

        AxMat4x4 ModelMatrix;
        if (BaseTransform) {
            ModelMatrix = Mat4x4Mul(*BaseTransform, MeshTransform);
        } else {
            ModelMatrix = MeshTransform;
        }

        RenderAPI_->SetUniform(ShaderData_, "model", &ModelMatrix);

        // Set per-mesh material color
        AxVec4 MeshColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        float MetallicFactor = 1.0f;
        float RoughnessFactor = 1.0f;
        AxVec3 EmissiveFactor = { 0.0f, 0.0f, 0.0f };
        int AlphaMode = 0;
        float AlphaCutoff = 0.5f;

        if (Mesh->MaterialIndex < Model->MaterialCount) {
            const AxMaterialDesc* Material = ResourceAPI_->GetMaterial(Model->Materials[Mesh->MaterialIndex]);
            if (Material && Material->Type == AX_MATERIAL_TYPE_PBR) {
                MeshColor = Material->PBR.BaseColorFactor;
                MetallicFactor = Material->PBR.MetallicFactor;
                RoughnessFactor = Material->PBR.RoughnessFactor;
                EmissiveFactor = Material->PBR.EmissiveFactor;
                AlphaMode = static_cast<int>(Material->PBR.AlphaMode);
                AlphaCutoff = Material->PBR.AlphaCutoff;
            }
        }
        RenderAPI_->SetUniform(ShaderData_, "materialColor", &MeshColor);
        RenderAPI_->SetUniform(ShaderData_, "metallicFactor", &MetallicFactor);
        RenderAPI_->SetUniform(ShaderData_, "roughnessFactor", &RoughnessFactor);
        RenderAPI_->SetUniform(ShaderData_, "emissiveFactor", &EmissiveFactor);
        RenderAPI_->SetUniform(ShaderData_, "alphaMode", &AlphaMode);
        RenderAPI_->SetUniform(ShaderData_, "alphaCutoff", &AlphaCutoff);

        // Reset texture flags
        int Zero = 0;
        RenderAPI_->SetUniform(ShaderData_, "useDiffuseTexture", &Zero);
        RenderAPI_->SetUniform(ShaderData_, "useNormalTexture", &Zero);
        RenderAPI_->SetUniform(ShaderData_, "useMetallicRoughnessTexture", &Zero);
        RenderAPI_->SetUniform(ShaderData_, "useEmissiveTexture", &Zero);
        RenderAPI_->SetUniform(ShaderData_, "useOcclusionTexture", &Zero);

        // Bind base color texture
        if (Model->TextureCount > 0) {
            uint32_t textureIndex = (Mesh->BaseColorTexture < Model->TextureCount) ?
                                    Mesh->BaseColorTexture : 0;
            const AxTexture* Texture = ResourceAPI_->GetTexture(Model->Textures[textureIndex]);
            if (Texture) {
                RenderAPI_->BindTexture(const_cast<AxTexture*>(Texture), 0);
                int Slot = 0;
                int One = 1;
                RenderAPI_->SetUniform(ShaderData_, "diffuseTexture", &Slot);
                RenderAPI_->SetUniform(ShaderData_, "useDiffuseTexture", &One);
            }
        }

        // Bind PBR textures from material
        if (Mesh->MaterialIndex < Model->MaterialCount) {
            const AxMaterialDesc* Material = ResourceAPI_->GetMaterial(Model->Materials[Mesh->MaterialIndex]);
            if (Material && Material->Type == AX_MATERIAL_TYPE_PBR) {
                // Normal texture (slot 1)
                if (Material->PBR.NormalTexture >= 0 &&
                    static_cast<uint32_t>(Material->PBR.NormalTexture) < Model->TextureCount) {
                    const AxTexture* NormalTex = ResourceAPI_->GetTexture(
                        Model->Textures[Material->PBR.NormalTexture]);
                    if (NormalTex) {
                        RenderAPI_->BindTexture(const_cast<AxTexture*>(NormalTex), 1);
                        int Slot = 1, One = 1;
                        RenderAPI_->SetUniform(ShaderData_, "normalTexture", &Slot);
                        RenderAPI_->SetUniform(ShaderData_, "useNormalTexture", &One);
                    }
                }

                // Metallic-Roughness texture (slot 2)
                if (Material->PBR.MetallicRoughnessTexture >= 0 &&
                    static_cast<uint32_t>(Material->PBR.MetallicRoughnessTexture) < Model->TextureCount) {
                    const AxTexture* MRTex = ResourceAPI_->GetTexture(
                        Model->Textures[Material->PBR.MetallicRoughnessTexture]);
                    if (MRTex) {
                        RenderAPI_->BindTexture(const_cast<AxTexture*>(MRTex), 2);
                        int Slot = 2, One = 1;
                        RenderAPI_->SetUniform(ShaderData_, "metallicRoughnessTexture", &Slot);
                        RenderAPI_->SetUniform(ShaderData_, "useMetallicRoughnessTexture", &One);
                    }
                }

                // Emissive texture (slot 3)
                if (Material->PBR.EmissiveTexture >= 0 &&
                    static_cast<uint32_t>(Material->PBR.EmissiveTexture) < Model->TextureCount) {
                    const AxTexture* EmTex = ResourceAPI_->GetTexture(
                        Model->Textures[Material->PBR.EmissiveTexture]);
                    if (EmTex) {
                        RenderAPI_->BindTexture(const_cast<AxTexture*>(EmTex), 3);
                        int Slot = 3, One = 1;
                        RenderAPI_->SetUniform(ShaderData_, "emissiveTexture", &Slot);
                        RenderAPI_->SetUniform(ShaderData_, "useEmissiveTexture", &One);
                    }
                }

                // Occlusion texture (slot 4)
                if (Material->PBR.OcclusionTexture >= 0 &&
                    static_cast<uint32_t>(Material->PBR.OcclusionTexture) < Model->TextureCount) {
                    const AxTexture* OccTex = ResourceAPI_->GetTexture(
                        Model->Textures[Material->PBR.OcclusionTexture]);
                    if (OccTex) {
                        RenderAPI_->BindTexture(const_cast<AxTexture*>(OccTex), 4);
                        int Slot = 4, One = 1;
                        RenderAPI_->SetUniform(ShaderData_, "occlusionTexture", &Slot);
                        RenderAPI_->SetUniform(ShaderData_, "useOcclusionTexture", &One);
                    }
                }
            }
        }

        // Configure depth writing based on alpha mode
        bool DisableDepthWrite = (AlphaMode == 2); // AX_ALPHA_MODE_BLEND
        RenderAPI_->SetDepthWrite(!DisableDepthWrite);

        // Render the mesh
        RenderAPI_->Render(Viewport_, Mesh, ShaderData_);

        // Restore depth writing
        if (DisableDepthWrite) {
            RenderAPI_->SetDepthWrite(true);
        }
    }
}
