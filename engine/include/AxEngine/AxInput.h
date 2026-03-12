#pragma once

#include "Foundation/AxTypes.h"

struct AxAPIRegistry;
struct AxWindowAPI;
struct AxWindow;
struct AxInputState;

/**
 * AxInput - Centralized input state management
 *
 * Singleton class that tracks keyboard, mouse, and other input state.
 * Polls input from the window each frame when Update() is called.
 */
class AXENGINE_API AxInput
{
public:
    static AxInput& Get();

    /**
     * Initialize with API registry and window for input polling.
     * Must be called before Update().
     */
    void Initialize(AxAPIRegistry* Registry, AxWindow* Window);

    /**
     * Shutdown and release resources.
     */
    void Shutdown();

    /**
     * Poll input from window and update state.
     * Called once per frame by the engine.
     */
    void Update();

    // Keyboard queries
    bool IsKeyDown(int Key) const;
    bool IsKeyPressed(int Key) const;
    bool IsKeyReleased(int Key) const;
    float GetAxis(int PositiveKey, int NegativeKey) const;

    // Mouse queries
    AxVec2 GetMousePosition() const { return MousePos_; }
    AxVec2 GetMouseDelta() const { return MouseDelta_; }

    // Raw state access
    const AxInputState* GetCurrentState() const { return CurrentState_; }
    const AxInputState* GetPreviousState() const { return PrevState_; }

private:
    AxInput();
    ~AxInput();
    AxInput(const AxInput&) = delete;
    AxInput& operator=(const AxInput&) = delete;

    AxWindowAPI* WindowAPI_{nullptr};
    AxWindow* Window_{nullptr};

    AxInputState* CurrentState_{nullptr};
    AxInputState* PrevState_{nullptr};
    AxVec2 MousePos_{0.0f, 0.0f};
    AxVec2 PrevMousePos_{0.0f, 0.0f};
    AxVec2 MouseDelta_{0.0f, 0.0f};
};
