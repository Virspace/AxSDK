/**
 * AxInput.cpp - Centralized input state management
 *
 * Singleton that polls input from the window and provides query methods.
 */

#include "AxEngine/AxInput.h"
#include "AxWindow/AxWindow.h"
#include "Foundation/AxAPIRegistry.h"

#include <string.h>

AxInput::AxInput()
{
    CurrentState_ = new AxInputState();
    PrevState_ = new AxInputState();
    memset(CurrentState_, 0, sizeof(AxInputState));
    memset(PrevState_, 0, sizeof(AxInputState));
}

AxInput::~AxInput()
{
    Shutdown();
}

AxInput& AxInput::Get()
{
    static AxInput s_Instance;
    return (s_Instance);
}

void AxInput::Initialize(AxAPIRegistry* Registry, AxWindow* Window)
{
    WindowAPI_ = static_cast<AxWindowAPI*>(Registry->Get(AXON_WINDOW_API_NAME));
    Window_ = Window;
}

void AxInput::Shutdown()
{
    if (CurrentState_) {
        delete CurrentState_;
        CurrentState_ = nullptr;
    }
    if (PrevState_) {
        delete PrevState_;
        PrevState_ = nullptr;
    }
    WindowAPI_ = nullptr;
    Window_ = nullptr;
}

void AxInput::Update()
{
    if (!WindowAPI_ || !Window_ || !CurrentState_ || !PrevState_) {
        return;
    }

    // Store previous frame state
    *PrevState_ = *CurrentState_;
    PrevMousePos_ = MousePos_;

    // Get current input state from window
    AxWindowError Error = AX_WINDOW_ERROR_NONE;
    if (!WindowAPI_->GetWindowInputState(Window_, CurrentState_, &Error)) {
        return;
    }

    // Get mouse position and compute delta
    WindowAPI_->GetMouseCoords(Window_, &MousePos_);
    MouseDelta_.X = MousePos_.X - PrevMousePos_.X;
    MouseDelta_.Y = MousePos_.Y - PrevMousePos_.Y;
}

bool AxInput::IsKeyDown(int Key) const
{
    if (!CurrentState_) return (false);
    return (CurrentState_->Keys[Key]);
}

bool AxInput::IsKeyPressed(int Key) const
{
    if (!CurrentState_ || !PrevState_) return (false);
    return (CurrentState_->Keys[Key] && !PrevState_->Keys[Key]);
}

bool AxInput::IsKeyReleased(int Key) const
{
    if (!CurrentState_ || !PrevState_) return (false);
    return (!CurrentState_->Keys[Key] && PrevState_->Keys[Key]);
}

float AxInput::GetAxis(int PositiveKey, int NegativeKey) const
{
    if (!CurrentState_) return (0.0f);

    bool Pos = CurrentState_->Keys[PositiveKey];
    bool Neg = CurrentState_->Keys[NegativeKey];

    if (Pos && !Neg) return (1.0f);
    if (Neg && !Pos) return (-1.0f);
    return (0.0f);
}
