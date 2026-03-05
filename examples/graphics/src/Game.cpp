#include "AxEngine/AxScriptBase.h"
#include "AxEngine/AxInput.h"
#include "AxEngine/AxTypedNodes.h"
#include "AxEngine/AxPrimitives.h"
#include "AxWindow/AxWindow.h"
#include "Foundation/AxMath.h"

#include <stdio.h>

static float CameraSpeed = 5.0f;
static float MouseSensitivity = 0.001f;
static float RotationSpeed = 1.0f;

class Game : public ScriptBase
{
public:
    void OnInit() override
    {
        // Set initial camera position using inherited MainCamera (CameraNode*)
        MainCamera->GetTransform().Translation = { 11.12f, 1.7f, 1.83f };
        MainCamera->GetTransform().Rotation = QuatFromEuler({
            -2.06f * (AX_PI / 180.0f),
            76.10f * (AX_PI / 180.0f),
            0.0f
        });

        // Spawn five primitive shapes
        SpawnPrimitives();
    }

    void OnUpdate(float DeltaT) override
    {
        // Movement input via AxInput singleton
        float H = AxInput::Get().GetAxis(AX_KEY_D, AX_KEY_A);
        float V = AxInput::Get().GetAxis(AX_KEY_W, AX_KEY_S);
        float UD = AxInput::Get().GetAxis(AX_KEY_E, AX_KEY_Q);

        AxVec3 Movement = {
            H * CameraSpeed * DeltaT,
            UD * CameraSpeed * DeltaT,
            -V * CameraSpeed * DeltaT
        };

        TransformTranslate(&MainCamera->GetTransform(), Movement, false);
        TransformRotateFromMouseDelta(&MainCamera->GetTransform(), MouseDelta, MouseSensitivity);

        // Rotate spawned primitives
        RotatePrimitives(DeltaT);
    }

private:
    // Pointers to spawned primitive MeshInstance nodes
    MeshInstance* BoxNode_{nullptr};
    MeshInstance* SphereNode_{nullptr};
    MeshInstance* PlaneNode_{nullptr};
    MeshInstance* CylinderNode_{nullptr};
    MeshInstance* CapsuleNode_{nullptr};

    void SpawnPrimitives()
    {
        BoxNode_ = CreateNode<MeshInstance>("PrimBox");
        if (BoxNode_) {
            BoxNode_->SetPrimitiveMesh(BoxMesh());
            BoxNode_->SetPosition(-4.0f, 1.0f, 0.0f);
            AddChild(BoxNode_);
        }

        SphereNode_ = CreateNode<MeshInstance>("PrimSphere");
        if (SphereNode_) {
            SphereNode_->SetPrimitiveMesh(SphereMesh());
            SphereNode_->SetPosition(-2.0f, 1.0f, 0.0f);
            AddChild(SphereNode_);
        }

        PlaneNode_ = CreateNode<MeshInstance>("PrimPlane");
        if (PlaneNode_) {
            PlaneNode_->SetPrimitiveMesh(PlaneMesh().SetWidth(0.8f).SetDepth(0.8f));
            PlaneNode_->SetPosition(0.0f, 1.0f, 0.0f);
            AddChild(PlaneNode_);
        }

        CylinderNode_ = CreateNode<MeshInstance>("PrimCylinder");
        if (CylinderNode_) {
            CylinderNode_->SetPrimitiveMesh(CylinderMesh().SetRadius(0.3f).SetHeight(0.8f));
            CylinderNode_->SetPosition(2.0f, 1.0f, 0.0f);
            AddChild(CylinderNode_);
        }

        CapsuleNode_ = CreateNode<MeshInstance>("PrimCapsule");
        if (CapsuleNode_) {
            CapsuleNode_->SetPrimitiveMesh(CapsuleMesh().SetRadius(0.25f).SetHeight(1.0f));
            CapsuleNode_->SetPosition(4.0f, 1.0f, 0.0f);
            AddChild(CapsuleNode_);
        }
    }

    void RotatePrimitives(float DeltaT)
    {
        float Angle = RotationSpeed * DeltaT;
        AxQuat YRot = QuatFromAxisAngle({ 0.0f, 1.0f, 0.0f }, Angle);

        MeshInstance* Nodes[] = { BoxNode_, SphereNode_, PlaneNode_, CylinderNode_, CapsuleNode_ };
        for (MeshInstance* N : Nodes) {
            if (N) {
                N->SetRotation(QuatMultiply(N->GetTransform().Rotation, YRot));
            }
        }
    }
};

AX_IMPLEMENT_SCRIPT(Game)
