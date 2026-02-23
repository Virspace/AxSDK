#include "AxEngine/AxScriptBase.h"
#include "AxEngine/AxInput.h"
#include "AxEngine/AxTypedNodes.h"
#include "AxWindow/AxWindow.h"
#include "Foundation/AxMath.h"

#include <stdio.h>

static float CameraSpeed = 5.0f;
static float MouseSensitivity = 0.001f;

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
    }
};

AX_IMPLEMENT_SCRIPT(Game)
