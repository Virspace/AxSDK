#include "../include/AxWindow/AxWindow.h"
#include <cstdio>
#include <cstdlib>
#include <cstddef>

// Simple test to demonstrate platform abstraction features
// This would typically be integrated into a proper test framework

void test_platform_abstraction(void)
{
    printf("=== AxWindow Platform Abstraction Test ===\n\n");

    // Note: In a real application, you would get the API through the registry system
    // For this test, we'll demonstrate the structures and concepts

    // Test platform type enum
    printf("Platform Types:\n");
    printf("- AX_PLATFORM_WINDOWS: %d\n", AX_PLATFORM_WINDOWS);
    printf("- AX_PLATFORM_LINUX: %d\n", AX_PLATFORM_LINUX);
    printf("- AX_PLATFORM_MACOS: %d\n", AX_PLATFORM_MACOS);
    printf("- AX_PLATFORM_UNKNOWN: %d\n", AX_PLATFORM_UNKNOWN);

    // Test platform features
    printf("\nPlatform Features:\n");
    printf("- DPI Awareness: 0x%08X\n", AX_PLATFORM_FEATURE_DPI_AWARENESS);
    printf("- Raw Input: 0x%08X\n", AX_PLATFORM_FEATURE_RAW_INPUT);
    printf("- High DPI: 0x%08X\n", AX_PLATFORM_FEATURE_HIGH_DPI);
    printf("- Multi Monitor: 0x%08X\n", AX_PLATFORM_FEATURE_MULTI_MONITOR);
    printf("- OpenGL: 0x%08X\n", AX_PLATFORM_FEATURE_OPENGL);
    printf("- DirectX: 0x%08X\n", AX_PLATFORM_FEATURE_DIRECTX);
    printf("- Vulkan: 0x%08X\n", AX_PLATFORM_FEATURE_VULKAN);
    printf("- Metal: 0x%08X\n", AX_PLATFORM_FEATURE_METAL);
    printf("- Touch: 0x%08X\n", AX_PLATFORM_FEATURE_TOUCH);
    printf("- Gamepad: 0x%08X\n", AX_PLATFORM_FEATURE_GAMEPAD);
    printf("- Clipboard: 0x%08X\n", AX_PLATFORM_FEATURE_CLIPBOARD);
    printf("- Drag Drop: 0x%08X\n", AX_PLATFORM_FEATURE_DRAG_DROP);

    // Test platform info structure
    printf("\nPlatform Info Structure Size: %lu bytes\n", (unsigned long)sizeof(AxPlatformInfo));

    // Test platform hints structure
    printf("Platform Hints Structure Size: %lu bytes\n", (unsigned long)sizeof(AxPlatformHints));

    // Demonstrate platform hints usage
    printf("\nPlatform Hints Example:\n");
    AxPlatformHints hints = {};

    // Windows-specific hints
    hints.Windows.EnableCompositor = true;
    hints.Windows.EnableAcrylic = false;
    hints.Windows.EnableMica = true;
    hints.Windows.DisableWindowShadows = false;
    hints.Windows.UseImmersiveDarkMode = true;

    // Linux-specific hints
    hints.Linux.Display = ":0";
    hints.Linux.WindowManager = "i3";
    hints.Linux.UseWayland = false;
    hints.Linux.UseX11 = true;

    // macOS-specific hints
    hints.MacOS.EnableMetalLayer = true;
    hints.MacOS.UseRetinaDisplay = true;
    hints.MacOS.EnableVibrantWindow = false;

    printf("- Windows Compositor: %s\n", hints.Windows.EnableCompositor ? "Enabled" : "Disabled");
    printf("- Windows Mica: %s\n", hints.Windows.EnableMica ? "Enabled" : "Disabled");
    printf("- Windows Dark Mode: %s\n", hints.Windows.UseImmersiveDarkMode ? "Enabled" : "Disabled");
    printf("- Linux Display: %s\n", hints.Linux.Display ? hints.Linux.Display : "Default");
    printf("- Linux Window Manager: %s\n", hints.Linux.WindowManager ? hints.Linux.WindowManager : "Default");
    printf("- macOS Metal Layer: %s\n", hints.MacOS.EnableMetalLayer ? "Enabled" : "Disabled");
    printf("- macOS Retina Display: %s\n", hints.MacOS.UseRetinaDisplay ? "Enabled" : "Disabled");

    printf("\n=== Platform Abstraction Test Completed ===\n");
}

int main(void)
{
    test_platform_abstraction();
    return 0;
}