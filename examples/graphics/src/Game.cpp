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
        MainCamera->SetPosition(11.12f, 1.7f, 1.83f);
        MainCamera->SetRotation(
            -2.06f * (AX_PI / 180.0f),
            76.10f * (AX_PI / 180.0f),
            0.0f
        );

        // Map named input axes
        AxInput::Get().MapAxis("move_right", AX_KEY_D, AX_KEY_A);
        AxInput::Get().MapAxis("move_forward", AX_KEY_W, AX_KEY_S);
        AxInput::Get().MapAxis("move_up", AX_KEY_E, AX_KEY_Q);

        // Spawn five primitive shapes
        SpawnPrimitives();
    }

    void OnUpdate(float DeltaT) override
    {
        // Movement input via named axes
        float H = AxInput::Get().GetAxis("move_right");
        float V = AxInput::Get().GetAxis("move_forward");
        float UD = AxInput::Get().GetAxis("move_up");

        Vec3 Movement(
            H * CameraSpeed * DeltaT,
            UD * CameraSpeed * DeltaT,
            -V * CameraSpeed * DeltaT
        );

        MainCamera->GetTransform().Translate(Movement);
        MainCamera->GetTransform().RotateFromMouseDelta(MouseDelta, MouseSensitivity);

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
        Quat YRot = Quat::FromAxisAngle(Vec3::Up(), Angle);

        MeshInstance* Nodes[] = { BoxNode_, SphereNode_, PlaneNode_, CylinderNode_, CapsuleNode_ };
        for (MeshInstance* N : Nodes) {
            if (N) {
                N->SetRotation(N->GetTransform().Rotation * YRot);
            }
        }
    }
};

AX_IMPLEMENT_SCRIPT(Game)
