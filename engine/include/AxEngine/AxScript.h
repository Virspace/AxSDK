#pragma once

#include "Foundation/AxTypes.h"
#include "AxResource/AxResourceTypes.h"
#include "AxEngine/AxInput.h"
#include "AxEngine/AxSceneManager.h"

// Forward declarations
struct AxAPIRegistry;
struct AxCamera;
struct AxInputState;
struct AxModelData;
struct AxScene;
class AxScripting;

/**
 * Read-only wrapper: allows reading and mutating the pointed-to object, but not reassignment.
 *
 * This provides Unity-style property semantics where:
 *   MainCamera->Transform.Translation = {...};  // Works - mutating the object
 *   MainCamera = otherCamera;                   // Compile error - no assignment operator
 */
template<typename T>
class ReadOnly
{
public:
    ReadOnly() : Value_(nullptr) {}
    operator T() const { return Value_; }       // Implicit read
    T operator->() const { return Value_; }     // Arrow operator for pointer types

    // No public assignment operator - reassignment is not allowed

private:
    T Value_;
    friend class AxScripting;  // Only AxScripting can assign
};

/**
 * Interface class that all AxScripts should derive from.
 *
 * Lifecycle order:
 *   1. Init()      - Called once after script is loaded
 *   2. Start()     - Called once before first tick
 *   3. FixedTick() - Called at fixed intervals (physics, etc.)
 *   4. Tick()      - Called every frame
 *   5. LateTick()  - Called after all Tick() calls (camera follow, etc.)
 *   6. Stop()      - Called when script is paused/stopped
 *   7. Term()      - Called once before script is unloaded
 *
 * Scripts have direct access to engine state via protected members:
 *   MainCamera    - The main camera (read-only pointer, mutable object)
 *   CurrentScene  - Currently loaded scene/model data
 *   MouseDelta    - Mouse movement this frame
 *
 * Input queries use AxInput singleton (centralized input state):
 *   GetAxis(), IsKeyDown(), IsKeyPressed(), IsKeyReleased(), GetMousePosition(), GetMouseDelta()
 *
 * Engine operations are available via protected methods:
 *   LoadScene(), UnloadScene()
 */
class AXENGINE_API AxScript
{
    friend class AxScripting;  // Allow AxScripting to set engine state

public:
    virtual ~AxScript() {}

    // Lifecycle methods - override only what you need
    virtual void Init() {}
    virtual void Start() {}
    virtual void FixedTick(float FixedDeltaT) { (void)FixedDeltaT; }
    virtual void Tick(float Alpha, float DeltaT) { (void)Alpha; (void)DeltaT; }
    virtual void LateTick() {}
    virtual void Stop() {}
    virtual void Term() {}

    inline void SetRegistry(const AxAPIRegistry* APIRegistry) { APIRegistry_ = APIRegistry; }
    inline const AxAPIRegistry* GetRegistry() const { return APIRegistry_; }

protected:
    //=========================================================================
    // Engine State (read-only pointers, mutable objects)
    //=========================================================================

    /** Main camera - modify Transform for movement. Cannot reassign pointer. */
    ReadOnly<AxCamera*> MainCamera;

    /** Currently loaded scene/model data. NULL if no scene loaded. */
    ReadOnly<const AxModelData*> CurrentScene;

    /** Mouse movement this frame. Use for camera rotation. */
    AxVec2 MouseDelta{0.0f, 0.0f};

    //=========================================================================
    // Engine Operations
    //=========================================================================

    struct SceneManager
    {
        /**
         * Load a scene from file path.
         * Unloads any previously loaded scene.
         *
         * @param Path - Path to scene file (e.g., "scenes/sponza_atrium.ats")
         * @return true if loaded successfully, false on error
         */
        static bool Load(std::string_view Path) { return AxSceneManager::Get().Load(Path); }

        /** Unload the current scene and free resources. */
        static void Unload() { AxSceneManager::Get().Unload(); }

        /** Check if a scene is currently loaded. */
        static bool IsLoaded() { return AxSceneManager::Get().IsLoaded(); }

        /** Get the current scene handle. */
        static AxSceneHandle GetHandle() { return AxSceneManager::Get().GetHandle(); }
    };

    struct Input
    {
        /**
         * Get axis value from two keys.
         * Returns -1.0f if only NegativeKey is down,
         *         +1.0f if only PositiveKey is down,
         *          0.0f if neither or both are down.
         */
        static float GetAxis(int PositiveKey, int NegativeKey) { return AxInput::Get().GetAxis(PositiveKey, NegativeKey); }

        /** Check if a key is currently held down. */
        static bool IsKeyDown(int Key) { return AxInput::Get().IsKeyDown(Key); }

        /** Check if a key was pressed this frame (went from up to down). */
        static bool IsKeyPressed(int Key) { return AxInput::Get().IsKeyPressed(Key); }

        /** Check if a key was released this frame (went from down to up). */
        static bool IsKeyReleased(int Key) { return AxInput::Get().IsKeyReleased(Key); }

        /** Get current mouse position in window coordinates. */
        static AxVec2 GetMousePosition() { return AxInput::Get().GetMousePosition(); }

        /** Get mouse movement this frame. */
        static AxVec2 GetMouseDelta() { return AxInput::Get().GetMouseDelta(); }
    };

    //=========================================================================
    // Internal
    //=========================================================================

    const AxAPIRegistry* APIRegistry_{nullptr};
};

#define CREATE_SCRIPT( ScriptClass, ScriptName ) \
        extern "C" __declspec(dllexport) AxScript* CreateScript() \
        { \
            return new ScriptClass(); \
        } \
