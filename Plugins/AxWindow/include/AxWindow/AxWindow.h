#pragma once

#include "Foundation/Types.h"

// Window style flags
enum AxWindowStyleFlags
{
    AX_WINDOW_STYLE_VISIBLE    = 1 << 0,
    AX_WINDOW_STYLE_CENTERED   = 1 << 1,
    AX_WINDOW_STYLE_DECORATED  = 1 << 2,
    AX_WINDOW_STYLE_RESIZABLE  = 1 << 3,
    AX_WINDOW_STYLE_LOCKASPECT = 1 << 4,
    AX_WINDOW_STYLE_MAXIMIZED  = 1 << 5,
    AX_WINDOW_STYLE_FULLSCREEN = 1 << 6, // Fullscreen Borderless
};

// Cursor modes
enum AxCursorMode
{
    // Applies pointer acceleration (ballistics) to a visible active cursor and limits movement to screen resolution.
    AX_CURSOR_NORMAL,
    // Applies pointer acceleration (ballistics) to a hidden active cursor and limits movement to screen resolution.
    AX_CURSOR_HIDDEN,
    // Hides the active cursor and locks it to the window, a virtual cursor position is provided.
    AX_CURSOR_DISABLED,
};

// An opaque object that represents a window.
typedef struct AxWindow AxWindow;

// An opaque object that represents a display.
typedef struct AxDisplay AxDisplay;

typedef struct AxWin32WindowData
{
    uint64_t Handle; // A handle to a window associated with a particualr module instance.
    uint64_t Instance; // The module instance the window belongs to (a particular EXE or DLL).
    bool CursorInWindow;

    AxVec2 LastCursorPos;
} AxWin32WindowData;

typedef struct AxLinuxWindowData
{
    uint64_t Foo; // TODO(mdeforge): How are linux windows managed?
} AxLinuxWindowData;

// NOTE(mdeforge): This is exposed so that others can add additional platform data
typedef struct AxWindowPlatformData
{
    union
    {
        AxWin32WindowData Win32;
        AxLinuxWindowData Linux;
    };
} AxWindowPlatformData;

#ifndef AXON_WNDCLASSNAME
    #define AXON_WNDCLASSNAME "AXONENGINE"
#endif

#define AXON_WINDOW_API_NAME "AxonWindowAPI"

struct AxWindowAPI
{
    void (*Init)(void);

    /**
     * @brief Creates a new window.
     * @param Title The window title that shows in the title bar.
     * @param X The horizontal position of the window.
     * @param Y The vertical position of the window.
     * @param Width The width of the window.
     * @param Height The height of the window.
     * @param Display The 
     * @param StyleFlags A bitmask of AxWindowStyleFlags.
     * @return An opaque pointer to a window.
     */
    AxWindow *(*CreateWindow)(const char *Title, int32_t X, int32_t Y, int32_t Width, int32_t Height,
        AxDisplay *Display, enum AxWindowStyleFlags StyleFlags);

    /**
     * @brief Destroys the target window.
     * @param Window The window to be destroyed.
     */
    void (*DestroyWindow)(AxWindow *Window);

    /**
     * @brief 
     * 
     */
    void (*PollEvents)(AxWindow *Window);

    /**
     * @brief 
     * 
     */
    bool (*HasRequestedClose)(AxWindow *Window);

    /**
     * @brief 
     * 
     */
    AxRect (*WindowRect)(AxWindow *Window);

    /**
     * @brief 
     * 
     */
    void (*SetWindowPosition)(AxWindow *Window, int32_t X, int32_t Y);

    /**
     * @brief 
     * 
     */
    void (*SetWindowSize)(AxWindow *Window, int32_t Width, int32_t Height);

    /**
     * @brief 
     * 
     */
    void (*SetWindowVisible)(AxWindow *Window, bool IsVisible);

    /**
     * @brief Returns platform window data
     * 
     */
    AxWindowPlatformData (*PlatformData)(const AxWindow *Window);

    /**
     * @brief 
     * 
     */
    void (*GetMouseCoords)(const AxWindow *Window, AxVec2 *Position);

    /**
     * @brief 
     * 
     */
    void (*SetCursorMode)(AxWindow *Window, enum AxCursorMode CursorMode);

    // /**
    //  * @brief 
    //  * 
    //  */
    // void (*EnableCursor)(const AxWindow *Window);

    // /**
    //  * @brief 
    //  * 
    //  */
    // void (*DisableCursor)(const AxWindow *Window);
};