#include "AxEngine/AxScriptBase.h"
#include "AxEngine/AxInput.h"
#include "AxEngine/AxTypedNodes.h"
#include "AxEngine/AxPrimitives.h"
#include "AxWindow/AxWindow.h"
#include "Foundation/AxMath.h"

#include <stdio.h>

static float CameraSpeed = 5.0f;
static float MouseSensitivity = 0.001f;

// Box bounce parameters
static float BobSpeed = 0.4f;       // cycles per second
static float BobHeight = 0.8f;      // amplitude of bounce
static float BoxBaseY = 1.0f;       // resting Y position

// Reaction durations
static float ScaleDuration = 1.2f;  // sphere scale pulse
static float FlipDuration = 1.5f;   // plane flip
static float JumpDuration = 1.0f;   // cylinder jump
static float WobbleDuration = 1.0f; // capsule wobble

class Game : public ScriptBase
{
public:
    void OnInit() override
    {
        MainCamera->SetPosition(11.12f, 1.7f, 1.83f);
        MainCamera->SetRotation(
            -2.06f * (AX_PI / 180.0f),
            76.10f * (AX_PI / 180.0f),
            0.0f
        );

        AxInput::Get().MapAxis("move_right", AX_KEY_D, AX_KEY_A);
        AxInput::Get().MapAxis("move_forward", AX_KEY_W, AX_KEY_S);
        AxInput::Get().MapAxis("move_up", AX_KEY_E, AX_KEY_Q);

        SpawnPrimitives();
        ConnectSignals();
    }

    void OnUpdate(float DeltaT) override
    {
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

        AnimateBox(DeltaT);
        AnimateSphere(DeltaT);
        AnimatePlane(DeltaT);
        AnimateCylinder(DeltaT);
        AnimateCapsule(DeltaT);
    }

private:
    MeshInstance* BoxNode_{nullptr};
    MeshInstance* SphereNode_{nullptr};
    MeshInstance* PlaneNode_{nullptr};
    MeshInstance* CylinderNode_{nullptr};
    MeshInstance* CapsuleNode_{nullptr};

    // Box bob state
    float BobTime_{0.0f};
    bool WasRising_{false};

    // Reaction timers (< 0 means inactive)
    float SphereTimer_{-1.0f};
    float PlaneTimer_{-1.0f};
    float CylinderTimer_{-1.0f};
    float CapsuleTimer_{-1.0f};
    float CapsuleWobbleDir_{1.0f};

    // Plane needs to track its flip angle
    float PlaneFlipAngle_{0.0f};

    // Cylinder base Y for jump
    float CylinderBaseY_{1.0f};

    void SpawnPrimitives()
    {
        BoxNode_ = CreateNode<MeshInstance>("PrimBox");
        if (BoxNode_) {
            BoxNode_->SetPrimitiveMesh(BoxMesh());
            BoxNode_->SetPosition(-4.0f, BoxBaseY, 0.0f);
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
            CylinderNode_->SetPosition(2.0f, CylinderBaseY_, 0.0f);
            AddChild(CylinderNode_);
        }

        CapsuleNode_ = CreateNode<MeshInstance>("PrimCapsule");
        if (CapsuleNode_) {
            CapsuleNode_->SetPrimitiveMesh(CapsuleMesh().SetRadius(0.25f).SetHeight(1.0f));
            CapsuleNode_->SetPosition(4.0f, 1.0f, 0.0f);
            AddChild(CapsuleNode_);
        }
    }

    void ConnectSignals()
    {
        if (!BoxNode_) { return; }

        // Sphere: scale pulse on peak
        if (SphereNode_) {
            Connect(BoxNode_, "peak", [this](const SignalArgs&) {
                SphereTimer_ = 0.0f;
            });
        }

        // Plane: full flip on peak
        if (PlaneNode_) {
            Connect(BoxNode_, "peak", [this](const SignalArgs&) {
                PlaneTimer_ = 0.0f;
                PlaneFlipAngle_ = 0.0f;
            });
        }

        // Cylinder: jump on peak
        if (CylinderNode_) {
            Connect(BoxNode_, "peak", [this](const SignalArgs&) {
                CylinderTimer_ = 0.0f;
            });
        }

        // Capsule: wobble tilt on peak
        if (CapsuleNode_) {
            Connect(BoxNode_, "peak", [this](const SignalArgs&) {
                CapsuleTimer_ = 0.0f;
                CapsuleWobbleDir_ = -CapsuleWobbleDir_; // alternate direction
            });
        }
    }

    //=========================================================================
    // Box: smooth bounce, emits "peak" at the top
    //=========================================================================

    void AnimateBox(float DeltaT)
    {
        if (!BoxNode_) { return; }

        BobTime_ += DeltaT * BobSpeed;

        // Use SinPulse mapped to a continuous oscillation via sin
        // sin goes -1 to 1; we want 0 to 1 to 0 for the Y offset
        float Phase = sinf(BobTime_ * 2.0f * AX_PI);
        float YOffset = BobHeight * (Phase * 0.5f + 0.5f); // map [-1,1] to [0,1]
        BoxNode_->SetPosition(-4.0f, BoxBaseY + YOffset, 0.0f);

        // Detect peak: was rising, now falling
        bool IsRising = (Phase > 0.0f && cosf(BobTime_ * 2.0f * AX_PI) > 0.0f);
        if (WasRising_ && !IsRising) {
            BoxNode_->EmitSignal("peak");
        }
        WasRising_ = IsRising;
    }

    //=========================================================================
    // Sphere: scales up then back down (breathe effect)
    //=========================================================================

    void AnimateSphere(float DeltaT)
    {
        if (!SphereNode_ || SphereTimer_ < 0.0f) { return; }

        SphereTimer_ += DeltaT;
        float T = SphereTimer_ / ScaleDuration;

        if (T >= 1.0f) {
            SphereTimer_ = -1.0f;
            SphereNode_->SetScale(1.0f, 1.0f, 1.0f);
            return;
        }

        // SinPulse: 0 -> 1 -> 0, use it to scale from 1.0 to 1.8 and back
        float S = 1.0f + 0.8f * SinPulse(T);
        SphereNode_->SetScale(S, S, S);
    }

    //=========================================================================
    // Plane: does a full 360 flip around X axis
    //=========================================================================

    void AnimatePlane(float DeltaT)
    {
        if (!PlaneNode_ || PlaneTimer_ < 0.0f) { return; }

        PlaneTimer_ += DeltaT;
        float T = PlaneTimer_ / FlipDuration;

        if (T >= 1.0f) {
            PlaneTimer_ = -1.0f;
            PlaneNode_->SetRotation(Quat::Identity());
            return;
        }

        // EaseInOutSin for smooth acceleration/deceleration through the flip
        float FlipProgress = EaseInOutSin(T);
        float Angle = FlipProgress * 2.0f * AX_PI;
        Quat XRot = Quat::FromAxisAngle(Vec3(1.0f, 0.0f, 0.0f), Angle);
        PlaneNode_->SetRotation(XRot);
    }

    //=========================================================================
    // Cylinder: jumps up and lands back down
    //=========================================================================

    void AnimateCylinder(float DeltaT)
    {
        if (!CylinderNode_ || CylinderTimer_ < 0.0f) { return; }

        CylinderTimer_ += DeltaT;
        float T = CylinderTimer_ / JumpDuration;

        if (T >= 1.0f) {
            CylinderTimer_ = -1.0f;
            CylinderNode_->SetPosition(2.0f, CylinderBaseY_, 0.0f);
            return;
        }

        // SinPulse gives a smooth arc: 0 -> peak -> 0
        float JumpHeight = 1.5f * SinPulse(T);
        CylinderNode_->SetPosition(2.0f, CylinderBaseY_ + JumpHeight, 0.0f);
    }

    //=========================================================================
    // Capsule: wobble tilt (leans to one side and back)
    //=========================================================================

    void AnimateCapsule(float DeltaT)
    {
        if (!CapsuleNode_ || CapsuleTimer_ < 0.0f) { return; }

        CapsuleTimer_ += DeltaT;
        float T = CapsuleTimer_ / WobbleDuration;

        if (T >= 1.0f) {
            CapsuleTimer_ = -1.0f;
            CapsuleNode_->SetRotation(Quat::Identity());
            return;
        }

        // Tilt 30 degrees to one side then back using SinPulse
        float TiltAngle = CapsuleWobbleDir_ * 0.52f * SinPulse(T); // ~30 degrees
        Quat ZRot = Quat::FromAxisAngle(Vec3(0.0f, 0.0f, 1.0f), TiltAngle);
        CapsuleNode_->SetRotation(ZRot);
    }
};

AX_IMPLEMENT_SCRIPT(Game)
