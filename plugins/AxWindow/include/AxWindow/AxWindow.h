#pragma once

#include "Foundation/AxTypes.h"

// Key codes
#define AX_KEY_SPACE              32
#define AX_KEY_APOSTROPHE         39  /* ' */
#define AX_KEY_COMMA              44  /* , */
#define AX_KEY_MINUS              45  /* - */
#define AX_KEY_PERIOD             46  /* . */
#define AX_KEY_SLASH              47  /* / */
#define AX_KEY_0                  48
#define AX_KEY_1                  49
#define AX_KEY_2                  50
#define AX_KEY_3                  51
#define AX_KEY_4                  52
#define AX_KEY_5                  53
#define AX_KEY_6                  54
#define AX_KEY_7                  55
#define AX_KEY_8                  56
#define AX_KEY_9                  57
#define AX_KEY_SEMICOLON          59  /* ; */
#define AX_KEY_EQUAL              61  /* = */
#define AX_KEY_A                  65
#define AX_KEY_B                  66
#define AX_KEY_C                  67
#define AX_KEY_D                  68
#define AX_KEY_E                  69
#define AX_KEY_F                  70
#define AX_KEY_G                  71
#define AX_KEY_H                  72
#define AX_KEY_I                  73
#define AX_KEY_J                  74
#define AX_KEY_K                  75
#define AX_KEY_L                  76
#define AX_KEY_M                  77
#define AX_KEY_N                  78
#define AX_KEY_O                  79
#define AX_KEY_P                  80
#define AX_KEY_Q                  81
#define AX_KEY_R                  82
#define AX_KEY_S                  83
#define AX_KEY_T                  84
#define AX_KEY_U                  85
#define AX_KEY_V                  86
#define AX_KEY_W                  87
#define AX_KEY_X                  88
#define AX_KEY_Y                  89
#define AX_KEY_Z                  90
#define AX_KEY_LEFT_BRACKET       91  /* [ */
#define AX_KEY_BACKSLASH          92  /* \ */
#define AX_KEY_RIGHT_BRACKET      93  /* ] */
#define AX_KEY_GRAVE_ACCENT       96  /* ` */
#define AX_KEY_WORLD_1            161 /* non-US #1 */
#define AX_KEY_WORLD_2            162 /* non-US #2 */

#define AX_KEY_ESCAPE             256
#define AX_KEY_ENTER              257
#define AX_KEY_TAB                258
#define AX_KEY_BACKSPACE          259
#define AX_KEY_INSERT             260
#define AX_KEY_DELETE             261
#define AX_KEY_RIGHT              262
#define AX_KEY_LEFT               263
#define AX_KEY_DOWN               264
#define AX_KEY_UP                 265
#define AX_KEY_PAGE_UP            266
#define AX_KEY_PAGE_DOWN          267
#define AX_KEY_HOME               268
#define AX_KEY_END                269
#define AX_KEY_CAPS_LOCK          280
#define AX_KEY_SCROLL_LOCK        281
#define AX_KEY_NUM_LOCK           282
#define AX_KEY_PRINT_SCREEN       283
#define AX_KEY_PAUSE              284
#define AX_KEY_F1                 290
#define AX_KEY_F2                 291
#define AX_KEY_F3                 292
#define AX_KEY_F4                 293
#define AX_KEY_F5                 294
#define AX_KEY_F6                 295
#define AX_KEY_F7                 296
#define AX_KEY_F8                 297
#define AX_KEY_F9                 298
#define AX_KEY_F10                299
#define AX_KEY_F11                300
#define AX_KEY_F12                301
#define AX_KEY_F13                302
#define AX_KEY_F14                303
#define AX_KEY_F15                304
#define AX_KEY_F16                305
#define AX_KEY_F17                306
#define AX_KEY_F18                307
#define AX_KEY_F19                308
#define AX_KEY_F20                309
#define AX_KEY_F21                310
#define AX_KEY_F22                311
#define AX_KEY_F23                312
#define AX_KEY_F24                313
#define AX_KEY_F25                314
#define AX_KEY_KP_0               320
#define AX_KEY_KP_1               321
#define AX_KEY_KP_2               322
#define AX_KEY_KP_3               323
#define AX_KEY_KP_4               324
#define AX_KEY_KP_5               325
#define AX_KEY_KP_6               326
#define AX_KEY_KP_7               327
#define AX_KEY_KP_8               328
#define AX_KEY_KP_9               329
#define AX_KEY_KP_DECIMAL         330
#define AX_KEY_KP_DIVIDE          331
#define AX_KEY_KP_MULTIPLY        332
#define AX_KEY_KP_SUBTRACT        333
#define AX_KEY_KP_ADD             334
#define AX_KEY_KP_ENTER           335
#define AX_KEY_KP_EQUAL           336
#define AX_KEY_LEFT_SHIFT         340
#define AX_KEY_LEFT_CONTROL       341
#define AX_KEY_LEFT_ALT           342
#define AX_KEY_LEFT_SUPER         343
#define AX_KEY_RIGHT_SHIFT        344
#define AX_KEY_RIGHT_CONTROL      345
#define AX_KEY_RIGHT_ALT          346
#define AX_KEY_RIGHT_SUPER        347
#define AX_KEY_MENU               348

#define AX_KEY_LAST               AX_KEY_MENU

#define AX_MOD_SHIFT              0x0001
#define AX_MOD_CONTROL            0x0002
#define AX_MOD_ALT                0x0004
#define AX_MOD_SUPER              0x0008
#define AX_MOD_CAPS_LOCK          0x0010
#define AX_MOD_NUM_LOCK           0x0020

// Mouse buttons
#define AX_MOUSE_BUTTON_1      0
#define AX_MOUSE_BUTTON_2      1
#define AX_MOUSE_BUTTON_3      2
#define AX_MOUSE_BUTTON_4      3
#define AX_MOUSE_BUTTON_5      4
#define AX_MOUSE_BUTTON_6      5
#define AX_MOUSE_BUTTON_7      6
#define AX_MOUSE_BUTTON_8      7
#define AX_MOUSE_BUTTON_LAST   AX_MOUSE_BUTTON_8
#define AX_MOUSE_BUTTON_LEFT   AX_MOUSE_BUTTON_1 // Alias
#define AX_MOUSE_BUTTON_RIGHT  AX_MOUSE_BUTTON_2 // Alias
#define AX_MOUSE_BUTTON_MIDDLE AX_MOUSE_BUTTON_3 // Alias

// Mouse button states
#define AX_RELEASE             0
#define AX_PRESS               1
#define AX_REPEAT              2

#define AX_KEY_UNKNOWN        -1

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

// MessageBox button flags
enum AxMessageBoxResponse
{
    AX_MESSAGEBOX_RESPONSE_OK           = 1,
    AX_MESSAGEBOX_RESPONSE_CANCEL       = 2,
    AX_MESSAGEBOX_RESPONSE_ABORT        = 3,
    AX_MESSAGEBOX_RESPONSE_RETRY        = 4,
    AX_MESSAGEBOX_RESPONSE_IGNORE       = 5,
    AX_MESSAGEBOX_RESPONSE_YES          = 6,
    AX_MESSAGEBOX_RESPONSE_NO           = 7,
    AX_MESSAGEBOX_RESPONSE_TRYAGAIN     = 10,
    AX_MESSAGEBOX_RESPONSE_CONTINUE     = 11
};

enum AxMessageBoxFlags
{
    AX_MESSAGEBOX_TYPE_ABORTRETRYIGNORE   = 1 << 0,
    AX_MESSAGEBOX_TYPE_CANCELTRYCONTINUE  = 1 << 1,
    AX_MESSAGEBOX_TYPE_HELP               = 1 << 2,
    AX_MESSAGEBOX_TYPE_OK                 = 1 << 3,
    AX_MESSAGEBOX_TYPE_OKCANCEL           = 1 << 4,
    AX_MESSAGEBOX_TYPE_RETRYCANCEL        = 1 << 5,
    AX_MESSAGEBOX_TYPE_YESNO              = 1 << 6,
    AX_MESSAGEBOX_TYPE_YESNOCANCEL        = 1 << 7,
    AX_MESSAGEBOX_ICON_EXCLAMATION        = 1 << 8,
    AX_MESSAGEBOX_ICON_WARNING            = 1 << 9,
    AX_MESSAGEBOX_ICON_INFORMATION        = 1 << 10,
    AX_MESSAGEBOX_ICON_QUESTION           = 1 << 11,
    AX_MESSAGEBOX_ICON_STOP               = 1 << 12,
    AX_MESSAGEBOX_ICON_ERROR              = 1 << 13,
    AX_MESSAGEBOX_DEFBUTTON1              = 1 << 14,
    AX_MESSAGEBOX_DEFBUTTON2              = 1 << 15,
    AX_MESSAGEBOX_DEFBUTTON3              = 1 << 16,
    AX_MESSAGEBOX_DEFBUTTON4              = 1 << 17,
    AX_MESSAGEBOX_APPLMODAL               = 1 << 18,
    AX_MESSAGEBOX_SYSTEMMODAL             = 1 << 19,
    AX_MESSAGEBOX_TASKMODAL               = 1 << 20
};

// // Key modifer flags
// enum AxKeyModifier
// {
//     AX_KEY_SHIFT               = 1 << 0,
//     AX_KEY_CTRL                = 1 << 1,
//     AX_KEY_ALT                 = 1 << 2,
//     AX_KEY_WIN                 = 1 << 3,
//     AX_KEY_CAPS                = 1 << 4,
//     AX_KEY_NUMLOCK             = 1 << 5
// };

// // Key state flags
// // TODO(mdeforge): I don't think this is used currently?
// enum AxKeyState
// {
//     AX_KEY_PRESSED,
//     AX_KEY_RELEASED
// };

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
    int16_t KeyCodes[512];
    int16_t ScanCodes[AX_KEY_LAST + 1];

    bool KeyMenu;
    // The last received high surrogate when decoding pairs of UTF-16 messages
    uint16_t HighSurrogate;
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

// Callbacks
struct AxWindowAPI;
typedef void (*AxKeyCallback)(struct AxWindow *Window, int Key, int ScanCode, int Action, int Mods);
typedef void (*AxMousePosCallback)(struct AxWindow *Window, double X, double Y);
typedef void (*AxMouseButtonCallback)(struct AxWindow *Window, int Button, int Action, int Mods);
typedef void (*AxMouseScrollCallback)(struct AxWindow *Window, AxVec2 Offset);
typedef void (*AxCharCallback)(struct AxWindow *Window, unsigned int Char);

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
     * @param StyleFlags A bitmask of AxWindowStyle.
     * @return An opaque pointer to a window.
     */
    AxWindow *(*CreateWindow)(const char *Title, int32_t X, int32_t Y, int32_t Width, int32_t Height, enum AxWindowStyle StyleFlags);

    /**
     * @brief Destroys the target window.
     * @param Window The window to be destroyed.
     */
    void (*DestroyWindow)(AxWindow *Window);

    /**
     * @brief Polls the events of the target window.
     * @param Window The target window.
     */
    void (*PollEvents)(AxWindow *Window);

    /**
     * @brief Checks if the target window has requested to close.
     * @param Window The target window.
     * @return True if the target window has requested a close, otherwise false.
     */
    bool (*HasRequestedClose)(AxWindow *Window);

    /**
     * @brief Gets the position of the target window.
     * @param Window The target window.
     * @param X The X position of the target window.
     * @param Y The Y position of the target window.
     */
    void (*GetWindowPosition)(AxWindow *Window, int32_t *X, int32_t *Y);

    /**
     * @brief Sets the position of the target window.
     * @param Window The target window.
     * @param X The desired X position of the target window.
     * @param Y The desired Y position of the target window.
     */
    void (*SetWindowPosition)(AxWindow *Window, int32_t X, int32_t Y);

    /**
     * @brief Gets the size of the target window.
     * @param Window The target window.
     * @param Width The Width of the target window.
     * @param Height The Height of the target window.
     */
    void (*GetWindowSize)(AxWindow *Window, int32_t *Width, int32_t *Height);

    /**
     * @brief Sets the size of the target window.
     * @param Window The target window.
     * @param Width The desired width of the target window.
     * @param Height The desired height of the target window.
     */
    void (*SetWindowSize)(AxWindow *Window, int32_t Width, int32_t Height);

    /**
     * @brief Sets the visibility of the target window.
     * @param Window The target window.
     * @param IsVisible The desired visibility of the target window.
     */
    void (*SetWindowVisible)(AxWindow *Window, bool IsVisible);

    /**
     * @brief Returns platform window data
     * @param Window The target window.
     * @return The platform data for the target window if target is valid, otherwise a zero-initialized AxWindowPlatformData struct.
     */
    AxWindowPlatformData (*GetPlatformData)(const AxWindow *Window);

    /**
     * @brief Gets the mouse coordinates of the target window.
     * @param Window The target window.
     * @param Position To be filled out by GetMouseCoords.
     */
    void (*GetMouseCoords)(const AxWindow *Window, AxVec2 *Position);

    /**
     * @brief Gets the state of the target mouse button on the target window.
     * @param Window The target window.
     * @param Button The target button.
     * @return An integer representing the AxKeyState of the button.
     */
    int32_t (*GetMouseButton)(const AxWindow *Window, int32_t Button);

    /**
     * @brief Sets the cursor mode of the target window.
     * @param Window The target window.
     * @param CursorMode The desired AxCursorMode.
     */
    void (*SetCursorMode)(AxWindow *Window, enum AxCursorMode CursorMode);

    /**
     * @brief Gets the cursor mode of the target window.
     * @param Window The target window.
     */
    enum AxCursorMode (*GetCursorMode)(AxWindow *Window);

    /**
     * @brief Sets the keyboard mode of the target window.
     * @param Window The target window.
     * @param KeyboardMode The desired KeyboardMode of the target window.
     */
    void (*SetKeyboardMode)(AxWindow *Window, enum AxKeyboardMode KeyboardMode);

    // /**
    //  * @brief Enables the cursor on the target window.
    //  * @param The target window.
    //  */
    // void (*EnableCursor)(const AxWindow *Window);

    // /**
    //  * @brief Disables the cursor on the target window.
    //  * @param The target window.
    //  */
    // void (*DisableCursor)(const AxWindow *Window);

    int32_t (*GetKey)(AxWindow *Window, int32_t Key);

    AxKeyCallback (*SetKeyCallback)(AxWindow *Window, AxKeyCallback Callback);

    AxMousePosCallback (*SetMousePosCallback)(AxWindow *Window, AxMousePosCallback Callback);

    AxMouseButtonCallback (*SetMouseButtonCallback)(AxWindow *Window, AxMouseButtonCallback Callback);

    AxMouseScrollCallback (*SetMouseScrollCallback)(AxWindow *Window, AxMouseScrollCallback Callback);

    AxCharCallback (*SetCharCallback)(AxWindow *Window, AxCharCallback Callback);

    /**
     * @brief Opens a File Open dialog.
     * @param Window A handle to the owner window of the dialog to be created. If this parameter is NULL, the dialog will no owner window.
     * @param Title The title of the dialog box, NULL sets default.
     * @param Filter The file types to filter on, e.g. "Supported Files(*.ms, *.txt, *.cpp, *.h)\0*.ms;*.txt;*.cpp;*.h\0";
     * @param IntialDirectory Sets the initial directory to open the file open dialog in, can be NULL.
     * @param FileName Pointer to character array that returns filename. Setting it sets default filename, otherwise should be zero-initialized.
     * @param FileNameSize Size of zero-initialized character array for filename.
     */
    bool (*OpenFileDialog)(const AxWindow *Window, const char *Title, const char *Filter, const char *InitialDirectory, char *FileName, uint32_t FileNameSize);

    /**
     * @brief Opens a File Save dialog.
     * @param Window A handle to the owner window of the dialog to be created. If this parameter is NULL, the dialog will no owner window.
     * @param Title The title of the dialog box, NULL sets default.
     * @param Filter The file types to filter on, e.g. "Supported Files(*.ms, *.txt, *.cpp, *.h)\0*.ms;*.txt;*.cpp;*.h\0";
     * @param IntialDirectory Sets the initial directory to open the file save dialog in, can be NULL.
     * @param FileName Pointer to character array that returns filename. Setting it sets default filename, otherwise should be zero-initialized.
     * @param FileNameSize Size of zero-initialized character array for filename.
     */
    bool (*SaveFileDialog)(const AxWindow *Window, const char *Title, const char *Filter, const char *InitialDirectory, char *FileName, uint32_t FileNameSize);

    /**
     * @brief Opens a basic File Open dialog.
     * @param Window A handle to the owner window of the dialog to be created. If this parameter is NULL, the dialog will no owner window.
     * @param Message The message above the file tree, can be NULL.
     * @param InitialDirectory Sets the initial directory for the open folder dialog, can be NULL.
     * @param FolderName Pointer to character array that returns folder name.
     * @param FolderNameSize Size of zero-initialized character array for folder name.
     */
    bool (*OpenFolderDialog)(const AxWindow *Window, const char *Message, const char *InitialDirectory, char *FolderName, uint32_t FolderNameSize);

    /**
     * @brief Opens a Message Box.
     * @param Window A handle to the owner window of the message box to be created. If this parameter is NULL, the message box has no owner window.
     * @param Message The message above the file tree, can be NULL.
     * @param Title The title of the dialog box, NULL sets default.
     * @param Type The contents and behavior of the dialog box set by a combination of AxMessageBoxFlags flags.
     */
    enum AxMessageBoxResponse (*CreateMessageBox)(const AxWindow *Window, const char *Message, const char *Title, enum AxMessageBoxFlags Type);
};