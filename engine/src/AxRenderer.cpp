/**
 * AxRender.cpp - Rendering system implementation
 *
 * Handles viewport, camera, shaders, and scene rendering.
 * Traverses the Node hierarchy via GetFirstChild/GetNextSibling and checks
 * NodeType::MeshInstance for model rendering. Lights are collected from
 * LightNode typed nodes via SceneTree::GetNodesByType().
 *
 * Supports two camera modes:
 *   - Scene camera: uses MainCameraNode_ (Play mode / standalone)
 *   - Editor camera: uses stored view/projection matrices (Edit mode)
 */

#include "AxEngine/AxRenderer.h"
#include "AxEngine/AxDebugDraw.h"
#include "AxEngine/AxSceneTree.h"
#include "AxEngine/AxNode.h"
#include "AxEngine/AxTypedNodes.h"

#include "AxOpenGL/AxOpenGL.h"
#include "AxOpenGL/AxOpenGLTypes.h"
#include "AxResource/AxResource.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxMath.h"
#include "AxLog/AxLog.h"

#include <cmath>

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
        AX_LOG(ERROR, "Failed to get required APIs");
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

    // Initialize editor camera matrices to identity
    EditorViewMatrix_ = Identity();
    EditorProjectionMatrix_ = Identity();
    EditorCameraPosition_ = {0.0f, 0.0f, 0.0f};

    // Load default shaders
    AxShaderHandle ShaderHandle = ResourceAPI_->LoadShader(
        "examples/graphics/scenes/shaders/vert.glsl",
        "examples/graphics/scenes/shaders/frag.glsl",
        nullptr);
    if (AX_HANDLE_IS_VALID(ShaderHandle)) {
        ShaderData_ = const_cast<AxShaderData*>(ResourceAPI_->GetShader(ShaderHandle));
        AX_LOG(INFO, "Loaded shader program %u", ShaderData_->ShaderHandle);
    } else {
        AX_LOG(ERROR, "Failed to load shaders");
        return (false);
    }

    // Initialize debug draw with the render API
    DebugDraw_.Initialize(RenderAPI_);

    AX_LOG(INFO, "Initialized (%dx%d)", Width, Height);
    return (true);
}

void AxRenderer::Shutdown()
{
    // Shutdown debug draw (before destroying GL context)
    DebugDraw_.Shutdown();

    // Clear shader pointer (ResourceAPI handles cleanup)
    ShaderData_ = nullptr;

    // Camera is owned by the SceneTree, just clear our reference
    MainCameraNode_ = nullptr;

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

void AxRenderer::SetMainCamera(CameraNode* Camera)
{
    MainCameraNode_ = Camera;
    if (Camera && RenderAPI_ && Viewport_) {
        RenderAPI_->CreateCamera(&Camera->Camera);
        Camera->Camera.AspectRatio = Viewport_->Size.X / Viewport_->Size.Y;
    }
}

void AxRenderer::Resize(int32_t Width, int32_t Height)
{
    if (!Viewport_) {
        return;
    }

    Viewport_->Size = { static_cast<float>(Width), static_cast<float>(Height) };

    // Update camera aspect ratio if a main camera is set
    if (MainCameraNode_ && Height > 0) {
        MainCameraNode_->Camera.AspectRatio =
            static_cast<float>(Width) / static_cast<float>(Height);
    }

    AX_LOG(DEBUG, "Viewport resized to %dx%d", Width, Height);
}

void AxRenderer::SetEditorCameraView(AxVec3 Position, AxVec3 Target,
                                     float FOV, float Near, float Far)
{
    if (!Viewport_ || !RenderAPI_) {
        return;
    }

    EditorCameraPosition_ = Position;

    // Build a temporary transform oriented toward the target
    Transform CamTransform;
    CamTransform.SetTranslation(Vec3(Position));
    CamTransform.LookAt(Vec3(Target), Vec3::Up());
    EditorViewMatrix_ = CamTransform.GetViewMatrix();

    // Compute projection matrix
    float AspectRatio = Viewport_->Size.X / Viewport_->Size.Y;
    EditorProjectionMatrix_ = Mat4::Perspective(FOV, AspectRatio, Near, Far);
}

void AxRenderer::RenderScene(SceneTree* Scene)
{
    if (!Scene || !ShaderData_) {
        return;
    }

    // Determine which camera to use for view/projection
    AxMat4x4 ViewMatrix;
    AxMat4x4 ProjectionMatrix;
    AxVec3 ViewPos;

    if (UseEditorCamera_) {
        ViewMatrix = EditorViewMatrix_;
        ProjectionMatrix = EditorProjectionMatrix_;
        ViewPos = EditorCameraPosition_;
    } else {
        // Scene camera mode -- requires a MainCameraNode
        if (!MainCameraNode_) {
            return;
        }
        ViewMatrix = MainCameraNode_->GetViewMatrix();
        ProjectionMatrix = MainCameraNode_->GetProjectionMatrix();
        ViewPos = MainCameraNode_->GetTransform().Translation;
    }

    // Set up shared state once
    RenderAPI_->SetUniform(ShaderData_, "view", &ViewMatrix);
    RenderAPI_->SetUniform(ShaderData_, "projection", &ProjectionMatrix);

    // Set lighting uniforms
    RenderAPI_->SetUniform(ShaderData_, "viewPos", &ViewPos);

    // Collect lights from typed LightNode nodes into a flat AxLight array
    uint32_t LightNodeCount = 0;
    Node** LightNodes = Scene->GetNodesByType(NodeType::Light, &LightNodeCount);

    if (LightNodes && LightNodeCount > 0) {
        // Build a temporary AxLight array for the OpenGL API
        AxLight TempLights[AX_SCENE_TREE_MAX_TYPED_NODES];
        uint32_t Count = (LightNodeCount < AX_SCENE_TREE_MAX_TYPED_NODES)
                       ? LightNodeCount : AX_SCENE_TREE_MAX_TYPED_NODES;

        for (uint32_t i = 0; i < Count; ++i) {
            LightNode* LN = static_cast<LightNode*>(LightNodes[i]);
            TempLights[i] = LN->Light;

            // Copy the node's world position into the light's position
            const Mat4& WorldMat = LN->GetWorldTransform();
            TempLights[i].Position = {WorldMat.E[3][0], WorldMat.E[3][1], WorldMat.E[3][2]};
        }

        RenderAPI_->SetSceneLights(ShaderData_, TempLights, static_cast<int32_t>(Count));
    } else {
        RenderAPI_->SetSceneLights(ShaderData_, nullptr, 0);
    }

    // Render the node hierarchy starting from the root
    RenderNode(static_cast<Node*>(Scene->GetRootNode()), nullptr);
}

void AxRenderer::FlushDebugDraw()
{
    if (DebugDraw_.GetLineVertexCount() == 0) {
        return;
    }

    // Determine current camera matrices
    AxMat4x4 ViewMatrix;
    AxMat4x4 ProjectionMatrix;

    if (UseEditorCamera_) {
        ViewMatrix = EditorViewMatrix_;
        ProjectionMatrix = EditorProjectionMatrix_;
    } else if (MainCameraNode_) {
        ViewMatrix = MainCameraNode_->GetViewMatrix();
        ProjectionMatrix = MainCameraNode_->GetProjectionMatrix();
    } else {
        // No camera available — cannot render debug draw
        DebugDraw_.Clear();
        return;
    }

    // Render debug lines on top of scene geometry
    RenderAPI_->SetDepthTest(false);
    DebugDraw_.Flush(ViewMatrix, ProjectionMatrix);
    RenderAPI_->SetDepthTest(true);
    DebugDraw_.Clear();
}

void AxRenderer::RenderNode(Node* NodePtr, const AxMat4x4* ParentTransform)
{
    if (!NodePtr) return;

    // Get the node's local forward matrix
    const Transform& T = NodePtr->GetTransform();
    AxMat4x4 LocalTransform = T.GetForwardMatrix();

    AxMat4x4 WorldTransform;
    if (ParentTransform) {
        WorldTransform = Mat4x4Mul(*ParentTransform, LocalTransform);
    } else {
        WorldTransform = LocalTransform;
    }

    // If this node is a MeshInstance with a loaded model, render it
    if (NodePtr->GetType() == NodeType::MeshInstance) {
        MeshInstance* MI = static_cast<MeshInstance*>(NodePtr);
        if (AX_HANDLE_IS_VALID(MI->ModelHandle)) {
            const AxModelData* Model = ResourceAPI_->GetModel(MI->ModelHandle);
            if (Model) {
                RenderModel(Model, &WorldTransform);
            }
        }
    }

    // Render children with this node's world transform as parent
    Node* Child = NodePtr->GetFirstChild();
    while (Child) {
        RenderNode(Child, &WorldTransform);
        Child = Child->GetNextSibling();
    }
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
