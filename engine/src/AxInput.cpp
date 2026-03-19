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
    if (!Registry) {
        return;
    }

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

//=============================================================================
// Action Mapping
//=============================================================================

void AxInput::MapAction(std::string_view Name, int Key)
{
    auto& Binding = Actions_[std::string(Name)];
    for (int K : Binding.Keys) {
        if (K == Key) return;
    }
    Binding.Keys.push_back(Key);
}

void AxInput::UnmapAction(std::string_view Name)
{
    Actions_.erase(std::string(Name));
}

bool AxInput::IsActionDown(std::string_view Name) const
{
    auto It = Actions_.find(std::string(Name));
    if (It == Actions_.end()) return (false);
    for (int Key : It->second.Keys) {
        if (IsKeyDown(Key)) return (true);
    }
    return (false);
}

bool AxInput::IsActionPressed(std::string_view Name) const
{
    auto It = Actions_.find(std::string(Name));
    if (It == Actions_.end()) return (false);
    for (int Key : It->second.Keys) {
        if (IsKeyPressed(Key)) return (true);
    }
    return (false);
}

bool AxInput::IsActionReleased(std::string_view Name) const
{
    auto It = Actions_.find(std::string(Name));
    if (It == Actions_.end()) return (false);
    for (int Key : It->second.Keys) {
        if (IsKeyReleased(Key)) return (true);
    }
    return (false);
}

//=============================================================================
// Axis Mapping
//=============================================================================

void AxInput::MapAxis(std::string_view Name, int PositiveKey, int NegativeKey)
{
    Axes_[std::string(Name)] = {PositiveKey, NegativeKey};
}

void AxInput::UnmapAxis(std::string_view Name)
{
    Axes_.erase(std::string(Name));
}

float AxInput::GetAxis(std::string_view Name) const
{
    auto It = Axes_.find(std::string(Name));
    if (It == Axes_.end()) return (0.0f);
    return (GetAxis(It->second.PositiveKey, It->second.NegativeKey));
}
