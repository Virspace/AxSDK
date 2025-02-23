#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxMath.h"
#include "Foundation/AxHashTable.h"
#include "Foundation/AxPlatform.h"
#include "AxWindow.h"
#include <stdio.h>
#include <stdlib.h>
#include <float.h>

// NOTE(mdeforge): Uncomment to get some leak detection
//#define _CRTDBG_MAP_ALLOC
//#include <stdlib.h>
//#include <crtdbg.h>

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

    struct {
        AxKeyCallback Key;
        AxMousePosCallback MousePos;
        AxMouseButtonCallback MouseButton;
        AxMouseScrollCallback Scroll;
        AxCharCallback Char;
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

    Window->VirtualCursorPos = Pos;

    if (Window->Callbacks.MousePos) {
        Window->Callbacks.MousePos(Window, Pos.X, Pos.Y);
    }
}

static int GetKeyModifiers(void)
{
    int Mods = 0;

    if (GetKeyState(VK_CONTROL) & 0x8000) {
        Mods |= AX_MOD_CONTROL;
    }

    if (GetKeyState(VK_MENU) & 0x8000) {
        Mods |= AX_MOD_ALT;
    }

    if (GetKeyState(VK_SHIFT) & 0x8000) {
        Mods |= AX_MOD_SHIFT;
    }

    if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) {
        Mods |= AX_MOD_SUPER;
    }

    if (GetKeyState(VK_CAPITAL) & 1) {
        Mods |= AX_MOD_CAPS_LOCK;
    }

    if (GetKeyState(VK_NUMLOCK) & 1) {
        Mods |= AX_MOD_NUM_LOCK;
    }

    return (Mods);
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

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
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
            // We += here because it's possible to accrue a few messages before the value actually gets used
            AxVec2 Offset = { 0.0f, (float)GET_WHEEL_DELTA_WPARAM(WParam) / (float)WHEEL_DELTA };
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

        case WM_CHAR:
        case WM_SYSCHAR:
        {
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

static void Init(void)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    Win32RegisterWindowClass();
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

static AxWindow *CreateWindow_(const char *Title, int32_t X, int32_t Y, int32_t Width, int32_t Height, enum AxWindowStyle Style)
{
    AXON_ASSERT(Title != NULL);

    if (Width <= 0 || Height <= 0) {
        return (false);
    }

    RECT Rect = {
        .left = X,
        .top = Y,
        .right = X + Width,
        .bottom = Y + Height
    };

    AxWindow *Window = calloc(1, sizeof(AxWindow));
    if (Window)
    {
        Window->Platform.Win32.Instance = (uint64_t)GetModuleHandle(0);
        Window->IsRequestingClose = false;
        Window->Title = Title;
        Window->Rect = Rect;
        Window->Style = Style;

        if (!CreateNativeWindow(Window))
        {
            DestroyWindow_(Window);

            return (NULL);
        }

        if (Style & AX_WINDOW_STYLE_FULLSCREEN ||
            Style & AX_WINDOW_STYLE_VISIBLE)
        {
            ShowWindow((HWND)Window->Platform.Win32.Handle, SW_SHOW);
            SetFocus((HWND)Window->Platform.Win32.Handle);
            UpdateWindow((HWND)Window->Platform.Win32.Handle);
        }
    }

    return(Window);
}

static void PollEvents(AxWindow *Window)
{
    AXON_ASSERT(Window);

    MSG Msg = {0};

    for (;;)
    {
        bool GotMessage = false;
        DWORD SkipMessages[] = { 0x738, 0xFFFFFFFF };

        DWORD LastMessage = 0;
        for (uint32_t SkipIndex = 0; SkipIndex < ArrayCount(SkipMessages); ++SkipIndex)
        {
            DWORD Skip = SkipMessages[SkipIndex];
            GotMessage = PeekMessage(&Msg, 0, LastMessage, Skip - 1, PM_REMOVE);
            if (GotMessage) {
                break;
            }

            LastMessage = Skip + 1;
        }

        if (!GotMessage) {
            break;
        }

        if(Msg.message == WM_QUIT)
        {
            Window->IsRequestingClose = true;
        } else {
            TranslateMessage(&Msg);
            DispatchMessage(&Msg);
        }
    }

    const int Keys[4][2] =
    {
        { VK_LSHIFT, AX_KEY_LEFT_SHIFT },
        { VK_RSHIFT, AX_KEY_RIGHT_SHIFT },
        { VK_LWIN, AX_KEY_LEFT_SUPER },
        { VK_RWIN, AX_KEY_RIGHT_SUPER }
    };

    for (int32_t i = 0; i < 4;  i++)
    {
        const int VK = Keys[i][0];
        const int Key = Keys[i][1];
        const int ScanCode = Window->Platform.Win32.ScanCodes[Key];

        if ((GetKeyState(VK) & 0x8000))
            continue;
        if (Window->Keys[Key] != AX_PRESS)
            continue;

        InputKey(Window, Key, ScanCode, AX_RELEASE, GetKeyModifiers());
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

static void SetWindowPosition(AxWindow *Window, int32_t X, int32_t Y)
{
    AXON_ASSERT(Window);

    if (Window->Style & AX_WINDOW_STYLE_FULLSCREEN) {
        return;
    }

    RECT Rect = { X, Y, 0, 0 };
    UINT DPI = GetNearestMonitorDPI(Window);

    // Adjust size to account for non-client area
    DWORD Style = GetWindowStyle(Window);
    AdjustWindowRectExForDpi((LPRECT)&Rect, Style, FALSE, 0, DPI);
    SetWindowPos((HWND)Window->Platform.Win32.Handle, NULL,
        Rect.left, Rect.top, 0, 0,
        SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE);

    Window->Rect = Rect;
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

static void SetWindowSize(AxWindow *Window, int32_t Width, int32_t Height)
{
    AXON_ASSERT(Window);

    if (Window->Style & AX_WINDOW_STYLE_FULLSCREEN) {
        return;
    }

    RECT Rect = { 0, 0, Width, Height };
    UINT DPI = GetNearestMonitorDPI(Window);
    DWORD Style = GetWindowStyle(Window);

    AdjustWindowRectExForDpi((LPRECT)&Rect, Style, FALSE, 0, DPI);
    SetWindowPos((HWND)Window->Platform.Win32.Handle, NULL,
        0, 0, Rect.right - Rect.left, Rect.bottom - Rect.top,
        SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOZORDER);

    Window->Rect = Rect;
}

static void SetWindowVisible(AxWindow *Window, bool IsVisible)
{
    AXON_ASSERT(Window);

    if (IsVisible) {
        ShowWindow((HWND)Window->Platform.Win32.Handle, SW_SHOW);
    } else {
        ShowWindow((HWND)Window->Platform.Win32.Handle, SW_HIDE);
    }
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
static void SetCursorMode(AxWindow *Window, enum AxCursorMode CursorMode)
{
    AXON_ASSERT(Window && "Window is NULL in SetCursorMode()!");

    // TODO(mdeforge): Validate CursorMode?
    Window->CursorMode = CursorMode;
    UpdateCursorImage(Window);
}

static enum AxCursorMode GetCursorMode(AxWindow *Window)
{
  AXON_ASSERT(Window);

  return (Window->CursorMode);
}

static void SetKeyboardMode(AxWindow *Window, enum AxKeyboardMode KeyboardMode)
{
    AXON_ASSERT(Window);

    // TODO(mdeforge): Validate KeyboardMode?
    Window->KeyboardMode = KeyboardMode;
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

static bool OpenFileDialog(const AxWindow *Window, const char *Title, const char *Filter, const char *InitialDirectory, char *FileName, uint32_t FileNameSize)
{
    OPENFILENAME OpenFileName;
    ZeroMemory(&OpenFileName, sizeof(OPENFILENAME));
    OpenFileName.lStructSize = sizeof(OPENFILENAME);
    OpenFileName.hwndOwner = (HWND)Window->Platform.Win32.Handle;
    OpenFileName.lpstrFile = FileName;
    OpenFileName.nMaxFile = (DWORD)FileNameSize;
    OpenFileName.lpstrFilter = (Filter) ? Filter : TEXT("All files\0*.*\0\0");
    OpenFileName.nFilterIndex = 1;
    OpenFileName.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    OpenFileName.lpstrInitialDir = InitialDirectory;
    OpenFileName.lpstrTitle = (Title) ? Title : TEXT("Open File");

    if (GetOpenFileName(&OpenFileName) == TRUE)
    {
        FileName = OpenFileName.lpstrFile;
        return (true);
    }

    return (false);
}

static bool SaveFileDialog(const AxWindow *Window, const char *Title, const char *Filter, const char *InitialDirectory, char *FileName, uint32_t FileNameSize)
{
    OPENFILENAME OpenFileName;
    ZeroMemory(&OpenFileName, sizeof(OPENFILENAME));
    OpenFileName.lStructSize = sizeof(OPENFILENAME);
    OpenFileName.hwndOwner = (HWND)Window->Platform.Win32.Handle;
    OpenFileName.lpstrFile = FileName;
    OpenFileName.nMaxFile = (DWORD)FileNameSize;
    OpenFileName.lpstrFilter = (Filter) ? Filter : TEXT("All files\0*.*\0\0");
    OpenFileName.nFilterIndex = 1;
    OpenFileName.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    OpenFileName.lpstrInitialDir = InitialDirectory;
    OpenFileName.lpstrTitle = (Title) ? Title : TEXT("Save File");

    if (GetSaveFileName(&OpenFileName) == TRUE)
    {
        FileName = OpenFileName.lpstrFile;
        return (true);
    }

    return (false);
}

INT CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lp, LPARAM pData)
{
    if (uMsg == BFFM_INITIALIZED) {
        SendMessage(hwnd, BFFM_SETSELECTION, TRUE, pData);
    }

    return 0;
}

static bool OpenFolderDialog(const AxWindow *Window, const char *Message, const char *InitialDirectory, char *FolderName, uint32_t FolderNameSize)
{
    BROWSEINFO BrowseInfo;
    ZeroMemory(&BrowseInfo, sizeof(BROWSEINFO));
    BrowseInfo.hwndOwner = (HWND)Window->Platform.Win32.Handle;
    BrowseInfo.pszDisplayName = FolderName;
    BrowseInfo.pidlRoot = NULL;
    BrowseInfo.lpszTitle = (Message) ? Message : TEXT("Open Folder");
    BrowseInfo.ulFlags = BIF_NEWDIALOGSTYLE;
    BrowseInfo.lParam = (LPARAM)InitialDirectory;
    BrowseInfo.lpfn = BrowseCallbackProc;

    LPITEMIDLIST IDL = SHBrowseForFolder(&BrowseInfo);
    if (IDL != NULL)
    {
        SHGetPathFromIDList(IDL, FolderName);
        return (true);
    }

    return (false);
}

static enum AxMessageBoxResponse CreateMessageBox(const AxWindow *Window, const char *Message, const char *Title, enum AxMessageBoxFlags Type)
{
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

    enum AxMessageBoxResponse Result = (enum AxMessageBoxResponse)MessageBox((HWND)Window->Platform.Win32.Handle, Message, Title, Flags);
    return (Result);
}

// We forward declare these functions because these functions need the WindowAPI declared below
static AxKeyCallback SetKeyCallback(AxWindow *Window, AxKeyCallback Callback);
static AxMousePosCallback SetMousePosCallback(AxWindow *Window, AxMousePosCallback Callback);
static AxMouseButtonCallback SetMouseButtonCallback(AxWindow *Window, AxMouseButtonCallback Callback);
static AxMouseScrollCallback SetMouseScrollCallback(AxWindow *Window, AxMouseScrollCallback Callback);
static AxCharCallback SetCharCallback(AxWindow *Window, AxCharCallback Callback);

// Use a compound literal to construct an unnamed object of API type in-place
struct AxWindowAPI *WindowAPI = &(struct AxWindowAPI) {
    .Init = Init,
    .CreateWindow = CreateWindow_,
    .DestroyWindow = DestroyWindow_,
    .PollEvents = PollEvents,
    .HasRequestedClose = HasRequestedClose,
    .GetWindowPosition = GetWindowPosition,
    .SetWindowPosition = SetWindowPosition,
    .GetWindowSize = GetWindowSize,
    .SetWindowSize = SetWindowSize,
    .SetWindowVisible = SetWindowVisible,
    .GetPlatformData = GetPlatformData,
    .GetMouseCoords = GetMouseCoords,
    .GetMouseButton = GetMouseButton,
    .SetCursorMode = SetCursorMode,
    .GetCursorMode = GetCursorMode,
    .SetKeyboardMode = SetKeyboardMode,
    .GetKey = GetKey,
    .SetKeyCallback = SetKeyCallback,
    .SetMousePosCallback = SetMousePosCallback,
    .SetMouseButtonCallback = SetMouseButtonCallback,
    .SetMouseScrollCallback = SetMouseScrollCallback,
    .SetCharCallback = SetCharCallback,
    .OpenFileDialog = OpenFileDialog,
    .SaveFileDialog = SaveFileDialog,
    .OpenFolderDialog = OpenFolderDialog,
    .CreateMessageBox = CreateMessageBox
    // .EnableCursor = EnableCursor,
    // .DisableCursor = DisableCursor
};

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