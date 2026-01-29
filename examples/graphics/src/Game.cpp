#include "Game.h"

#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxPlugin.h"
#include "Foundation/AxHashTable.h"
#include "Foundation/AxMath.h"

#define AXARRAY_IMPLEMENTATION
#include "Foundation/AxArray.h"

#include "AxWindow/AxWindow.h"
#include "AxOpenGL/AxOpenGL.h"
#include "AxScene/AxScene.h"

#include <stdio.h>

// Camera controls state
static float CameraSpeed = 5.0f;
static float MouseSensitivity = 0.001f;
static AxVec2 LastMousePos = { 0.0f, 0.0f };

struct AxWindowAPI *Window;

// Static callback functions
static void KeyCallback(AxWindow *Window, int Key, int ScanCode, int Action, int Mods, void *UserData)
{
    Game* App = static_cast<Game*>(UserData);

    // Game now has direct access to WindowAPI, no need to query registry
    AxWindowAPI *WindowAPI = App->GetWindowAPI();


    if (Key == AX_KEY_ESCAPE && Action == AX_PRESS) {
        App->IsRequestingExit(true);
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
}

static void MousePosCallback(AxWindow *Window, double X, double Y, void *UserData)
{
    Game* App = static_cast<Game*>(UserData);

    AxVec2 MouseDelta = {
        (float)X - LastMousePos.X,
        (float)Y - LastMousePos.Y
    };

    AxTransform *Transform = App->GetTransform();
    TransformRotateFromMouseDelta(Transform, MouseDelta, MouseSensitivity);
    LastMousePos = { (float)X, (float)Y };
}

static void MouseButtonCallback(AxWindow *Window, int Button, int Action, int Mods, void *UserData)
{
    if (Action == AX_PRESS) {
        printf("Mouse button pressed: %d\n", Button);
    }
}

static void ScrollCallback(AxWindow *Window, AxVec2 Offset, void *UserData)
{
    printf("Scroll: %.2f, %.2f\n", Offset.X, Offset.Y);
}

static void StateChangedCallback(AxWindow *Window, enum AxWindowState OldState, enum AxWindowState NewState, void *UserData)
{
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
}

void Game::RegisterCallbacks()
{
    // Use the new callback system with error handling
    AxWindowCallbacks Callbacks = {0};
    enum AxWindowError Error = AX_WINDOW_ERROR_NONE;

    // Set up all callbacks at once
    Callbacks.Key = KeyCallback;
    Callbacks.MousePos = MousePosCallback;
    Callbacks.MouseButton = MouseButtonCallback;
    Callbacks.Scroll = ScrollCallback;
    Callbacks.StateChanged = StateChangedCallback;

    // Set all callbacks at once with user data
    if (!WindowAPI_->SetCallbacks(Window_, &Callbacks, this, &Error)) {
        fprintf(stderr, "Failed to set callbacks: %s\n", WindowAPI_->GetErrorString(Error));
    }
}

bool Game::Create()
{
    // Create scene first, then load shaders from materials
    if (!CreateScene()) {
        fprintf(stderr, "Failed to create scene!\n");
        return false;
    }

    return true;
}

bool Game::Tick(float DeltaT)
{
    // Update camera with input
    UpdateCamera(DeltaT);

    return true;
}

void Game::Destroy()
{
}

// void Game::Initialize(AxAPIRegistry *APIRegistry, AxWindow *Window, const AxViewport *Viewport)
// {
//     // Store provided infrastructure
//     APIRegistry_ = APIRegistry;
//     // Window = Window;
//     // Viewport = Viewport;

//     // Get APIs from the registry
//     WindowAPI = (struct AxWindowAPI *)APIRegistry_->Get(AXON_WINDOW_API_NAME);
//     SceneAPI = (struct AxSceneAPI *)APIRegistry_->Get(AXON_SCENE_API_NAME);

//     // Register input callbacks now that we have everything
//     RegisterCallbacks();

//     // Set cursor for FPS-style controls
//     enum AxCursorMode CursorMode = AX_CURSOR_DISABLED;
//     AxWindowError Error;
//     if (!WindowAPI->SetCursorMode(Window, CursorMode, &Error)) {
//         fprintf(stderr, "Failed to set cursor mode: %s\n", WindowAPI->GetErrorString(Error));
//     }
// }

// void Game::SetupSceneCamera(const AxScene* Scene)
// {
//     if (!Scene) return;

//     // Allocate camera transform (owned by Game for modification)
//     static AxTransform CameraTransform = {0};

//     // Find camera object in scene for initial camera positioning
//     AxSceneObject* CameraObject = SceneAPI_->FindObject(Scene, "DefaultCamera");
//     if (CameraObject) {
//         // Copy transform from scene object so we can modify it
//         CameraTransform = CameraObject->Transform;
//         Transform_ = &CameraTransform;
//     } else {
//         // Use default camera transform if no camera object found
//         CameraTransform = (AxTransform){
//             .Translation = {0.0f, 2.0f, 5.0f},
//             .Rotation = {0.0f, 0.0f, 0.0f, 1.0f},
//             .Scale = {1.0f, 1.0f, 1.0f},
//             .Up = {0.0f, 1.0f, 0.0f}
//         };
//         Transform_ = &CameraTransform;
//     }
// }

// bool Game::CreateScene()
// {
//     // // Get camera from Engine (creates default if scene doesn't have one)
//     // AxTransform* CameraTransform = nullptr;
//     // Camera_ = EngineAPI_->GetSceneCamera(Engine_, 0, &CameraTransform);
//     // if (!Camera_) {
//     //     fprintf(stderr, "Failed to get scene camera\n");
//     //     return (false);
//     // }

//     // Set up camera transform from scene's DefaultCamera object
//     SetupSceneCamera(Scene_);

//     // Customize camera properties for this game
//     RenderAPI_->CameraSetFOV(Camera_, 60.0f * (AX_PI / 180.0f));
//     RenderAPI_->CameraSetAspectRatio(Camera_, Viewport_->Size.X / Viewport_->Size.Y);
//     RenderAPI_->CameraSetNearClipPlane(Camera_, 0.1f);
//     RenderAPI_->CameraSetFarClipPlane(Camera_, 100.0f);

//     return (true);
// }

// void Game::UpdateCamera(float DeltaTime)
// {
//     // Get input state
//     enum AxWindowError Error = AX_WINDOW_ERROR_NONE;
//     AxInputState InputState = {0};
//     if (!WindowAPI_->GetWindowInputState(Window_, &InputState, &Error)) {
//         return;
//     }

//     float Horizontal = GetAxis(InputState.Keys[AX_KEY_D], InputState.Keys[AX_KEY_A]);
//     float Vertical = GetAxis(InputState.Keys[AX_KEY_W], InputState.Keys[AX_KEY_S]);
//     float VerticalUpDown = GetAxis(InputState.Keys[AX_KEY_E], InputState.Keys[AX_KEY_Q]);

//     // Calculate movement in local space (relative to camera rotation)
//     // Note: In OpenGL/3D graphics, -Z is forward, so negate the vertical movement
//     AxVec3 LocalMovement = {
//         Horizontal * CameraSpeed * DeltaTime,        // X: right/left
//         VerticalUpDown * CameraSpeed * DeltaTime,    // Y: up/down
//         -Vertical * CameraSpeed * DeltaTime          // Z: forward/back (negated for OpenGL)
//     };

//     TransformTranslate(Transform_, LocalMovement, false); // false = local space

//     // Update the scene's camera transform so Engine uses the updated position
//     if (Scene_ && SceneAPI_) {
//         AxTransform* SceneCameraTransform = nullptr;
//         SceneAPI_->GetCamera(Scene_, 0, &SceneCameraTransform);
//         if (SceneCameraTransform) {
//             *SceneCameraTransform = *Transform_;
//         }
//     }

//     // Print camera position and rotation when P key is pressed
//     if (InputState.Keys[AX_KEY_P]) {
//         AxVec3 EulerAngles = QuatToEuler(Transform_->Rotation);
//         printf("Camera Position: X=%.2f, Y=%.2f, Z=%.2f\n",
//                Transform_->Translation.X,
//                Transform_->Translation.Y,
//                Transform_->Translation.Z);
//         printf("Camera Rotation (Euler): Pitch=%.2f, Yaw=%.2f, Roll=%.2f\n",
//                EulerAngles.X * (180.0f / AX_PI),
//                EulerAngles.Y * (180.0f / AX_PI),
//                EulerAngles.Z * (180.0f / AX_PI));
//     }
// }

// void Game::IsRequestingExit(bool Value)
// {
//     IsRequestingExit_ = Value;
// }

