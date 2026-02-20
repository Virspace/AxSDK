#include "Game.h"

#include "AxEngine/AxSceneTypes.h"
#include "Foundation/AxMath.h"
#include "AxWindow/AxWindow.h"

#include <stdio.h>

static float CameraSpeed = 5.0f;
static float MouseSensitivity = 0.001f;

void Game::Init()
{
    // Load scene using Scene manager
    if (!SceneManager::Load("examples/graphics/scenes/sponza_atrium.ats")) {
        fprintf(stderr, "Game: Failed to load scene\n");
        return;
    }

    // Set initial camera position using inherited MainCamera
    MainCamera->Transform.Translation = { 11.12f, 1.7f, 1.83f };
    MainCamera->Transform.Rotation = QuatFromEuler({
        -2.06f * (AX_PI / 180.0f),
        76.10f * (AX_PI / 180.0f),
        0.0f
    });
}

void Game::Tick(float Alpha, float DeltaT)
{
    (void)Alpha;

    // Movement input using inherited methods
    float H = Input::GetAxis(AX_KEY_D, AX_KEY_A);
    float V = Input::GetAxis(AX_KEY_W, AX_KEY_S);
    float UD = Input::GetAxis(AX_KEY_E, AX_KEY_Q);

    AxVec3 Movement = {
        H * CameraSpeed * DeltaT,
        UD * CameraSpeed * DeltaT,
        -V * CameraSpeed * DeltaT
    };

    TransformTranslate(&MainCamera->Transform, Movement, false);
    TransformRotateFromMouseDelta(&MainCamera->Transform, MouseDelta, MouseSensitivity);
}
