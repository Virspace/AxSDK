#pragma once

#include "Foundation/Types.h"

// Key codes
#define AX_KEY_ESC 0x001
#define AX_KEY_1 0x002
#define AX_KEY_2 0x003
#define AX_KEY_3 0x004
#define AX_KEY_4 0x005
#define AX_KEY_5 0x006
#define AX_KEY_6 0x007
#define AX_KEY_7 0x008
#define AX_KEY_8 0x009
#define AX_KEY_9 0x00A
#define AX_KEY_0 0x00B
#define AX_KEY_A 0x01E
#define AX_KEY_B 0x030
#define AX_KEY_C 0x02E
#define AX_KEY_D 0x020
#define AX_KEY_E 0x012
#define AX_KEY_F 0x021
#define AX_KEY_G 0x022
#define AX_KEY_H 0x023
#define AX_KEY_I 0x017
#define AX_KEY_J 0x024
#define AX_KEY_K 0x025
#define AX_KEY_L 0x026
#define AX_KEY_M 0x032
#define AX_KEY_N 0x031
#define AX_KEY_O 0x018
#define AX_KEY_P 0x019
#define AX_KEY_Q 0x010
#define AX_KEY_R 0x013
#define AX_KEY_S 0x01F
#define AX_KEY_T 0x014
#define AX_KEY_U 0x016
#define AX_KEY_V 0x02F
#define AX_KEY_W 0x011
#define AX_KEY_X 0x02D
#define AX_KEY_Y 0x015
#define AX_KEY_Z 0x02C
#define AX_KEY_UP 0x148
#define AX_KEY_DOWN 0x150
#define AX_KEY_LEFT 0x14B
#define AX_KEY_RIGHT 0x14D
#define AX_KEY_LEFT_ALT 0x038
#define AX_KEY_RIGHT_ALT 0x138
#define AX_KEY_LEFT_SHIFT 0x02A
#define AX_KEY_RIGHT_SHIFT 0x036
#define AX_KEY_SPACE 0x039
#define AX_KEY_LEFT_CTRL 0x01D
#define AX_KEY_RIGHT_CTRL 0x11D
#define AX_KEY_NUMPAD_0 0x052
#define AX_KEY_NUMPAD_1 0x04F
#define AX_KEY_NUMPAD_2 0x050
#define AX_KEY_NUMPAD_3 0x051
#define AX_KEY_NUMPAD_4 0x04B
#define AX_KEY_NUMPAD_5 0x04C
#define AX_KEY_NUMPAD_6 0x04D
#define AX_KEY_NUMPAD_7 0x047
#define AX_KEY_NUMPAD_8 0x048
#define AX_KEY_NUMPAD_9 0x049
#define AX_KEY_NUMPAD_ENTER 0x11C
#define AX_KEY_NUMPAD_DECIMAL 0x053

// Window style flags
enum AxWindowStyle
{
    AX_WINDOW_STYLE_VISIBLE    = 1 << 0,
    AX_WINDOW_STYLE_CENTERED   = 1 << 1,
    AX_WINDOW_STYLE_DECORATED  = 1 << 2,
    AX_WINDOW_STYLE_RESIZABLE  = 1 << 3,
    AX_WINDOW_STYLE_LOCKASPECT = 1 << 4,
    AX_WINDOW_STYLE_MAXIMIZED  = 1 << 5,
    AX_WINDOW_STYLE_FULLSCREEN = 1 << 6, // Fullscreen Borderless
};

// Key modifer flags
enum AxKeyModifier
{
    AX_KEY_SHIFT               = 1 << 0,
    AX_KEY_CTRL                = 1 << 1,
    AX_KEY_ALT                 = 1 << 2,
    AX_KEY_WIN                 = 1 << 3,
    AX_KEY_CAPS                = 1 << 4,
    AX_KEY_NUMLOCK             = 1 << 5
};

// Key state flags
enum AxKeyState
{
    AX_KEY_PRESSED,
    AX_KEY_RELEASED
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

// Keyboard modes
enum AxKeyboardMode
{
    AX_KEYBOARD_ENABLED,
    AX_KEYBOARD_DISABLED
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
        AxDisplay *Display, enum AxWindowStyle StyleFlags);

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

    /**
     * @brief 
     * 
     */
    void (*SetKeyboardMode)(AxWindow *Window, enum AxKeyboardMode KeyboardMode);

    /**
     * @brief 
     * 
     */
    

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