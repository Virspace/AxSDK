#include "Foundation/APIRegistry.h"
#include "Foundation/Math.h"
#include "Foundation/HashTable.h"
#include "Foundation/Platform.h"
#include "AxWindow.h"
#include <stdio.h>
#include <stdlib.h>

// NOTE(mdeforge): Uncomment to get some leak detection
//#define _CRTDBG_MAP_ALLOC
//#include <stdlib.h>
//#include <crtdbg.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// From hidusage.h
#define HID_USAGE_PAGE_GENERIC                          ((unsigned short) 0x01) // Generic Desktop Controls Usage Pages
#define HID_USAGE_GENERIC_MOUSE                         ((unsigned short) 0x02) // Generic Mouse
#define HID_USAGE_GENERIC_KEYBOARD                      ((unsigned short) 0x06) // Generic Keyboard

// NOTE(mdeforge): Special thanks to the Win32 API for making me use _'s for
//                 my CreateWindow, DestroyWindow, and UpdateWindow functions
//                 in order to avoid collision with their #defined names...
#include <windows.h>
#include <VersionHelpers.h>
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

struct AxDisplayMode
{
    int32_t Width;
    int32_t Height;
    int32_t RefreshRateHz;
    AxVec3 BitDepth;
};

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
};

static AxRect RectToAxRect(RECT Rect)
{
    AxRect Result = {
        .Left = Rect.left,
        .Top = Rect.top,
        .Right = Rect.right,
        .Bottom = Rect.bottom
    };

    return (Result);
}

static RECT AxRectToRect(AxRect Rect)
{
    RECT Result = {
        .left = Rect.Left,
        .top = Rect.Top,
        .right = Rect.Right,
        .bottom = Rect.Bottom
    };

    return (Result);
}

struct AxPlatformAPI *PlatformAPI;

static enum AxKeyModifier GetKeyModifiers(void)
{
    enum AxKeyModifier Mods;

    if (GetKeyState(VK_CONTROL) & 0x8000) {
        Mods |= AX_KEY_CTRL;
    }

    if (GetKeyState(VK_MENU) & 0x8000) {
        Mods |= AX_KEY_ALT;
    }

    if (GetKeyState(VK_SHIFT) & 0x8000) {
        Mods |= AX_KEY_SHIFT;
    }

    if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) {
        Mods |= AX_KEY_WIN;
    }

    if (GetKeyState(VK_CAPITAL) & 0x8000) {
        Mods |= AX_KEY_CAPS;
    }

    return (Mods);
}

static DWORD GetWindowStyle(const AxWindow *Window)
{
    Assert(Window != NULL);

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
    Assert(Window != NULL);

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
            const enum AxKeyState State = (HIWORD(LParam) & KF_UP) ? AX_KEY_RELEASED : AX_KEY_PRESSED;
            const enum AxKeyModifier Mods = GetKeyModifiers();

            ScanCode = (HIWORD(LParam) & (KF_EXTENDED | 0xff));
            if (!ScanCode) {
                ScanCode = MapVirtualKey((UINT)WParam, MAPVK_VK_TO_VSC);
            }


        } break;

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
                Window->VirtualCursorPos = (AxVec2){ (float)X, (float)Y };
            }

            Window->Platform.Win32.LastCursorPos = (AxVec2){ (float)X, (float)Y };

            return (0);
        }

        case WM_INPUT:
        {
            // If the cursor mode is normal or hidden, and the keyboard is disabled, break. This section is for raw input.
            if (!Window->CursorMode == AX_CURSOR_DISABLED && Window->KeyboardMode == AX_KEYBOARD_DISABLED) {
                break;
            }

            UINT RawInputSize;
            BYTE RawInputData[sizeof(RAWINPUT)] = {0};
            if (!GetRawInputData((HRAWINPUT)LParam, RID_INPUT, RawInputData, &RawInputSize, sizeof(RAWINPUTHEADER))) {
                break;
            }

            AxVec2 MouseDelta;
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
                    MouseDelta = (AxVec2)
                    {
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
                        VirtualKey = (IsEscSeq0) ? AX_KEY_RIGHT_CTRL : AX_KEY_LEFT_CTRL;
                    } break;

                    case VK_MENU:
                    {
                        VirtualKey = (IsEscSeq0) ? AX_KEY_RIGHT_ALT : AX_KEY_LEFT_ALT;
                    } break;

                    // NUMPAD ENTER has its E0 bit set
                    case VK_RETURN:
                    {
                        if (IsEscSeq0) {
                            VirtualKey = AX_KEY_NUMPAD_ENTER;
                        }
                    } break;

                    // The standard INSERT, DELETE, HOME, END, PRIOR, and NEXT keys will always have the E0 bit set
                    // but the same keys on the numpad will not.
                    case VK_INSERT:
                    {
                        if (!IsEscSeq0) {
                            VirtualKey = AX_KEY_NUMPAD_0;
                        }
                    } break;

                    case VK_DELETE:
                    {
                        if (!IsEscSeq0) {
                            VirtualKey = AX_KEY_NUMPAD_DECIMAL;
                        }
                    } break;

                    case VK_HOME:
                    {
                        if (!IsEscSeq0) {
                            VirtualKey = AX_KEY_NUMPAD_7;
                        }
                    } break;

                    case VK_END:
                    {
                        if (!IsEscSeq0) {
                            VirtualKey = AX_KEY_NUMPAD_1;
                        }
                    } break;

                    case VK_PRIOR:
                    {
                        if (!IsEscSeq0) {
                            VirtualKey = AX_KEY_NUMPAD_9;
                        }
                    } break;

                    case VK_NEXT:
                    {
                        if (!IsEscSeq0) {
                            VirtualKey = AX_KEY_NUMPAD_3;
                        }
                    } break;

                    // The standard arrow keys will always have the E0 bit set, but the same keys
                    // on the numpad will not.
                    case VK_UP:
                    {
                        if (!IsEscSeq0) {
                            VirtualKey = AX_KEY_NUMPAD_8;
                        }
                    } break;

                    case VK_DOWN:
                    {
                        if (!IsEscSeq0) {
                            VirtualKey = AX_KEY_NUMPAD_2;
                        }
                    } break;

                    case VK_LEFT:
                    {
                        if (!IsEscSeq0) {
                            VirtualKey = AX_KEY_NUMPAD_4;
                        }
                    } break;

                    case VK_RIGHT:
                    {
                        if (!IsEscSeq0) {
                            VirtualKey = AX_KEY_NUMPAD_6;
                        }
                    } break;
                }
            }

            Window->VirtualCursorPos = Vec2Add(Window->VirtualCursorPos, MouseDelta);
            Window->Platform.Win32.LastCursorPos = Vec2Add(Window->Platform.Win32.LastCursorPos, MouseDelta);

            break;
        }

        case WM_MOUSELEAVE:
        {
            Window->Platform.Win32.CursorInWindow = false;

            return (0);
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
    Assert(Window != NULL);

    UINT XDPI, YDPI;
    HANDLE MonitorHandle = MonitorFromWindow((HWND)Window->Platform.Win32.Handle, MONITOR_DEFAULTTONEAREST);
    LRESULT Result = GetDpiForMonitor(MonitorHandle, 0, &XDPI, &YDPI); // NOTE(mdeforge): Requires Windows 8.1 or higher

    return ((Result == S_OK) ? XDPI : USER_DEFAULT_SCREEN_DPI);
}

static bool CreateNativeWindow(AxWindow *Window)
{
    Assert(Window != NULL);

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

static AxWindow *CreateWindow_(const char *Title, int32_t X, int32_t Y, int32_t Width, int32_t Height, AxDisplay *Display, enum AxWindowStyle Style)
{
    Assert(Title != NULL);

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
    Assert(Window != NULL);

    MSG Message = {0};

    for (;;)
    {
        bool GotMessage = false;
        DWORD SkipMessages[] = { 0x738, 0xFFFFFFFF };

        DWORD LastMessage = 0;
        for (uint32_t SkipIndex = 0; SkipIndex < ArrayCount(SkipMessages); ++SkipIndex)
        {
            DWORD Skip = SkipMessages[SkipIndex];
            GotMessage = PeekMessage(&Message, 0, LastMessage, Skip - 1, PM_REMOVE);
            if (GotMessage) {
                break;
            }

            LastMessage = Skip + 1;
        }

        if (!GotMessage) {
            break;
        }

        switch(Message.message)
        {
            case WM_QUIT:
            {
                Window->IsRequestingClose = true;
            } break;
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                // TODO(mdeforge): Consider input
            } break;
            default:
            {
                TranslateMessage(&Message);
                DispatchMessage(&Message);
            } break;
        }
    }
}

static bool HasRequestedClose(AxWindow *Window)
{
    Assert(Window != NULL);

    if (Window->IsRequestingClose) {
        return (true);
    }

    return (false);
}

static AxRect WindowRect(AxWindow *Window)
{
    Assert(Window != NULL);

    RECT Rect;
    GetClientRect((HWND)Window->Platform.Win32.Handle, &Rect);

    return (RectToAxRect(Rect));
}

static void SetWindowPosition(AxWindow *Window, int32_t X, int32_t Y)
{
    Assert(Window != NULL);

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

static void SetWindowSize(AxWindow *Window, int32_t Width, int32_t Height)
{
    Assert(Window != NULL);

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
    Assert(Window != NULL);

    if (IsVisible) {
        ShowWindow((HWND)Window->Platform.Win32.Handle, SW_SHOW);
    } else {
        ShowWindow((HWND)Window->Platform.Win32.Handle, SW_HIDE);
    }
}

static AxWindowPlatformData PlatformData(const AxWindow *Window)
{
    Assert(Window != NULL);

    AxWindowPlatformData Data = {0};
    if (Window) {
        Data = Window->Platform;
    }

    return (Data);
}

static void GetMouseCoords(const AxWindow *Window, AxVec2 *Position)
{
    Assert(Window != NULL);

    if (Position) {
        *Position = (AxVec2){0};
    }

    // if (Window->CursorMode == AX_CURSOR_DISABLED)
    // {
    //     if (Position) {
            *Position = Window->VirtualCursorPos;
    //     }
    // }
}

// TODO(mdeforge): Update cursor image using enable/disable cursor functions
static void SetCursorMode(AxWindow *Window, enum AxCursorMode CursorMode)
{
    Assert(Window != NULL);

    // TODO(mdeforge): Validate CursorMode?
    Window->CursorMode = CursorMode;
    UpdateCursorImage(Window);
}

static void SetKeyboardMode(AxWindow *Window, enum AxKeyboardMode KeyboardMode)
{
    Assert(Window != NULL);

    // TODO(mdeforge): Validate KeyboardMode?
    Window->KeyboardMode = KeyboardMode;
}

struct AxWindowAPI *WindowAPI = &(struct AxWindowAPI) {
    .Init = Init,
    .CreateWindow = CreateWindow_,
    .DestroyWindow = DestroyWindow_,
    .PollEvents = PollEvents,
    .HasRequestedClose = HasRequestedClose,
    .WindowRect = WindowRect,
    .SetWindowPosition = SetWindowPosition,
    .SetWindowSize = SetWindowSize,
    .SetWindowVisible = SetWindowVisible,
    .PlatformData = PlatformData,
    .GetMouseCoords = GetMouseCoords,
    .SetCursorMode = SetCursorMode,
    .SetKeyboardMode = SetKeyboardMode,
    // .EnableCursor = EnableCursor,
    // .DisableCursor = DisableCursor
};

AXON_DLL_EXPORT void LoadPlugin(struct AxAPIRegistry *APIRegistry, bool Load)
{
    HMODULE ShcoreHandle = LoadLibrary("shcore");
    GetDpiForMonitor = GetProcAddress(ShcoreHandle, "GetDpiForMonitor");

    if (APIRegistry)
    {
        PlatformAPI = APIRegistry->Get(AXON_PLATFORM_API_NAME);

        APIRegistry->Set(AXON_WINDOW_API_NAME, WindowAPI, sizeof(struct AxWindowAPI));
    }
}