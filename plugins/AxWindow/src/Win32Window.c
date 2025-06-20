#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxMath.h"
#include "Foundation/AxPlatform.h"
#include "AxWindow.h"
#include <stdio.h>
#include <stdlib.h>
#include <float.h>

// NOTE(mdeforge): Uncomment to get some leak detection
//#define _CRTDBG_MAP_ALLOC
//#include <stdlib.h>
//#include <crtdbg.h>

// Macro to safely assign error codes
#define SET_ERROR(ErrorPtr, ErrorCode) do { \
    if (ErrorPtr) { \
        *(ErrorPtr) = (ErrorCode); \
    } \
} while(0)

// From hidusage.h
#define HID_USAGE_PAGE_GENERIC                          ((unsigned short) 0x01) // Generic Desktop Controls Usage Pages
#define HID_USAGE_GENERIC_MOUSE                         ((unsigned short) 0x02) // Generic Mouse
#define HID_USAGE_GENERIC_KEYBOARD                      ((unsigned short) 0x06) // Generic Keyboard
#define STRICT_TYPED_ITEMIDS

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define AX_MOD_MASK (AX_MOD_SHIFT | \
                       AX_MOD_CONTROL | \
                       AX_MOD_ALT | \
                       AX_MOD_SUPER | \
                       AX_MOD_CAPS_LOCK | \
                       AX_MOD_NUM_LOCK)

#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <winuser.h>

#undef CreateWindow

typedef HRESULT (WINAPI *GetDpiForMonitorPtr)(HMONITOR Monitor, int DPIType, UINT * XDPI, UINT *YDPI);

// Forward declarations
static GetDpiForMonitorPtr GetDpiForMonitor;
static enum AxWindowState GetWindowState(const AxWindow *Window);

struct AxWindowContext
{
    int32_t Major;
    int32_t Minor;
};

struct AxWindowHints
{
    struct AxWindowContext Context;
    //struct AxWindowConfig Config;
};

typedef struct AxWin32MonitorData
{
    HMONITOR Handle;      // A handle to the physical display
    CHAR AdapterName[32];
    CHAR DisplayName[32];
    bool HasModesPruned;
} AxWin32MonitorData;

typedef struct AxLinuxMonitorData
{
    void *Handle;
    char AdapterName[32];
    char DisplayName[32];
} AxLinuxMonitorData;

typedef struct AxMonitorPlatformData
{
    union
    {
        AxWin32MonitorData Win32;
        AxLinuxMonitorData Linux;
    };
} AxMonitorPlatformData;

struct AxWindow
{
    // Window title
    const char *Title;
    // User has requested close
    bool IsRequestingClose;
    // Size and Position
    RECT Rect;
    // Platform specific data
    AxWindowPlatformData Platform;
    // Window style flags
    enum AxWindowStyle Style;
    // Cursor modes
    enum AxCursorMode CursorMode;
    // Keyboard modes
    enum AxKeyboardMode KeyboardMode;
    // Virtual cursor position
    AxVec2 VirtualCursorPos;
    // Mouse button state
    char MouseButtons[AX_MOUSE_BUTTON_LAST + 1];
    char Keys[AX_KEY_LAST + 1];
    // Input handling
    bool HasFocus;                    ///< Current window focus state
    bool FilterEventsWhenUnfocused;   ///< Whether to filter input events when window loses focus
    AxVec2 LastMousePos;              ///< Previous mouse position for delta calculation

    struct {
        AxKeyCallback Key;
        AxMousePosCallback MousePos;
        AxMouseButtonCallback MouseButton;
        AxMouseScrollCallback Scroll;
        AxCharCallback Char;
        AxWindowStateCallback StateChanged;
    } Callbacks;
};

static AxRect RectToAxRect(RECT Rect)
{
    AxRect Result = {
        .Left = (float)Rect.left,
        .Top = (float)Rect.top,
        .Right = (float)Rect.right,
        .Bottom = (float)Rect.bottom
    };

    return (Result);
}

static RECT AxRectToRect(AxRect Rect)
{
    RECT Result = {
        .left = (int)Rect.Left,
        .top = (int)Rect.Top,
        .right = (int)Rect.Right,
        .bottom = (int)Rect.Bottom
    };

    return (Result);
}

struct AxPlatformAPI *PlatformAPI;

static void CreateKeyTable(AxWindow *Window)
{
    AXON_ASSERT(Window);

    memset(Window->Platform.Win32.KeyCodes, -1, sizeof(Window->Platform.Win32.KeyCodes));
    memset(Window->Platform.Win32.ScanCodes, -1, sizeof(Window->Platform.Win32.ScanCodes));

    Window->Platform.Win32.KeyCodes[0x00B] = AX_KEY_0;
    Window->Platform.Win32.KeyCodes[0x002] = AX_KEY_1;
    Window->Platform.Win32.KeyCodes[0x003] = AX_KEY_2;
    Window->Platform.Win32.KeyCodes[0x004] = AX_KEY_3;
    Window->Platform.Win32.KeyCodes[0x005] = AX_KEY_4;
    Window->Platform.Win32.KeyCodes[0x006] = AX_KEY_5;
    Window->Platform.Win32.KeyCodes[0x007] = AX_KEY_6;
    Window->Platform.Win32.KeyCodes[0x008] = AX_KEY_7;
    Window->Platform.Win32.KeyCodes[0x009] = AX_KEY_8;
    Window->Platform.Win32.KeyCodes[0x00A] = AX_KEY_9;
    Window->Platform.Win32.KeyCodes[0x01E] = AX_KEY_A;
    Window->Platform.Win32.KeyCodes[0x030] = AX_KEY_B;
    Window->Platform.Win32.KeyCodes[0x02E] = AX_KEY_C;
    Window->Platform.Win32.KeyCodes[0x020] = AX_KEY_D;
    Window->Platform.Win32.KeyCodes[0x012] = AX_KEY_E;
    Window->Platform.Win32.KeyCodes[0x021] = AX_KEY_F;
    Window->Platform.Win32.KeyCodes[0x022] = AX_KEY_G;
    Window->Platform.Win32.KeyCodes[0x023] = AX_KEY_H;
    Window->Platform.Win32.KeyCodes[0x017] = AX_KEY_I;
    Window->Platform.Win32.KeyCodes[0x024] = AX_KEY_J;
    Window->Platform.Win32.KeyCodes[0x025] = AX_KEY_K;
    Window->Platform.Win32.KeyCodes[0x026] = AX_KEY_L;
    Window->Platform.Win32.KeyCodes[0x032] = AX_KEY_M;
    Window->Platform.Win32.KeyCodes[0x031] = AX_KEY_N;
    Window->Platform.Win32.KeyCodes[0x018] = AX_KEY_O;
    Window->Platform.Win32.KeyCodes[0x019] = AX_KEY_P;
    Window->Platform.Win32.KeyCodes[0x010] = AX_KEY_Q;
    Window->Platform.Win32.KeyCodes[0x013] = AX_KEY_R;
    Window->Platform.Win32.KeyCodes[0x01F] = AX_KEY_S;
    Window->Platform.Win32.KeyCodes[0x014] = AX_KEY_T;
    Window->Platform.Win32.KeyCodes[0x016] = AX_KEY_U;
    Window->Platform.Win32.KeyCodes[0x02F] = AX_KEY_V;
    Window->Platform.Win32.KeyCodes[0x011] = AX_KEY_W;
    Window->Platform.Win32.KeyCodes[0x02D] = AX_KEY_X;
    Window->Platform.Win32.KeyCodes[0x015] = AX_KEY_Y;
    Window->Platform.Win32.KeyCodes[0x02C] = AX_KEY_Z;

    Window->Platform.Win32.KeyCodes[0x028] = AX_KEY_APOSTROPHE;
    Window->Platform.Win32.KeyCodes[0x02B] = AX_KEY_BACKSLASH;
    Window->Platform.Win32.KeyCodes[0x033] = AX_KEY_COMMA;
    Window->Platform.Win32.KeyCodes[0x00D] = AX_KEY_EQUAL;
    Window->Platform.Win32.KeyCodes[0x029] = AX_KEY_GRAVE_ACCENT;
    Window->Platform.Win32.KeyCodes[0x01A] = AX_KEY_LEFT_BRACKET;
    Window->Platform.Win32.KeyCodes[0x00C] = AX_KEY_MINUS;
    Window->Platform.Win32.KeyCodes[0x034] = AX_KEY_PERIOD;
    Window->Platform.Win32.KeyCodes[0x01B] = AX_KEY_RIGHT_BRACKET;
    Window->Platform.Win32.KeyCodes[0x027] = AX_KEY_SEMICOLON;
    Window->Platform.Win32.KeyCodes[0x035] = AX_KEY_SLASH;
    Window->Platform.Win32.KeyCodes[0x056] = AX_KEY_WORLD_2;

    Window->Platform.Win32.KeyCodes[0x00E] = AX_KEY_BACKSPACE;
    Window->Platform.Win32.KeyCodes[0x153] = AX_KEY_DELETE;
    Window->Platform.Win32.KeyCodes[0x14F] = AX_KEY_END;
    Window->Platform.Win32.KeyCodes[0x01C] = AX_KEY_ENTER;
    Window->Platform.Win32.KeyCodes[0x001] = AX_KEY_ESCAPE;
    Window->Platform.Win32.KeyCodes[0x147] = AX_KEY_HOME;
    Window->Platform.Win32.KeyCodes[0x152] = AX_KEY_INSERT;
    Window->Platform.Win32.KeyCodes[0x15D] = AX_KEY_MENU;
    Window->Platform.Win32.KeyCodes[0x151] = AX_KEY_PAGE_DOWN;
    Window->Platform.Win32.KeyCodes[0x149] = AX_KEY_PAGE_UP;
    Window->Platform.Win32.KeyCodes[0x045] = AX_KEY_PAUSE;
    Window->Platform.Win32.KeyCodes[0x039] = AX_KEY_SPACE;
    Window->Platform.Win32.KeyCodes[0x00F] = AX_KEY_TAB;
    Window->Platform.Win32.KeyCodes[0x03A] = AX_KEY_CAPS_LOCK;
    Window->Platform.Win32.KeyCodes[0x145] = AX_KEY_NUM_LOCK;
    Window->Platform.Win32.KeyCodes[0x046] = AX_KEY_SCROLL_LOCK;
    Window->Platform.Win32.KeyCodes[0x03B] = AX_KEY_F1;
    Window->Platform.Win32.KeyCodes[0x03C] = AX_KEY_F2;
    Window->Platform.Win32.KeyCodes[0x03D] = AX_KEY_F3;
    Window->Platform.Win32.KeyCodes[0x03E] = AX_KEY_F4;
    Window->Platform.Win32.KeyCodes[0x03F] = AX_KEY_F5;
    Window->Platform.Win32.KeyCodes[0x040] = AX_KEY_F6;
    Window->Platform.Win32.KeyCodes[0x041] = AX_KEY_F7;
    Window->Platform.Win32.KeyCodes[0x042] = AX_KEY_F8;
    Window->Platform.Win32.KeyCodes[0x043] = AX_KEY_F9;
    Window->Platform.Win32.KeyCodes[0x044] = AX_KEY_F10;
    Window->Platform.Win32.KeyCodes[0x057] = AX_KEY_F11;
    Window->Platform.Win32.KeyCodes[0x058] = AX_KEY_F12;
    Window->Platform.Win32.KeyCodes[0x064] = AX_KEY_F13;
    Window->Platform.Win32.KeyCodes[0x065] = AX_KEY_F14;
    Window->Platform.Win32.KeyCodes[0x066] = AX_KEY_F15;
    Window->Platform.Win32.KeyCodes[0x067] = AX_KEY_F16;
    Window->Platform.Win32.KeyCodes[0x068] = AX_KEY_F17;
    Window->Platform.Win32.KeyCodes[0x069] = AX_KEY_F18;
    Window->Platform.Win32.KeyCodes[0x06A] = AX_KEY_F19;
    Window->Platform.Win32.KeyCodes[0x06B] = AX_KEY_F20;
    Window->Platform.Win32.KeyCodes[0x06C] = AX_KEY_F21;
    Window->Platform.Win32.KeyCodes[0x06D] = AX_KEY_F22;
    Window->Platform.Win32.KeyCodes[0x06E] = AX_KEY_F23;
    Window->Platform.Win32.KeyCodes[0x076] = AX_KEY_F24;
    Window->Platform.Win32.KeyCodes[0x038] = AX_KEY_LEFT_ALT;
    Window->Platform.Win32.KeyCodes[0x01D] = AX_KEY_LEFT_CONTROL;
    Window->Platform.Win32.KeyCodes[0x02A] = AX_KEY_LEFT_SHIFT;
    Window->Platform.Win32.KeyCodes[0x15B] = AX_KEY_LEFT_SUPER;
    Window->Platform.Win32.KeyCodes[0x137] = AX_KEY_PRINT_SCREEN;
    Window->Platform.Win32.KeyCodes[0x138] = AX_KEY_RIGHT_ALT;
    Window->Platform.Win32.KeyCodes[0x11D] = AX_KEY_RIGHT_CONTROL;
    Window->Platform.Win32.KeyCodes[0x036] = AX_KEY_RIGHT_SHIFT;
    Window->Platform.Win32.KeyCodes[0x15C] = AX_KEY_RIGHT_SUPER;
    Window->Platform.Win32.KeyCodes[0x150] = AX_KEY_DOWN;
    Window->Platform.Win32.KeyCodes[0x14B] = AX_KEY_LEFT;
    Window->Platform.Win32.KeyCodes[0x14D] = AX_KEY_RIGHT;
    Window->Platform.Win32.KeyCodes[0x148] = AX_KEY_UP;

    Window->Platform.Win32.KeyCodes[0x052] = AX_KEY_KP_0;
    Window->Platform.Win32.KeyCodes[0x04F] = AX_KEY_KP_1;
    Window->Platform.Win32.KeyCodes[0x050] = AX_KEY_KP_2;
    Window->Platform.Win32.KeyCodes[0x051] = AX_KEY_KP_3;
    Window->Platform.Win32.KeyCodes[0x04B] = AX_KEY_KP_4;
    Window->Platform.Win32.KeyCodes[0x04C] = AX_KEY_KP_5;
    Window->Platform.Win32.KeyCodes[0x04D] = AX_KEY_KP_6;
    Window->Platform.Win32.KeyCodes[0x047] = AX_KEY_KP_7;
    Window->Platform.Win32.KeyCodes[0x048] = AX_KEY_KP_8;
    Window->Platform.Win32.KeyCodes[0x049] = AX_KEY_KP_9;
    Window->Platform.Win32.KeyCodes[0x04E] = AX_KEY_KP_ADD;
    Window->Platform.Win32.KeyCodes[0x053] = AX_KEY_KP_DECIMAL;
    Window->Platform.Win32.KeyCodes[0x135] = AX_KEY_KP_DIVIDE;
    Window->Platform.Win32.KeyCodes[0x11C] = AX_KEY_KP_ENTER;
    Window->Platform.Win32.KeyCodes[0x059] = AX_KEY_KP_EQUAL;
    Window->Platform.Win32.KeyCodes[0x037] = AX_KEY_KP_MULTIPLY;
    Window->Platform.Win32.KeyCodes[0x04A] = AX_KEY_KP_SUBTRACT;

    for (int16_t ScanCode = 0;  ScanCode < 512;  ScanCode++)
    {
        if (Window->Platform.Win32.KeyCodes[ScanCode] > 0) {
            Window->Platform.Win32.ScanCodes[Window->Platform.Win32.ScanCodes[ScanCode]] = ScanCode;
        }
    }
}

void InputKey(AxWindow *Window, int Key, int ScanCode, int Action, int Mods)
{
    AXON_ASSERT(Window);
    AXON_ASSERT(Key >= 0 || Key == AX_KEY_UNKNOWN);
    AXON_ASSERT(Key <= AX_KEY_LAST);
    AXON_ASSERT(Action == AX_PRESS || Action == AX_RELEASE);
    AXON_ASSERT(Mods == (Mods & AX_MOD_MASK));

    if (Key >= 0 && Key <= AX_KEY_LAST)
    {
        bool Repeated = false;
        if (Action == AX_RELEASE && Window->Keys[Key] == AX_RELEASE) {
            return;
        }

        if (Action == AX_PRESS && Window->Keys[Key] == AX_PRESS) {
            Repeated = true;
        }

        Window->Keys[Key] = (char)Action;

        if (Repeated) {
            Action = AX_REPEAT;
        }
    }

    if (Window->Callbacks.Key) {
        Window->Callbacks.Key(Window, Key, ScanCode, Action, Mods);
    }
}

void InputChar(AxWindow *Window, uint32_t CodePoint, int Mods, bool Plain)
{
    AXON_ASSERT(Window);
    AXON_ASSERT(Mods == (Mods & AX_MOD_MASK));

    if (CodePoint < 32 || (CodePoint > 126 && CodePoint < 160))
        return;

    // if (!Window->lockKeyMods)
    //     Mods &= ~(AX_MOD_CAPS_LOCK | AX_MOD_NUM_LOCK);

    // if (Window->Callbacks.charmods)
    //     Window->Callbacks.charmods(Window, CodePoint, Mods);

    if (Plain)
    {
        if (Window->Callbacks.Char)
            Window->Callbacks.Char(Window, CodePoint);
    }
}

void InputMouseClick(AxWindow *Window, int Button, int Action, int Mods)
{
    AXON_ASSERT(Window);
    AXON_ASSERT(Button >= 0);
    AXON_ASSERT(Action == AX_PRESS || Action == AX_RELEASE);
    AXON_ASSERT(Mods == (Mods & AX_MOD_MASK))

    // Update internal button state
    if (Button >= 0 && Button <= AX_MOUSE_BUTTON_LAST) {
        Window->MouseButtons[Button] = Action;
    }

    if (Window->Callbacks.MouseButton) {
        Window->Callbacks.MouseButton(Window, Button, Action, Mods);
    }
}

void InputMouseScroll(AxWindow *Window, AxVec2 Offset)
{
    AXON_ASSERT(Window);

    if (Window->Callbacks.Scroll) {
        Window->Callbacks.Scroll(Window, Offset);
    }
}

void InputMousePos(AxWindow *Window, AxVec2 Pos)
{
    AXON_ASSERT(Window);
    AXON_ASSERT(Pos.X > -FLT_MAX);
    AXON_ASSERT(Pos.X < FLT_MAX);
    AXON_ASSERT(Pos.Y > -FLT_MAX);
    AXON_ASSERT(Pos.Y < FLT_MAX);

    // Check if the mouse actually moved
    bool MouseMoved = (Pos.X != Window->VirtualCursorPos.X || Pos.Y != Window->VirtualCursorPos.Y);

    // Store previous position for delta calculation
    Window->LastMousePos = Window->VirtualCursorPos;
    Window->VirtualCursorPos = Pos;

    // Only call the callback if the mouse actually moved
    if (MouseMoved && Window->Callbacks.MousePos) {
        Window->Callbacks.MousePos(Window, Pos.X, Pos.Y);
    }
}

static int GetKeyModifiers(void)
{
    int Mods = 0;

    // Get all modifier states in one pass to minimize system calls
    SHORT CtrlState = GetKeyState(VK_CONTROL);
    SHORT AltState = GetKeyState(VK_MENU);
    SHORT ShiftState = GetKeyState(VK_SHIFT);
    SHORT LWinState = GetKeyState(VK_LWIN);
    SHORT RWinState = GetKeyState(VK_RWIN);
    SHORT CapsState = GetKeyState(VK_CAPITAL);
    SHORT NumState = GetKeyState(VK_NUMLOCK);

    if (CtrlState & 0x8000) {
        Mods |= AX_MOD_CONTROL;
    }

    if (AltState & 0x8000) {
        Mods |= AX_MOD_ALT;
    }

    if (ShiftState & 0x8000) {
        Mods |= AX_MOD_SHIFT;
    }

    if ((LWinState | RWinState) & 0x8000) {
        Mods |= AX_MOD_SUPER;
    }

    if (CapsState & 1) {
        Mods |= AX_MOD_CAPS_LOCK;
    }

    if (NumState & 1) {
        Mods |= AX_MOD_NUM_LOCK;
    }

    return Mods;
}

static DWORD GetWindowStyle(const AxWindow *Window)
{
    AXON_ASSERT(Window);
    if (!Window) {
        return (0);
    }

    // Clips all other overlapping sibling and child windows out of the draw region.
    // If these are not specified and they overlap, it is possible, when drawing within the client area
    // of a sibling or child window, to draw within the client area of a neighboring sibling or child window.
    DWORD Style = WS_CLIPSIBLINGS | WS_CLIPCHILDREN; // Default
    if (Window->Style & AX_WINDOW_STYLE_FULLSCREEN)
    {
        Style |= WS_POPUP;
    }
    else
    {
        Style |= WS_SYSMENU | WS_MINIMIZEBOX;
        if (Window->Style & AX_WINDOW_STYLE_DECORATED)
        {
            Style |= WS_CAPTION; // Title bar
            if (Window->Style & AX_WINDOW_STYLE_RESIZABLE) {
                Style |= WS_MAXIMIZEBOX | WS_THICKFRAME;
            }
        }
        else
        {
            Style |= WS_POPUP;
        }
    }

    return (Style);
}

static void UpdateCursorImage(const AxWindow *Window)
{
    AXON_ASSERT(Window);

    if (Window->CursorMode == AX_CURSOR_NORMAL) {
        SetCursor(LoadCursor(0, IDC_ARROW));
    } else {
        SetCursor(NULL);
    }
}

static LRESULT CALLBACK Win32MainWindowCallback(HWND Hwnd, UINT Message, WPARAM WParam, LPARAM LParam)
{
    AxWindow *Window = (AxWindow *)GetProp(Hwnd, "AxonEngine");
    if (!Window) {
        return DefWindowProc(Hwnd, Message, WParam, LParam);
    }

    LRESULT Result = 0;

    switch(Message)
    {
        case WM_CLOSE:
        {
            Window->IsRequestingClose = true;
        } break;

        // case WM_SIZE:
        // {
        //     // if (Window)
        //     // {
        //     //     RECT ClipRect;
        //     //     GetWindowRect((HWND)Window->Platform.Win32.Handle, &ClipRect);
        //     //     ClientToScreen((HWND)Window->Platform.Win32.Handle, (POINT *) &ClipRect.left);
        //     //     ClientToScreen((HWND)Window->Platform.Win32.Handle, (POINT *) &ClipRect.right);
        //     //     ClipCursor(&ClipRect);
        //     // }

        //     return (0);
        // }
        // NOTE(mdeforge): This takes the place of WM_MOVE and WM_SIZE
        // https://devblogs.microsoft.com/oldnewthing/20080115-00/?p=23813
        // https://devblogs.microsoft.com/oldnewthing/20080116-00/?p=23803
        case WM_WINDOWPOSCHANGING:
        {
            RECT ClientRect;
            GetWindowRect(Hwnd, &ClientRect);

            // TODO(mdeforge): If resizing is now allowed, enforce here

            Window->Rect = ClientRect;
            Result = DefWindowProc(Hwnd, Message, WParam, LParam);
        } break;

        case WM_SIZE:
        {
            // Detect window state changes
            enum AxWindowState OldState = GetWindowState(Window);
            enum AxWindowState NewState = AX_WINDOW_STATE_NORMAL;
            
            switch (WParam) {
                case SIZE_MINIMIZED:
                    NewState = AX_WINDOW_STATE_MINIMIZED;
                    break;
                case SIZE_MAXIMIZED:
                    NewState = AX_WINDOW_STATE_MAXIMIZED;
                    break;
                case SIZE_RESTORED:
                    if (Window->Style & AX_WINDOW_STYLE_FULLSCREEN) {
                        NewState = AX_WINDOW_STATE_FULLSCREEN;
                    } else {
                        NewState = AX_WINDOW_STATE_NORMAL;
                    }
                    break;
            }
            
            // Call state change callback if state actually changed
            if (OldState != NewState && Window->Callbacks.StateChanged) {
                Window->Callbacks.StateChanged(Window, OldState, NewState);
            }
            
            Result = DefWindowProc(Hwnd, Message, WParam, LParam);
        } break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            // Filter events if window doesn't have focus and filtering is enabled
            if (Window->FilterEventsWhenUnfocused && !Window->HasFocus) {
                break;
            }

            int Key, ScanCode;
            const int Action = (HIWORD(LParam) & KF_UP) ? AX_RELEASE : AX_PRESS;
            const int Mods = GetKeyModifiers();

            ScanCode = (HIWORD(LParam) & (KF_EXTENDED | 0xff));
            if (!ScanCode) {
                ScanCode = MapVirtualKey((UINT)WParam, MAPVK_VK_TO_VSC);
            }

            // Alt+PrtSc has a different scancode than just PrtSc
            if (ScanCode == 0x54)
                ScanCode = 0x137;

            // Ctrl+Pause has a different scancode than just Pause
            if (ScanCode == 0x146)
                ScanCode = 0x45;

            // CJK IME sets the extended bit for right Shift
            if (ScanCode == 0x136)
                ScanCode = 0x36;

            Key = Window->Platform.Win32.KeyCodes[ScanCode];

            // The Ctrl keys require special handling
            if (WParam == VK_CONTROL)
            {
                if (HIWORD(LParam) & KF_EXTENDED)
                {
                    // Right side keys have the extended key bit set
                    Key = AX_KEY_RIGHT_CONTROL;
                }
                else
                {
                    // NOTE: Alt Gr sends Left Ctrl followed by Right Alt
                    // HACK: We only want one event for Alt Gr, so if we detect
                    //       this sequence we discard this Left Ctrl message now
                    //       and later report Right Alt normally
                    MSG Next;
                    const DWORD Time = GetMessageTime();

                    if (PeekMessageW(&Next, NULL, 0, 0, PM_NOREMOVE))
                    {
                        if (Next.message == WM_KEYDOWN ||
                            Next.message == WM_SYSKEYDOWN ||
                            Next.message == WM_KEYUP ||
                            Next.message == WM_SYSKEYUP)
                        {
                            if (Next.wParam == VK_MENU &&
                                (HIWORD(Next.lParam) & KF_EXTENDED) &&
                                Next.time == Time) {
                                // Next message is Right Alt down so discard this
                                break;
                            }
                        }
                    }

                    // This is a regular Left Ctrl message
                    Key = AX_KEY_LEFT_CONTROL;
                }
            }
            else if (WParam == VK_PROCESSKEY)
            {
                // IME notifies that keys have been filtered by setting the
                // virtual key-code to VK_PROCESSKEY
                break;
            }

            if (Action == AX_RELEASE && WParam == VK_SHIFT)
            {
                // HACK: Release both Shift keys on Shift up event, as when both
                //       are pressed the first release does not emit any event
                // NOTE: The other half of this is in _glfwPollEventsWin32
                InputKey(Window, AX_KEY_LEFT_SHIFT, ScanCode, Action, Mods);
                InputKey(Window, AX_KEY_RIGHT_SHIFT, ScanCode, Action, Mods);
            }
            else if (WParam == VK_SNAPSHOT)
            {
                // HACK: Key down is not reported for the Print Screen key
                InputKey(Window, Key, ScanCode, AX_PRESS, Mods);
                InputKey(Window, Key, ScanCode, AX_RELEASE, Mods);
            }
            else
            {
                InputKey(Window, Key, ScanCode, Action, Mods);
            }
        } break;

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_XBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        case WM_XBUTTONUP:
        {
            // Filter events if window doesn't have focus and filtering is enabled
            if (Window->FilterEventsWhenUnfocused && !Window->HasFocus) {
                break;
            }

            int Button, Action;

            // XBUTTON1 and XBUTTON2 are often located on the sides of the mouse, near the base.
            if (Message == WM_LBUTTONDOWN || Message == WM_LBUTTONUP) {
                Button = AX_MOUSE_BUTTON_LEFT;
            } else if (Message == WM_RBUTTONDOWN || Message == WM_RBUTTONUP) {
                Button = AX_MOUSE_BUTTON_RIGHT;
            } else if (Message == WM_MBUTTONDOWN || Message == WM_MBUTTONUP) {
                Button = AX_MOUSE_BUTTON_MIDDLE;
            } else if (GET_XBUTTON_WPARAM(WParam) == XBUTTON1) {
                Button = AX_MOUSE_BUTTON_4;
            } else {
                Button = AX_MOUSE_BUTTON_5;
            }

            if (Message == WM_LBUTTONDOWN || Message == WM_RBUTTONDOWN ||
                Message == WM_MBUTTONDOWN || Message == WM_XBUTTONDOWN)
            {
                Action = AX_PRESS;
            }
            else
            {
                Action = AX_RELEASE;
            }

            int i = 0;
            for (i = 0; i <= AX_MOUSE_BUTTON_LAST; i++)
            {
                if (Window->MouseButtons[i] == AX_PRESS) {
                    break;
                }
            }

            if (i > AX_MOUSE_BUTTON_LAST) {
                SetCapture(Hwnd);
            }

            InputMouseClick(Window, Button, Action, GetKeyModifiers());

            for (i = 0; i <= AX_MOUSE_BUTTON_LAST; i++)
            {
                if (Window->MouseButtons[i] == AX_PRESS) {
                    break;
                }
            }

            if (i > AX_MOUSE_BUTTON_LAST) {
                ReleaseCapture();
            }

            if (Message == WM_XBUTTONDOWN || Message == WM_XBUTTONUP) {
                return TRUE;
            }

            return (0);
        }

        // NOTE(mdeforge): WM_MOUSEMOVE is only received when the mouse moves INSIDE the window OR while "captured"
        // https://docs.microsoft.com/en-us/windows/win32/learnwin32/mouse-movement
        // NOTE(mdeforge): WM_MOUSEMOVE should be used for GUI's, while WM_INPUT should be used otherwise
        // https://docs.microsoft.com/en-us/windows/win32/dxtecharts/taking-advantage-of-high-dpi-mouse-movement
        case WM_MOUSEMOVE:
        {
            // Filter events if window doesn't have focus and filtering is enabled
            if (Window->FilterEventsWhenUnfocused && !Window->HasFocus) {
                break;
            }

            const int32_t X = LOWORD(LParam);
            const int32_t Y = HIWORD(LParam);

            if(!Window->Platform.Win32.CursorInWindow)
            {
                // This sets things up to post a WM_LEAVE message when the mouse leaves the window
                TRACKMOUSEEVENT TME = {0};
                TME.cbSize = sizeof(TME);
                TME.dwFlags = TME_LEAVE;
                TME.hwndTrack = (HWND)Window->Platform.Win32.Handle;
                TrackMouseEvent(&TME);

                Window->Platform.Win32.CursorInWindow = true;
            }

            if (Window->CursorMode == AX_CURSOR_DISABLED) {
                break;
            } else {
                // Use the X and Y directly from WM_MOUSEMOVE (cursor bounded to the resolution)
                InputMousePos(Window, (AxVec2){(float)X, (float)Y});
            }

            Window->Platform.Win32.LastCursorPos = (AxVec2){ (float)X, (float)Y };

            return (0);
        }

        case WM_MOUSEWHEEL:
        {
            // Filter events if window doesn't have focus and filtering is enabled
            if (Window->FilterEventsWhenUnfocused && !Window->HasFocus) {
                break;
            }

            // We += here because it's possible to accrue a few messages before the value actually gets used
            AxVec2 Offset = { 0.0f, (float)GET_WHEEL_DELTA_WPARAM(WParam) / (float)WHEEL_DELTA };
            InputMouseScroll(Window, Offset);

            return (0);
        }

        case WM_MOUSEHWHEEL:
        {
            // Filter events if window doesn't have focus and filtering is enabled
            if (Window->FilterEventsWhenUnfocused && !Window->HasFocus) {
                break;
            }

            // Handle horizontal scroll
            AxVec2 Offset = { (float)GET_WHEEL_DELTA_WPARAM(WParam) / (float)WHEEL_DELTA, 0.0f };
            InputMouseScroll(Window, Offset);

            return (0);
        }

        case WM_INPUT:
        {
            // If the cursor mode is normal or hidden, and the keyboard is disabled, break. This section is for raw input.
            if ((Window->CursorMode != AX_CURSOR_DISABLED) && (Window->KeyboardMode == AX_KEYBOARD_DISABLED)) {
                break;
            }

            UINT RawInputSize;
            BYTE RawInputData[sizeof(RAWINPUT)] = {0};
            if (!GetRawInputData((HRAWINPUT)LParam, RID_INPUT, RawInputData, &RawInputSize, sizeof(RAWINPUTHEADER))) {
                break;
            }

            AxVec2 MouseDelta = {0};
            RAWINPUT *RawInput = (RAWINPUT *)RawInputData;

            if (RawInput->header.dwType == RIM_TYPEMOUSE)
            {
                if (RawInput->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)
                {
                    MouseDelta = (AxVec2)
                    {
                        .X = (float)RawInput->data.mouse.lLastX - Window->Platform.Win32.LastCursorPos.X,
                        .Y = (float)RawInput->data.mouse.lLastY - Window->Platform.Win32.LastCursorPos.Y
                    };
                }
                else
                {
                    MouseDelta = (AxVec2) {
                        .X = (float)RawInput->data.mouse.lLastX,
                        .Y = (float)RawInput->data.mouse.lLastY
                    };
                }
            }
            else if (RawInput->header.dwType == RIM_TYPEKEYBOARD)
            {
                // NOTE(mdeforge): https://blog.molecular-matters.com/2011/09/05/properly-handling-keyboard-input/
                const RAWKEYBOARD RawKeyboard = RawInput->data.keyboard;

                UINT VirtualKey = RawKeyboard.VKey;
                UINT ScanCode = RawKeyboard.MakeCode;
                UINT Flags = RawKeyboard.Flags;

                if (VirtualKey == 255) {
                    // Discard fake keys that are a part of an escape sequence
                }
                else if (VirtualKey == VK_SHIFT)
                {
                    // Correct left-hand/right-hand SHIFT
                    VirtualKey = MapVirtualKey(ScanCode, MAPVK_VSC_TO_VK_EX);
                }
                else if (VirtualKey == VK_NUMLOCK)
                {
                    // Correct pause/break and num lock issues, and set the extra extended bit
                    ScanCode = (MapVirtualKey(VirtualKey, MAPVK_VK_TO_VSC) | 0x100);
                }

                const bool IsEscSeq0 = ((Flags & RI_KEY_E0) != 0);
                const bool IsEscSeq1 = ((Flags & RI_KEY_E1) != 0);
                const bool WasUp = ((Flags & RI_KEY_BREAK) != 0);

                if (IsEscSeq1)
                {
                    // For escaped sequences, turn the virtual key into the correct scan code using MapVirtualKey.
                    // However, MapVirtualKey is unable to map VK_PAUSE (known bug), so we map it by hand.
                    if (VirtualKey == VK_PAUSE) {
                        ScanCode = 0x45;
                    } else {
                        ScanCode = MapVirtualKey(VirtualKey, MAPVK_VK_TO_VSC);
                    }
                }

                switch (VirtualKey)
                {
                    // Right-hand CONTROL and ALT have their E0 bit set
                    case VK_CONTROL:
                    {
                        //VirtualKey = (IsEscSeq0) ? AX_KEY_RIGHT_CTRL : AX_KEY_LEFT_CTRL;
                    } break;

                    case VK_MENU:
                    {
                        VirtualKey = (IsEscSeq0) ? AX_KEY_RIGHT_ALT : AX_KEY_LEFT_ALT;
                    } break;

                    // NUMPAD ENTER has its E0 bit set
                    case VK_RETURN:
                    {
                        if (IsEscSeq0) {
                            //VirtualKey = AX_KEY_NUMPAD_ENTER;
                        }
                    } break;

                    // The standard INSERT, DELETE, HOME, END, PRIOR, and NEXT keys will always have the E0 bit set
                    // but the same keys on the numpad will not.
                    case VK_INSERT:
                    {
                        if (!IsEscSeq0) {
                            //VirtualKey = AX_KEY_NUMPAD_0;
                        }
                    } break;

                    case VK_DELETE:
                    {
                        if (!IsEscSeq0) {
                            //VirtualKey = AX_KEY_NUMPAD_DECIMAL;
                        }
                    } break;

                    case VK_HOME:
                    {
                        if (!IsEscSeq0) {
                            //VirtualKey = AX_KEY_NUMPAD_7;
                        }
                    } break;

                    case VK_END:
                    {
                        if (!IsEscSeq0) {
                            //VirtualKey = AX_KEY_NUMPAD_1;
                        }
                    } break;

                    case VK_PRIOR:
                    {
                        if (!IsEscSeq0) {
                            //VirtualKey = AX_KEY_NUMPAD_9;
                        }
                    } break;

                    case VK_NEXT:
                    {
                        if (!IsEscSeq0) {
                            //VirtualKey = AX_KEY_NUMPAD_3;
                        }
                    } break;

                    // The standard arrow keys will always have the E0 bit set, but the same keys
                    // on the numpad will not.
                    case VK_UP:
                    {
                        if (!IsEscSeq0) {
                            //VirtualKey = AX_KEY_NUMPAD_8;
                        }
                    } break;

                    case VK_DOWN:
                    {
                        if (!IsEscSeq0) {
                            //VirtualKey = AX_KEY_NUMPAD_2;
                        }
                    } break;

                    case VK_LEFT:
                    {
                        if (!IsEscSeq0) {
                            //VirtualKey = AX_KEY_NUMPAD_4;
                        }
                    } break;

                    case VK_RIGHT:
                    {
                        if (!IsEscSeq0) {
                            //VirtualKey = AX_KEY_NUMPAD_6;
                        }
                    } break;
                }
            }

            InputMousePos(Window, Vec2Add(Window->VirtualCursorPos, MouseDelta));
            Window->Platform.Win32.LastCursorPos = Vec2Add(Window->Platform.Win32.LastCursorPos, MouseDelta);

            break;
        }

        case WM_MOUSELEAVE:
        {
            Window->Platform.Win32.CursorInWindow = false;

            return (0);
        }

        case WM_SETFOCUS:
        {
            Window->HasFocus = true;
            return (0);
        }

        case WM_KILLFOCUS:
        {
            Window->HasFocus = false;
            return (0);
        }

        case WM_SYSCOMMAND:
        {
            // Handle system commands that change window state
            switch (WParam & 0xFFF0) {
                case SC_MINIMIZE:
                case SC_MAXIMIZE:
                case SC_RESTORE:
                {
                    // Get current state before the command is processed
                    enum AxWindowState OldState = GetWindowState(Window);

                    // Let the default handler process the command
                    Result = DefWindowProc(Hwnd, Message, WParam, LParam);

                    // Get the new state after processing
                    enum AxWindowState NewState = GetWindowState(Window);

                    // Call state change callback if state actually changed
                    if (OldState != NewState && Window->Callbacks.StateChanged) {
                        Window->Callbacks.StateChanged(Window, OldState, NewState);
                    }

                    return Result;
                }
                break;

                default:
                    Result = DefWindowProc(Hwnd, Message, WParam, LParam);
                    break;
            }
        } break;

        case WM_CHAR:
        case WM_SYSCHAR:
        {
            // Filter events if window doesn't have focus and filtering is enabled
            if (Window->FilterEventsWhenUnfocused && !Window->HasFocus) {
                break;
            }

            if (WParam >= 0xd800 && WParam <= 0xdbff)
                Window->Platform.Win32.HighSurrogate = (WCHAR)WParam;
            else
            {
                uint32_t codepoint = 0;

                if (WParam >= 0xdc00 && WParam <= 0xdfff)
                {
                    if (Window->Platform.Win32.HighSurrogate)
                    {
                        codepoint += (Window->Platform.Win32.HighSurrogate - 0xd800) << 10;
                        codepoint += (WCHAR)WParam - 0xdc00;
                        codepoint += 0x10000;
                    }
                }
                else
                    codepoint = (WCHAR) WParam;

                Window->Platform.Win32.HighSurrogate = 0;
                InputChar(Window, codepoint, GetKeyModifiers(), Message != WM_SYSCHAR);
            }

            // TODO(mdeforge): There should be a  && Window->Platform.Win32.keymenu here
            if (Message == WM_SYSCHAR) {
                break;
            }

            return 0;
        }

        // Only useful if we need to scale the window non-linearly
        // case WM_GETDPISCALEDSIZE:
        // {
        //     RECT Source = {0}, Target = {0};
        //     SIZE *Size = (SIZE *)LParam;

        //     UINT DPI = GetNearestMonitorDPI(Window);
        //     DWORD Style = GetWindowStyle(Window);
        //     AdjustWindowRectExForDpi(&Source, Style, FALSE, 0, DPI);

        //     AdjustWindowRectExForDpi(&Target, Style, FALSE, 0, LOWORD(WParam));

        //     Size->cx += (Target.right - Target.left) -
        //                 (Source.right - Source.left);
        //     Size->cy += (Target.bottom - Target.top) -
        //                 (Source.bottom - Source.top);

        //     return (TRUE);
        // };

        // NOTE(mdeforge): When this gets triggered, it's because the DPI
        // has changed at runtime either because the user has moved to a new
        // monitor with a different DPI or the DPI of the monitor has changed.
        // The LParam contains a pointer to a RECT that provides a new suggested
        // size and position of the window for the new DPI which is why we don't
        // need to call AdjustWindowRectExForDpi ourselves.
        // https://docs.microsoft.com/en-us/windows/win32/hidpi/wm-dpichanged
        case WM_DPICHANGED:
        {
            // DPI changes don't affect fullscreen windows
            if (Window->Style & ~AX_WINDOW_STYLE_FULLSCREEN)
            {
                RECT *Rect = (RECT *)LParam;
                SetWindowPos((HWND)Window->Platform.Win32.Handle, NULL,
                    Rect->left,
                    Rect->top,
                    Rect->right - Rect->left,
                    Rect->bottom - Rect->top,
                    SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }

        // NOTE(mdeforge): Not sure we're going to be polling displays anymore
        // The display resolution has changed
        // case WM_DISPLAYCHANGE:
        // {
        //     PollDisplays();
        //     break;
        // }

        // NOTE(mdeforge): Warning, using this prevents the resize cursor
        case WM_SETCURSOR:
        {
            if (LOWORD(LParam) == HTCLIENT) {
                UpdateCursorImage(Window);

                return (TRUE);
            }
        } break;

        default:
        {
            Result = DefWindowProc(Hwnd, Message, WParam, LParam);
        } break;
    }

    return(Result);
}

static bool Win32RegisterWindowClass()
{
    WNDCLASSEX WNDClass;
    ZeroMemory(&WNDClass, sizeof(WNDCLASSEX));

    WNDClass.cbSize = sizeof(WNDCLASSEX);
    WNDClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    WNDClass.lpfnWndProc = (WNDPROC)Win32MainWindowCallback;
    WNDClass.cbClsExtra = 0;
    WNDClass.cbWndExtra = 0;
    WNDClass.hInstance = GetModuleHandle(0);
    WNDClass.hCursor = LoadCursor(0, IDC_ARROW);
    //WNDClass.hbrBackground = NULL; // Setting this to ANYTHING causes black window while resizing
    //WNDClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // Using this doesn't work with DPI changes
    WNDClass.lpszMenuName = NULL;
    WNDClass.lpszClassName = AXON_WNDCLASSNAME;
    WNDClass.hIconSm = NULL;

    // TODO(mdeforge): Load user provided icon if available
    WNDClass.hIcon = NULL;

    if (!RegisterClassEx(&WNDClass))
    {
        DWORD Error = GetLastError();
        fprintf(stderr, "Failed to register window class: %lu\n", Error);
        MessageBox(NULL, "Window Registration Failed!", "Abandon Ship!",
            MB_ICONEXCLAMATION | MB_OK);

        return (false);
    }

    return (true);
}

// Gets the DPI of the monitor the window is currently mostly on
// NOTE(mdeforge): Basically does what GetDpiForWindow() does, but is compatible back to Windows 8.1
static UINT GetNearestMonitorDPI(const AxWindow *Window)
{
    AXON_ASSERT(Window);

    UINT XDPI, YDPI;
    HANDLE MonitorHandle = MonitorFromWindow((HWND)Window->Platform.Win32.Handle, MONITOR_DEFAULTTONEAREST);
    LRESULT Result = GetDpiForMonitor(MonitorHandle, 0, &XDPI, &YDPI); // NOTE(mdeforge): Requires Windows 8.1 or higher

    return ((Result == S_OK) ? XDPI : USER_DEFAULT_SCREEN_DPI);
}

static bool CreateNativeWindow(AxWindow *Window)
{
    AXON_ASSERT(Window);

    if (!Window) {
        return (false);
    }

    DWORD Style = GetWindowStyle(Window);

    if (Window->Style & AX_WINDOW_STYLE_FULLSCREEN ||
        Window->Style & AX_WINDOW_STYLE_MAXIMIZED)
    {
        Style |= WS_MAXIMIZE;
    }

    UINT DPI = GetNearestMonitorDPI(Window);
    AdjustWindowRectExForDpi((LPRECT)&Window->Rect, Style, FALSE, 0, DPI);

    // TODO(mdeforge): If centered flag, calculate display center
    HWND Handle = CreateWindowEx(
        0, //WS_EX_TOPMOST | WS_EX_LAYERED,
        AXON_WNDCLASSNAME,
        Window->Title,
        Style,
        Window->Rect.left,
        Window->Rect.top,
        Window->Rect.right - Window->Rect.left,
        Window->Rect.bottom - Window->Rect.top,
        NULL,
        NULL,
        GetModuleHandle(0),
        NULL);

    if (!Handle)
    {
        DWORD Error = GetLastError();
        MessageBox(NULL, "Window Creation Failed!", "Abandon Ship!", MB_ICONEXCLAMATION | MB_OK);

        return (false);
    }

    // Add the AxWindow pointer to the window property list
    SetProp(Handle, "AxonEngine", Window);
    Window->Platform.Win32.Handle = (uint64_t)Handle;
    //Window->Platform.Win32.KeyMenu = 

    // Create key table for platform
    CreateKeyTable(Window);

    // If fullscreen, disable legacy window messages such as WM_KEYDOWN, WM_CHAR, WM_MOUSEMOVE, etc.
    DWORD Flags = 0;
    if (Window->Style & AX_WINDOW_STYLE_FULLSCREEN) {
        Flags = RIDEV_NOLEGACY;
    }

    // Register raw input devices for this window
    RAWINPUTDEVICE RawInputDevice[2];
    RawInputDevice[0] = (RAWINPUTDEVICE){ HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_MOUSE, Flags, Handle };
    RawInputDevice[1] = (RAWINPUTDEVICE){ HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_KEYBOARD, Flags, Handle };

    if (!RegisterRawInputDevices(RawInputDevice, 2, sizeof(RAWINPUTDEVICE))) {
        return (false);
    }

    return (true);
}

static enum AxWindowError Init(void)
{
    // Set DPI awareness - this should be done before any window creation
    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        DWORD Error = GetLastError();
        fprintf(stderr, "Failed to set DPI awareness: %lu\n", Error);
        return AX_WINDOW_ERROR_UNKNOWN;
    }

    // Register window class
    if (!Win32RegisterWindowClass()) {
        return AX_WINDOW_ERROR_WINDOW_REGISTRATION_FAILED;
    }

    return AX_WINDOW_ERROR_NONE;
}

static void DestroyWindow_(AxWindow *Window)
{
    AXON_ASSERT(Window);

    if (Window == NULL) {
        return;
    }

    if (Window->Platform.Win32.Handle)
    {
        RemoveProp((HWND)Window->Platform.Win32.Handle, "AxonEngine");
        DestroyWindow((HWND)Window->Platform.Win32.Handle);
        Window->Platform.Win32.Handle = 0;

        free(Window);
    }
}

static AxWindow *CreateWindow_(const char *Title, int32_t X, int32_t Y, int32_t Width, int32_t Height, enum AxWindowStyle Style, enum AxWindowError *Error)
{
    AXON_ASSERT(Title != NULL);

    SET_ERROR(Error, AX_WINDOW_ERROR_NONE);

    if (Width <= 0 || Height <= 0) {
        SET_ERROR(Error, AX_WINDOW_ERROR_INVALID_PARAMETERS);
        return NULL;
    }

    RECT Rect = {
        .left = X,
        .top = Y,
        .right = X + Width,
        .bottom = Y + Height
    };

    AxWindow *Window = calloc(1, sizeof(AxWindow));
    if (!Window) {
        SET_ERROR(Error, AX_WINDOW_ERROR_OUT_OF_MEMORY);
        return NULL;
    }

    Window->Platform.Win32.Instance = (uint64_t)GetModuleHandle(0);
    Window->IsRequestingClose = false;
    Window->Title = Title;
    Window->Rect = Rect;
    Window->Style = Style;
    Window->HasFocus = false;
    Window->FilterEventsWhenUnfocused = true;  // Default to filtering events when unfocused
    Window->LastMousePos = (AxVec2){0, 0};

    if (!CreateNativeWindow(Window)) {
        SET_ERROR(Error, AX_WINDOW_ERROR_WINDOW_CREATION_FAILED);
        DestroyWindow_(Window);
        return NULL;
    }

    if (Style & AX_WINDOW_STYLE_FULLSCREEN ||
        Style & AX_WINDOW_STYLE_VISIBLE)
    {
        ShowWindow((HWND)Window->Platform.Win32.Handle, SW_SHOW);
        SetFocus((HWND)Window->Platform.Win32.Handle);
        UpdateWindow((HWND)Window->Platform.Win32.Handle);
    }

    return Window;
}

static AxWindow *CreateWindowWithConfig(const AxWindowConfig *Config, enum AxWindowError *Error)
{
    SET_ERROR(Error, AX_WINDOW_ERROR_NONE);

    if (!Config) {
        SET_ERROR(Error, AX_WINDOW_ERROR_INVALID_PARAMETERS);
        return NULL;
    }

    // Validate required fields
    if (!Config->Title) {
        SET_ERROR(Error, AX_WINDOW_ERROR_INVALID_PARAMETERS);
        return NULL;
    }

    if (Config->Width <= 0 || Config->Height <= 0) {
        SET_ERROR(Error, AX_WINDOW_ERROR_INVALID_PARAMETERS);
        return NULL;
    }

    // Create the window with the provided config
    return CreateWindow_(Config->Title, Config->X, Config->Y, Config->Width, Config->Height, Config->Style, Error);
}

static AxWindow *CreateWindowDefault(const char *Title, enum AxWindowError *Error)
{
    SET_ERROR(Error, AX_WINDOW_ERROR_NONE);

    // Use default title if none provided
    const char *FinalTitle = Title ? Title : "Axon Window";

    // Default configuration: centered, 800x600, decorated, visible
    return CreateWindow_(FinalTitle, -1, -1, 800, 600, 
                        AX_WINDOW_STYLE_VISIBLE | AX_WINDOW_STYLE_DECORATED, Error);
}

static void PollEvents(AxWindow *Window)
{
    AXON_ASSERT(Window);

    MSG Msg = {0};

    // Process messages with Windows 10 compatibility fix
    // Skip problematic message 0x738 that can cause system hangs
    while (PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE))
    {
        // Skip problematic Windows 10 message 0x738 (1848)
        // This message can cause system hangs and input lag on Windows 10
        if (Msg.message == 0x738) {
            continue;
        }

        if (Msg.message == WM_QUIT)
        {
            Window->IsRequestingClose = true;
            break;
        }

        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }

    // Handle modifier key state synchronization
    // This ensures our internal state matches the actual Windows key state
    const struct {
        int VK;
        int Key;
    } ModifierKeys[] = {
        { VK_LSHIFT, AX_KEY_LEFT_SHIFT },
        { VK_RSHIFT, AX_KEY_RIGHT_SHIFT },
        { VK_LCONTROL, AX_KEY_LEFT_CONTROL },
        { VK_RCONTROL, AX_KEY_RIGHT_CONTROL },
        { VK_LMENU, AX_KEY_LEFT_ALT },
        { VK_RMENU, AX_KEY_RIGHT_ALT },
        { VK_LWIN, AX_KEY_LEFT_SUPER },
        { VK_RWIN, AX_KEY_RIGHT_SUPER }
    };

    // Check each modifier key for state changes
    for (int32_t i = 0; i < 8; i++)
    {
        const int vk = ModifierKeys[i].VK;
        const int key = ModifierKeys[i].Key;
        const int scanCode = Window->Platform.Win32.ScanCodes[key];
        const bool isPressed = (GetKeyState(vk) & 0x8000) != 0;
        const bool wasPressed = (Window->Keys[key] == AX_PRESS);

        // Generate events only when state actually changes
        if (isPressed && !wasPressed)
        {
            // Key was just pressed
            InputKey(Window, key, scanCode, AX_PRESS, GetKeyModifiers());
        }
        else if (!isPressed && wasPressed)
        {
            // Key was just released
            InputKey(Window, key, scanCode, AX_RELEASE, GetKeyModifiers());
        }
    }
}

static bool HasRequestedClose(AxWindow *Window)
{
    AXON_ASSERT(Window);

    if (Window)
    {
        return Window->IsRequestingClose;
    }

    return (false);
}

static void GetWindowPosition(AxWindow *Window, int32_t *X, int32_t *Y)
{
    AXON_ASSERT(Window);

    *X = 0;
    *Y = 0;

    if (Window)
    {
        RECT Rect = { 0 };
        GetClientRect((HWND)Window->Platform.Win32.Handle, &Rect);

        *X = Rect.left;
        *Y = Rect.top;
    }
}

static bool SetWindowPosition(AxWindow *Window, int32_t X, int32_t Y, enum AxWindowError *Error)
{
    AXON_ASSERT(Window);

    SET_ERROR(Error, AX_WINDOW_ERROR_NONE);

    if (!Window) {
        SET_ERROR(Error, AX_WINDOW_ERROR_INVALID_WINDOW);
        return false;
    }

    if (Window->Style & AX_WINDOW_STYLE_FULLSCREEN) {
        SET_ERROR(Error, AX_WINDOW_ERROR_INVALID_STATE);
        return false;
    }

    RECT Rect = { X, Y, 0, 0 };
    UINT DPI = GetNearestMonitorDPI(Window);

    // Adjust size to account for non-client area
    DWORD Style = GetWindowStyle(Window);
    AdjustWindowRectExForDpi((LPRECT)&Rect, Style, FALSE, 0, DPI);
    
    if (!SetWindowPos((HWND)Window->Platform.Win32.Handle, NULL,
        Rect.left, Rect.top, 0, 0,
        SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE)) {
        SET_ERROR(Error, AX_WINDOW_ERROR_OPERATION_FAILED);
        return false;
    }

    Window->Rect = Rect;
    return true;
}

static void GetWindowSize(AxWindow *Window, int32_t *Width, int32_t *Height)
{
    AXON_ASSERT(Window);

    *Width = 0;
    *Height = 0;

    if (Window)
    {
        RECT Rect = { 0 };
        GetClientRect((HWND)Window->Platform.Win32.Handle, &Rect);

        *Width = Rect.right - Rect.left;
        *Height = Rect.bottom - Rect.top;
    }
}

static bool SetWindowSize(AxWindow *Window, int32_t Width, int32_t Height, enum AxWindowError *Error)
{
    AXON_ASSERT(Window);

    SET_ERROR(Error, AX_WINDOW_ERROR_NONE);

    if (!Window) {
        SET_ERROR(Error, AX_WINDOW_ERROR_INVALID_WINDOW);
        return false;
    }

    if (Window->Style & AX_WINDOW_STYLE_FULLSCREEN) {
        SET_ERROR(Error, AX_WINDOW_ERROR_INVALID_STATE);
        return false;
    }

    RECT Rect = { 0, 0, Width, Height };
    UINT DPI = GetNearestMonitorDPI(Window);
    DWORD Style = GetWindowStyle(Window);

    AdjustWindowRectExForDpi((LPRECT)&Rect, Style, FALSE, 0, DPI);
    
    if (!SetWindowPos((HWND)Window->Platform.Win32.Handle, NULL,
        0, 0, Rect.right - Rect.left, Rect.bottom - Rect.top,
        SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOZORDER)) {
        SET_ERROR(Error, AX_WINDOW_ERROR_OPERATION_FAILED);
        return false;
    }

    Window->Rect = Rect;
    return true;
}

static bool SetWindowVisible(AxWindow *Window, bool IsVisible, enum AxWindowError *Error)
{
    AXON_ASSERT(Window);

    SET_ERROR(Error, AX_WINDOW_ERROR_NONE);

    if (!Window) {
        SET_ERROR(Error, AX_WINDOW_ERROR_INVALID_WINDOW);
        return false;
    }

    int ShowCommand = IsVisible ? SW_SHOW : SW_HIDE;
    if (!ShowWindow((HWND)Window->Platform.Win32.Handle, ShowCommand)) {
        SET_ERROR(Error, AX_WINDOW_ERROR_OPERATION_FAILED);
        return false;
    }

    return true;
}

static enum AxWindowState GetWindowState(const AxWindow *Window)
{
    AXON_ASSERT(Window);

    if (!Window) {
        return AX_WINDOW_STATE_NORMAL;
    }

    WINDOWPLACEMENT Placement = {0};
    Placement.length = sizeof(WINDOWPLACEMENT);
    
    if (!GetWindowPlacement((HWND)Window->Platform.Win32.Handle, &Placement)) {
        return AX_WINDOW_STATE_NORMAL;
    }

    if (Placement.showCmd == SW_SHOWMINIMIZED) {
        return AX_WINDOW_STATE_MINIMIZED;
    } else if (Placement.showCmd == SW_SHOWMAXIMIZED) {
        return AX_WINDOW_STATE_MAXIMIZED;
    } else if (Window->Style & AX_WINDOW_STYLE_FULLSCREEN) {
        return AX_WINDOW_STATE_FULLSCREEN;
    }

    return AX_WINDOW_STATE_NORMAL;
}

static bool SetWindowStateEnum(AxWindow *Window, enum AxWindowState State, enum AxWindowError *Error)
{
    AXON_ASSERT(Window);

    SET_ERROR(Error, AX_WINDOW_ERROR_NONE);

    if (!Window) {
        SET_ERROR(Error, AX_WINDOW_ERROR_INVALID_WINDOW);
        return false;
    }

    // Get current state for callback
    enum AxWindowState OldState = GetWindowState(Window);

    int ShowCommand = SW_SHOW;
    bool StyleChanged = false;

    switch (State) {
        case AX_WINDOW_STATE_NORMAL:
            ShowCommand = SW_SHOWNORMAL;
            // Remove fullscreen style if present
            if (Window->Style & AX_WINDOW_STYLE_FULLSCREEN) {
                Window->Style &= ~AX_WINDOW_STYLE_FULLSCREEN;
                StyleChanged = true;
            }
            break;

        case AX_WINDOW_STATE_MINIMIZED:
            ShowCommand = SW_SHOWMINIMIZED;
            break;

        case AX_WINDOW_STATE_MAXIMIZED:
            ShowCommand = SW_SHOWMAXIMIZED;
            // Remove fullscreen style if present
            if (Window->Style & AX_WINDOW_STYLE_FULLSCREEN) {
                Window->Style &= ~AX_WINDOW_STYLE_FULLSCREEN;
                StyleChanged = true;
            }
            break;

        case AX_WINDOW_STATE_FULLSCREEN:
            ShowCommand = SW_SHOW;
            // Add fullscreen style
            if (!(Window->Style & AX_WINDOW_STYLE_FULLSCREEN)) {
                Window->Style |= AX_WINDOW_STYLE_FULLSCREEN;
                StyleChanged = true;
            }
            break;

        default:
            SET_ERROR(Error, AX_WINDOW_ERROR_INVALID_PARAMETERS);
            return false;
    }

    // Apply the state change
    if (!ShowWindow((HWND)Window->Platform.Win32.Handle, ShowCommand)) {
        SET_ERROR(Error, AX_WINDOW_ERROR_OPERATION_FAILED);
        return false;
    }

    // Call state change callback if state actually changed
    if (OldState != State && Window->Callbacks.StateChanged) {
        Window->Callbacks.StateChanged(Window, OldState, State);
    }

    return true;
}

static bool GetWindowStateInfo(const AxWindow *Window, AxWindowStateInfo *StateInfo, enum AxWindowError *Error)
{
    AXON_ASSERT(Window);
    AXON_ASSERT(StateInfo);

    SET_ERROR(Error, AX_WINDOW_ERROR_NONE);

    if (!Window) {
        SET_ERROR(Error, AX_WINDOW_ERROR_INVALID_WINDOW);
        return false;
    }

    if (!StateInfo) {
        SET_ERROR(Error, AX_WINDOW_ERROR_INVALID_PARAMETERS);
        return false;
    }

    // Get current window state
    WINDOWPLACEMENT Placement = {0};
    Placement.length = sizeof(WINDOWPLACEMENT);
    
    if (!GetWindowPlacement((HWND)Window->Platform.Win32.Handle, &Placement)) {
        SET_ERROR(Error, AX_WINDOW_ERROR_OPERATION_FAILED);
        return false;
    }

    // Determine window state from placement
    enum AxWindowState State = AX_WINDOW_STATE_NORMAL;
    if (Placement.showCmd == SW_SHOWMINIMIZED) {
        State = AX_WINDOW_STATE_MINIMIZED;
    } else if (Placement.showCmd == SW_SHOWMAXIMIZED) {
        State = AX_WINDOW_STATE_MAXIMIZED;
    } else if (Window->Style & AX_WINDOW_STYLE_FULLSCREEN) {
        State = AX_WINDOW_STATE_FULLSCREEN;
    }

    // Get position and size
    RECT ClientRect = {0};
    GetClientRect((HWND)Window->Platform.Win32.Handle, &ClientRect);

    // Get visibility
    bool IsVisible = (IsWindowVisible((HWND)Window->Platform.Win32.Handle) != 0);

    // Fill the state info structure
    StateInfo->State = State;
    StateInfo->X = ClientRect.left;
    StateInfo->Y = ClientRect.top;
    StateInfo->Width = ClientRect.right - ClientRect.left;
    StateInfo->Height = ClientRect.bottom - ClientRect.top;
    StateInfo->IsVisible = IsVisible;
    StateInfo->Style = Window->Style;

    return true;
}

static bool SetWindowState(AxWindow *Window, const AxWindowStateInfo *StateInfo, enum AxWindowError *Error)
{
    AXON_ASSERT(Window);
    AXON_ASSERT(StateInfo);

    SET_ERROR(Error, AX_WINDOW_ERROR_NONE);

    if (!Window) {
        SET_ERROR(Error, AX_WINDOW_ERROR_INVALID_WINDOW);
        return false;
    }

    if (!StateInfo) {
        SET_ERROR(Error, AX_WINDOW_ERROR_INVALID_PARAMETERS);
        return false;
    }

    // Validate parameters
    if (StateInfo->Width <= 0 || StateInfo->Height <= 0) {
        SET_ERROR(Error, AX_WINDOW_ERROR_INVALID_PARAMETERS);
        return false;
    }

    // Store old state for callback
    enum AxWindowState OldState = GetWindowState(Window);
    WINDOWPLACEMENT Placement = {0};
    Placement.length = sizeof(WINDOWPLACEMENT);
    if (GetWindowPlacement((HWND)Window->Platform.Win32.Handle, &Placement)) {
        if (Placement.showCmd == SW_SHOWMINIMIZED) {
            OldState = AX_WINDOW_STATE_MINIMIZED;
        } else if (Placement.showCmd == SW_SHOWMAXIMIZED) {
            OldState = AX_WINDOW_STATE_MAXIMIZED;
        } else if (Window->Style & AX_WINDOW_STYLE_FULLSCREEN) {
            OldState = AX_WINDOW_STATE_FULLSCREEN;
        }
    }

    // Update window style if needed
    if (StateInfo->Style != Window->Style) {
        Window->Style = StateInfo->Style;
        // TODO: Recreate window with new style if needed
    }

    // Set position and size
    if (!SetWindowPosition(Window, StateInfo->X, StateInfo->Y, Error)) {
        return false;
    }

    if (!SetWindowSize(Window, StateInfo->Width, StateInfo->Height, Error)) {
        return false;
    }

    // Set visibility
    if (!SetWindowVisible(Window, StateInfo->IsVisible, Error)) {
        return false;
    }

    // Set window state
    if (!SetWindowStateEnum(Window, StateInfo->State, Error)) {
        return false;
    }

    // Call state change callback if state actually changed
    if (OldState != StateInfo->State && Window->Callbacks.StateChanged) {
        Window->Callbacks.StateChanged(Window, OldState, StateInfo->State);
    }

    return true;
}

static AxWindowPlatformData GetPlatformData(const AxWindow *Window)
{
    AXON_ASSERT(Window);

    AxWindowPlatformData Data = {0};
    if (Window) {
        Data = Window->Platform;
    }

    return (Data);
}

static void GetMouseCoords(const AxWindow *Window, AxVec2 *Position)
{
    AXON_ASSERT(Window);

    if (Position) {
        *Position = (AxVec2){0};
    }

    // TODO(mdeforge): Does cursor mode have an effect on this?
    // if (Window->CursorMode == AX_CURSOR_DISABLED)
    // {
    //     if (Position) {
            *Position = Window->VirtualCursorPos;
    //     }
    // }
}

static int32_t GetMouseButton(const AxWindow *Window, int32_t Button)
{
    AXON_ASSERT(Window);

    if (Button < AX_MOUSE_BUTTON_1 || Button > AX_MOUSE_BUTTON_LAST) {
        return AX_RELEASE;
    }

    return (Window->MouseButtons[Button]);
}

// TODO(mdeforge): Update cursor image using enable/disable cursor functions
static bool SetCursorMode(AxWindow *Window, enum AxCursorMode CursorMode, enum AxWindowError *Error)
{
    AXON_ASSERT(Window && "Window is NULL in SetCursorMode()!");

    SET_ERROR(Error, AX_WINDOW_ERROR_NONE);

    if (!Window) {
        SET_ERROR(Error, AX_WINDOW_ERROR_INVALID_WINDOW);
        return false;
    }

    // TODO(mdeforge): Validate CursorMode?
    Window->CursorMode = CursorMode;
    UpdateCursorImage(Window);
    
    return true;
}

static enum AxCursorMode GetCursorMode(AxWindow *Window)
{
  AXON_ASSERT(Window);

  return (Window->CursorMode);
}

static bool SetKeyboardMode(AxWindow *Window, enum AxKeyboardMode KeyboardMode, enum AxWindowError *Error)
{
    AXON_ASSERT(Window);

    SET_ERROR(Error, AX_WINDOW_ERROR_NONE);

    if (!Window) {
        SET_ERROR(Error, AX_WINDOW_ERROR_INVALID_WINDOW);
        return false;
    }

    // TODO(mdeforge): Validate KeyboardMode?
    Window->KeyboardMode = KeyboardMode;
    
    return true;
}

static int32_t GetKey(AxWindow *Window, int32_t Key)
{
    AXON_ASSERT(Window);

    if (Key < AX_KEY_SPACE || Key > AX_KEY_LAST)
    {
        // TODO(mdeforge): Invalid key error
        return AX_RELEASE;
    }

    return (Window->Keys[Key]);
}

static AxFileDialogResult OpenFileDialog(const AxWindow *Window, const char *Title, const char *Filter, const char *InitialDirectory)
{
    AxFileDialogResult result = {0};
    result.Success = false;
    result.Error = AX_WINDOW_ERROR_NONE;
    result.FilePath[0] = '\0';

    OPENFILENAME OpenFileName;
    char FileName[1024] = {0};
    ZeroMemory(&OpenFileName, sizeof(OPENFILENAME));
    OpenFileName.lStructSize = sizeof(OPENFILENAME);
    OpenFileName.hwndOwner = (HWND)Window->Platform.Win32.Handle;
    OpenFileName.lpstrFile = FileName;
    OpenFileName.nMaxFile = (DWORD)sizeof(FileName);
    OpenFileName.lpstrFilter = (Filter) ? Filter : TEXT("All files\0*.*\0\0");
    OpenFileName.nFilterIndex = 1;
    OpenFileName.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    OpenFileName.lpstrInitialDir = InitialDirectory;
    OpenFileName.lpstrTitle = (Title) ? Title : TEXT("Open File");

    if (GetOpenFileName(&OpenFileName) == TRUE)
    {
        strncpy(result.FilePath, FileName, sizeof(result.FilePath) - 1);
        result.Success = true;
    }
    else
    {
        DWORD err = CommDlgExtendedError();
        result.Error = (err != 0) ? AX_WINDOW_ERROR_OPERATION_FAILED : AX_WINDOW_ERROR_NONE;
    }
    return result;
}

static AxFileDialogResult SaveFileDialog(const AxWindow *Window, const char *Title, const char *Filter, const char *InitialDirectory)
{
    AxFileDialogResult result = {0};
    result.Success = false;
    result.Error = AX_WINDOW_ERROR_NONE;
    result.FilePath[0] = '\0';

    OPENFILENAME OpenFileName;
    char FileName[1024] = {0};
    ZeroMemory(&OpenFileName, sizeof(OPENFILENAME));
    OpenFileName.lStructSize = sizeof(OPENFILENAME);
    OpenFileName.hwndOwner = (HWND)Window->Platform.Win32.Handle;
    OpenFileName.lpstrFile = FileName;
    OpenFileName.nMaxFile = (DWORD)sizeof(FileName);
    OpenFileName.lpstrFilter = (Filter) ? Filter : TEXT("All files\0*.*\0\0");
    OpenFileName.nFilterIndex = 1;
    OpenFileName.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    OpenFileName.lpstrInitialDir = InitialDirectory;
    OpenFileName.lpstrTitle = (Title) ? Title : TEXT("Save File");

    if (GetSaveFileName(&OpenFileName) == TRUE)
    {
        strncpy(result.FilePath, FileName, sizeof(result.FilePath) - 1);
        result.Success = true;
    }
    else
    {
        DWORD err = CommDlgExtendedError();
        result.Error = (err != 0) ? AX_WINDOW_ERROR_OPERATION_FAILED : AX_WINDOW_ERROR_NONE;
    }
    return result;
}

static AxFileDialogResult OpenFolderDialog(const AxWindow *Window, const char *Message, const char *InitialDirectory)
{
    AxFileDialogResult result = {0};
    result.Success = false;
    result.Error = AX_WINDOW_ERROR_NONE;
    result.FilePath[0] = '\0';

    BROWSEINFO bi = {0};
    char FolderName[1024] = {0};
    bi.hwndOwner = (HWND)Window->Platform.Win32.Handle;
    bi.lpszTitle = Message;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpfn = NULL;
    bi.lParam = 0;

    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
    if (pidl && SHGetPathFromIDList(pidl, FolderName))
    {
        strncpy(result.FilePath, FolderName, sizeof(result.FilePath) - 1);
        result.Success = true;
    }
    else
    {
        result.Error = AX_WINDOW_ERROR_OPERATION_FAILED;
    }
    return result;
}

static AxMessageBoxResult CreateMessageBox(const AxWindow *Window, const char *Title, const char *Message, enum AxMessageBoxFlags Type)
{
    AxMessageBoxResult result = {0};
    result.Success = false;
    result.Error = AX_WINDOW_ERROR_NONE;
    result.Response = AX_MESSAGEBOX_RESPONSE_CANCEL;

    uint32_t Flags = 0;
    if (Type & AX_MESSAGEBOX_TYPE_ABORTRETRYIGNORE) { Flags |= MB_ABORTRETRYIGNORE; }
    if (Type & AX_MESSAGEBOX_TYPE_CANCELTRYCONTINUE) { Flags |= MB_CANCELTRYCONTINUE; }
    if (Type & AX_MESSAGEBOX_TYPE_HELP) { Flags |= MB_HELP; }
    if (Type & AX_MESSAGEBOX_TYPE_OK) { Flags |= MB_OK; }
    if (Type & AX_MESSAGEBOX_TYPE_OKCANCEL) { Flags |= MB_OKCANCEL; }
    if (Type & AX_MESSAGEBOX_TYPE_RETRYCANCEL) { Flags |= MB_RETRYCANCEL; }
    if (Type & AX_MESSAGEBOX_TYPE_YESNO) { Flags |= MB_YESNO; }
    if (Type & AX_MESSAGEBOX_TYPE_YESNOCANCEL) { Flags |= MB_YESNOCANCEL; }
    if (Type & AX_MESSAGEBOX_ICON_EXCLAMATION) { Flags |= MB_ICONEXCLAMATION; }
    if (Type & AX_MESSAGEBOX_ICON_WARNING) { Flags |= MB_ICONWARNING; }
    if (Type & AX_MESSAGEBOX_ICON_INFORMATION) { Flags |= MB_ICONINFORMATION; }
    if (Type & AX_MESSAGEBOX_ICON_QUESTION) { Flags |= MB_ICONQUESTION; }
    if (Type & AX_MESSAGEBOX_ICON_STOP) { Flags |= MB_ICONSTOP; }
    if (Type & AX_MESSAGEBOX_ICON_ERROR) { Flags |= MB_ICONERROR; }
    if (Type & AX_MESSAGEBOX_DEFBUTTON1) { Flags |= MB_DEFBUTTON1; }
    if (Type & AX_MESSAGEBOX_DEFBUTTON2) { Flags |= MB_DEFBUTTON2; }
    if (Type & AX_MESSAGEBOX_DEFBUTTON3) { Flags |= MB_DEFBUTTON3; }
    if (Type & AX_MESSAGEBOX_DEFBUTTON4) { Flags |= MB_DEFBUTTON4; }
    if (Type & AX_MESSAGEBOX_APPLMODAL) { Flags |= MB_APPLMODAL; }
    if (Type & AX_MESSAGEBOX_SYSTEMMODAL) { Flags |= MB_SYSTEMMODAL; }
    if (Type & AX_MESSAGEBOX_TASKMODAL) { Flags |= MB_TASKMODAL; }

    int response = MessageBox((HWND)Window->Platform.Win32.Handle, Message, Title, Flags);
    if (response != 0)
    {
        result.Response = (enum AxMessageBoxResponse)response;
        result.Success = true;
    }
    else
    {
        result.Error = AX_WINDOW_ERROR_OPERATION_FAILED;
    }
    return result;
}

static const char* GetErrorString(enum AxWindowError Error)
{
    switch (Error) {
        case AX_WINDOW_ERROR_NONE:
            return "No error";
        case AX_WINDOW_ERROR_WINDOW_REGISTRATION_FAILED:
            return "Window registration failed";
        case AX_WINDOW_ERROR_WINDOW_CREATION_FAILED:
            return "Window creation failed";
        case AX_WINDOW_ERROR_RAW_INPUT_REGISTRATION_FAILED:
            return "Raw input registration failed";
        case AX_WINDOW_ERROR_INVALID_PARAMETERS:
            return "Invalid parameters";
        case AX_WINDOW_ERROR_INVALID_WINDOW:
            return "Invalid window handle";
        case AX_WINDOW_ERROR_OPERATION_FAILED:
            return "Operation failed";
        case AX_WINDOW_ERROR_NOT_SUPPORTED:
            return "Operation not supported";
        case AX_WINDOW_ERROR_ALREADY_EXISTS:
            return "Resource already exists";
        case AX_WINDOW_ERROR_OUT_OF_MEMORY:
            return "Out of memory";
        case AX_WINDOW_ERROR_INVALID_STATE:
            return "Invalid state";
        case AX_WINDOW_ERROR_UNKNOWN:
        default:
            return "Unknown error";
    }
}

static bool SetCallbacks(AxWindow *Window, const AxWindowCallbacks *Callbacks, enum AxWindowError *Error)
{
    AXON_ASSERT(Window);

    SET_ERROR(Error, AX_WINDOW_ERROR_NONE);

    if (!Window) {
        SET_ERROR(Error, AX_WINDOW_ERROR_INVALID_WINDOW);
        return false;
    }

    if (Callbacks) {
        // Set all callbacks from the structure
        Window->Callbacks.Key = Callbacks->Key;
        Window->Callbacks.MousePos = Callbacks->MousePos;
        Window->Callbacks.MouseButton = Callbacks->MouseButton;
        Window->Callbacks.Scroll = Callbacks->Scroll;
        Window->Callbacks.Char = Callbacks->Char;
        Window->Callbacks.StateChanged = Callbacks->StateChanged;
    } else {
        // Clear all callbacks
        Window->Callbacks.Key = NULL;
        Window->Callbacks.MousePos = NULL;
        Window->Callbacks.MouseButton = NULL;
        Window->Callbacks.Scroll = NULL;
        Window->Callbacks.Char = NULL;
        Window->Callbacks.StateChanged = NULL;
    }

    return true;
}

static bool GetCallbacks(const AxWindow *Window, AxWindowCallbacks *Callbacks, enum AxWindowError *Error)
{
    AXON_ASSERT(Window);
    AXON_ASSERT(Callbacks);

    SET_ERROR(Error, AX_WINDOW_ERROR_NONE);

    if (!Window) {
        SET_ERROR(Error, AX_WINDOW_ERROR_INVALID_WINDOW);
        return false;
    }

    if (!Callbacks) {
        SET_ERROR(Error, AX_WINDOW_ERROR_INVALID_PARAMETERS);
        return false;
    }

    // Copy all callbacks to the structure
    Callbacks->Key = Window->Callbacks.Key;
    Callbacks->MousePos = Window->Callbacks.MousePos;
    Callbacks->MouseButton = Window->Callbacks.MouseButton;
    Callbacks->Scroll = Window->Callbacks.Scroll;
    Callbacks->Char = Window->Callbacks.Char;
    Callbacks->StateChanged = Window->Callbacks.StateChanged;

    return true;
}

static AxKeyCallback SetKeyCallback(AxWindow *Window, AxKeyCallback Callback)
{
    AXON_ASSERT(Window);

    // _GLFW_SWAP(GLFWkeyfun, window->callbacks.key, cbfun);
    // return cbfun;

    AxKeyCallback type;
    type = Window->Callbacks.Key;
    Window->Callbacks.Key = Callback;
    Callback = type;

    return (Callback);
}

static AxMousePosCallback SetMousePosCallback(AxWindow *Window, AxMousePosCallback Callback)
{
    AXON_ASSERT(Window);

    AxMousePosCallback type;
    type = Window->Callbacks.MousePos;
    Window->Callbacks.MousePos = Callback;
    Callback = type;

    return (Callback);
}

static AxMouseButtonCallback SetMouseButtonCallback(AxWindow *Window, AxMouseButtonCallback Callback)
{
    AXON_ASSERT(Window);

    AxMouseButtonCallback type;
    type = Window->Callbacks.MouseButton;
    Window->Callbacks.MouseButton = Callback;
    Callback = type;

    return (Callback);
}

static AxMouseScrollCallback SetMouseScrollCallback(AxWindow *Window, AxMouseScrollCallback Callback)
{
    AXON_ASSERT(Window);

    AxMouseScrollCallback type;
    type = Window->Callbacks.Scroll;
    Window->Callbacks.Scroll = Callback;
    Callback = type;

    return (Callback);
}

static AxCharCallback SetCharCallback(AxWindow *Window, AxCharCallback Callback)
{
    AXON_ASSERT(Window);

    AxCharCallback type;
    type = Window->Callbacks.Char;
    Window->Callbacks.Char = Callback;
    Callback = type;

    return (Callback);
}

static AxWindowStateCallback SetWindowStateCallback(AxWindow *Window, AxWindowStateCallback Callback)
{
    AXON_ASSERT(Window);

    AxWindowStateCallback type;
    type = Window->Callbacks.StateChanged;
    Window->Callbacks.StateChanged = Callback;
    Callback = type;

    return (Callback);
}

static bool GetWindowInputState(const AxWindow *Window, AxInputState *State, enum AxWindowError *Error)
{
    AXON_ASSERT(Window);
    AXON_ASSERT(State);

    SET_ERROR(Error, AX_WINDOW_ERROR_NONE);

    if (!Window) {
        SET_ERROR(Error, AX_WINDOW_ERROR_INVALID_WINDOW);
        return false;
    }

    if (!State) {
        SET_ERROR(Error, AX_WINDOW_ERROR_INVALID_PARAMETERS);
        return false;
    }

    // Copy key states
    for (int i = 0; i <= AX_KEY_LAST; i++) {
        State->Keys[i] = (Window->Keys[i] == AX_PRESS);
    }

    // Copy mouse button states
    for (int i = 0; i <= AX_MOUSE_BUTTON_LAST; i++) {
        State->MouseButtons[i] = (Window->MouseButtons[i] == AX_PRESS);
    }

    // Copy mouse position and delta
    State->MousePosition = Window->VirtualCursorPos;
    State->MouseDelta.X = Window->VirtualCursorPos.X - Window->LastMousePos.X;
    State->MouseDelta.Y = Window->VirtualCursorPos.Y - Window->LastMousePos.Y;

    // Get current modifiers
    State->Modifiers = GetKeyModifiers();
    State->WindowFocused = Window->HasFocus;

    return true;
}

static bool IsKeyPressed(const AxWindow *Window, int Key)
{
    AXON_ASSERT(Window);

    if (!Window || Key < 0 || Key > AX_KEY_LAST) {
        return false;
    }

    return (Window->Keys[Key] == AX_PRESS);
}

static bool IsMouseButtonPressed(const AxWindow *Window, int Button)
{
    AXON_ASSERT(Window);

    if (!Window || Button < 0 || Button > AX_MOUSE_BUTTON_LAST) {
        return false;
    }

    return (Window->MouseButtons[Button] == AX_PRESS);
}

static int GetModifiers(const AxWindow *Window)
{
    AXON_ASSERT(Window);

    if (!Window) {
        return 0;
    }

    return GetKeyModifiers();
}

static bool HasFocus(const AxWindow *Window)
{
    AXON_ASSERT(Window);

    if (!Window) {
        return false;
    }

    return Window->HasFocus;
}

static bool SetInputEventFiltering(AxWindow *Window, bool FilterEvents, enum AxWindowError *Error)
{
    AXON_ASSERT(Window);

    SET_ERROR(Error, AX_WINDOW_ERROR_NONE);

    if (!Window) {
        SET_ERROR(Error, AX_WINDOW_ERROR_INVALID_WINDOW);
        return false;
    }

    Window->FilterEventsWhenUnfocused = FilterEvents;
    return true;
}

static bool GetInputEventFiltering(const AxWindow *Window)
{
    AXON_ASSERT(Window);

    if (!Window) {
        return false;
    }

    return Window->FilterEventsWhenUnfocused;
}

// Global platform hints storage
static AxPlatformHints GlobalPlatformHints = {0};

static bool GetPlatformInfo(AxPlatformInfo *Info, enum AxWindowError *Error)
{
    AXON_ASSERT(Info);

    SET_ERROR(Error, AX_WINDOW_ERROR_NONE);

    if (!Info) {
        SET_ERROR(Error, AX_WINDOW_ERROR_INVALID_PARAMETERS);
        return false;
    }

    // Fill platform information
    Info->Type = AX_PLATFORM_WINDOWS;
    Info->Features = 0;
    Info->IsInitialized = true;

    // Get Windows version information
    OSVERSIONINFOEX osvi = {0};
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    
    if (GetVersionEx((OSVERSIONINFO*)&osvi)) {
        Info->MajorVersion = osvi.dwMajorVersion;
        Info->MinorVersion = osvi.dwMinorVersion;
        Info->BuildNumber = osvi.dwBuildNumber;
        
        // Set platform name and version
        snprintf(Info->Name, sizeof(Info->Name), "Windows");
        snprintf(Info->Version, sizeof(Info->Version), "%d.%d.%d", 
                Info->MajorVersion, Info->MinorVersion, Info->BuildNumber);
    } else {
        Info->MajorVersion = 0;
        Info->MinorVersion = 0;
        Info->BuildNumber = 0;
        snprintf(Info->Name, sizeof(Info->Name), "Windows");
        snprintf(Info->Version, sizeof(Info->Version), "Unknown");
    }

    // Detect available features
    if (Info->MajorVersion >= 10) {
        Info->Features |= AX_PLATFORM_FEATURE_DPI_AWARENESS;
        Info->Features |= AX_PLATFORM_FEATURE_HIGH_DPI;
        Info->Features |= AX_PLATFORM_FEATURE_MULTI_MONITOR;
        Info->Features |= AX_PLATFORM_FEATURE_RAW_INPUT;
        Info->Features |= AX_PLATFORM_FEATURE_DIRECTX;
        Info->Features |= AX_PLATFORM_FEATURE_CLIPBOARD;
        Info->Features |= AX_PLATFORM_FEATURE_DRAG_DROP;
    } else if (Info->MajorVersion >= 6) {
        Info->Features |= AX_PLATFORM_FEATURE_DPI_AWARENESS;
        Info->Features |= AX_PLATFORM_FEATURE_MULTI_MONITOR;
        Info->Features |= AX_PLATFORM_FEATURE_RAW_INPUT;
        Info->Features |= AX_PLATFORM_FEATURE_DIRECTX;
        Info->Features |= AX_PLATFORM_FEATURE_CLIPBOARD;
        Info->Features |= AX_PLATFORM_FEATURE_DRAG_DROP;
    }

    // Check for OpenGL support (simplified check)
    Info->Features |= AX_PLATFORM_FEATURE_OPENGL;

    return true;
}

static bool HasPlatformFeature(enum AxPlatformFeatures Feature, enum AxWindowError *Error)
{
    SET_ERROR(Error, AX_WINDOW_ERROR_NONE);

    AxPlatformInfo Info = {0};
    if (!GetPlatformInfo(&Info, Error)) {
        return false;
    }

    return (Info.Features & Feature) != 0;
}

static enum AxPlatformType GetPlatformType(void)
{
    return AX_PLATFORM_WINDOWS;
}

static bool SetPlatformHints(const AxPlatformHints *Hints, enum AxWindowError *Error)
{
    SET_ERROR(Error, AX_WINDOW_ERROR_NONE);

    if (!Hints) {
        SET_ERROR(Error, AX_WINDOW_ERROR_INVALID_PARAMETERS);
        return false;
    }

    // Copy hints to global storage
    GlobalPlatformHints = *Hints;
    return true;
}

static bool GetPlatformHints(AxPlatformHints *Hints, enum AxWindowError *Error)
{
    SET_ERROR(Error, AX_WINDOW_ERROR_NONE);

    if (!Hints) {
        SET_ERROR(Error, AX_WINDOW_ERROR_INVALID_PARAMETERS);
        return false;
    }

    // Copy current hints
    *Hints = GlobalPlatformHints;
    return true;
}

static bool ValidatePlatform(enum AxWindowError *Error)
{
    SET_ERROR(Error, AX_WINDOW_ERROR_NONE);

    // Check if Windows API is available
    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    if (!kernel32) {
        SET_ERROR(Error, AX_WINDOW_ERROR_NOT_SUPPORTED);
        return false;
    }

    // Check if user32 is available
    HMODULE user32 = GetModuleHandleA("user32.dll");
    if (!user32) {
        SET_ERROR(Error, AX_WINDOW_ERROR_NOT_SUPPORTED);
        return false;
    }

    // Check if we can create a window class (basic functionality test)
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "AxonTestClass";
    
    ATOM result = RegisterClassEx(&wc);
    if (result == 0) {
        SET_ERROR(Error, AX_WINDOW_ERROR_WINDOW_REGISTRATION_FAILED);
        return false;
    }
    
    // Clean up test class
    UnregisterClass("AxonTestClass", GetModuleHandle(NULL));
    
    return true;
}
// Use a compound literal to construct an unnamed object of API type in-place
struct AxWindowAPI *WindowAPI = &(struct AxWindowAPI) {
    .Init = Init,
    .CreateWindow = CreateWindow_,
    .CreateWindowWithConfig = CreateWindowWithConfig,
    .CreateWindowDefault = CreateWindowDefault,
    .DestroyWindow = DestroyWindow_,
    .PollEvents = PollEvents,
    .HasRequestedClose = HasRequestedClose,
    .GetWindowPosition = GetWindowPosition,
    .SetWindowPosition = SetWindowPosition,
    .GetWindowSize = GetWindowSize,
    .SetWindowSize = SetWindowSize,
    .SetWindowVisible = SetWindowVisible,
    .GetWindowStateInfo = GetWindowStateInfo,
    .SetWindowState = SetWindowState,
    .GetWindowState = GetWindowState,
    .SetWindowStateEnum = SetWindowStateEnum,
    .GetPlatformData = GetPlatformData,
    .GetMouseCoords = GetMouseCoords,
    .GetMouseButton = GetMouseButton,
    .SetCursorMode = SetCursorMode,
    .GetCursorMode = GetCursorMode,
    .SetKeyboardMode = SetKeyboardMode,
    .GetKey = GetKey,
    .OpenFileDialog = OpenFileDialog,
    .SaveFileDialog = SaveFileDialog,
    .OpenFolderDialog = OpenFolderDialog,
    .CreateMessageBox = CreateMessageBox,
    .GetErrorString = GetErrorString,
    .SetKeyCallback = SetKeyCallback,
    .SetMousePosCallback = SetMousePosCallback,
    .SetMouseButtonCallback = SetMouseButtonCallback,
    .SetMouseScrollCallback = SetMouseScrollCallback,
    .SetCharCallback = SetCharCallback,
    .SetWindowStateCallback = SetWindowStateCallback,
    .SetCallbacks = SetCallbacks,
    .GetCallbacks = GetCallbacks,
    .GetWindowInputState = GetWindowInputState,
    .IsKeyPressed = IsKeyPressed,
    .IsMouseButtonPressed = IsMouseButtonPressed,
    .GetModifiers = GetModifiers,
    .HasFocus = HasFocus,
    .SetInputEventFiltering = SetInputEventFiltering,
    .GetInputEventFiltering = GetInputEventFiltering,
    .GetPlatformInfo = GetPlatformInfo,
    .HasPlatformFeature = HasPlatformFeature,
    .GetPlatformType = GetPlatformType,
    .SetPlatformHints = SetPlatformHints,
    .GetPlatformHints = GetPlatformHints,
    .ValidatePlatform = ValidatePlatform
};

AXON_DLL_EXPORT void LoadPlugin(struct AxAPIRegistry *APIRegistry, bool Load)
{
    HMODULE ShcoreHandle = LoadLibrary("shcore");
    GetDpiForMonitor = (GetDpiForMonitorPtr)GetProcAddress(ShcoreHandle, "GetDpiForMonitor");

    if (APIRegistry)
    {
        PlatformAPI = APIRegistry->Get(AXON_PLATFORM_API_NAME);

        APIRegistry->Set(AXON_WINDOW_API_NAME, WindowAPI, sizeof(struct AxWindowAPI));
    }
}
