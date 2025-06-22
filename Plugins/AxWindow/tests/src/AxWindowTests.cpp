#include "AxWindow/AxWindow.h"
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstddef>

// Platform detection
#ifdef _WIN32
    #define AX_PLATFORM_WINDOWS_DETECTED
#elif defined(__linux__)
    #define AX_PLATFORM_LINUX_DETECTED
#elif defined(__APPLE__)
    #define AX_PLATFORM_MACOS_DETECTED
#endif

class AxWindowPlatformTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Any setup code needed for tests
    }

    void TearDown() override {
        // Any cleanup code needed for tests
    }
};

// Test platform type enum values are valid
TEST_F(AxWindowPlatformTest, PlatformTypeEnumValues) {
    // Test that enum values are within valid range
    EXPECT_GE(AX_PLATFORM_WINDOWS, 0);
    EXPECT_GE(AX_PLATFORM_LINUX, 0);
    EXPECT_GE(AX_PLATFORM_MACOS, 0);
    EXPECT_GE(AX_PLATFORM_UNKNOWN, 0);
    
    // Test that all enum values are different
    EXPECT_NE(AX_PLATFORM_WINDOWS, AX_PLATFORM_LINUX);
    EXPECT_NE(AX_PLATFORM_WINDOWS, AX_PLATFORM_MACOS);
    EXPECT_NE(AX_PLATFORM_WINDOWS, AX_PLATFORM_UNKNOWN);
    EXPECT_NE(AX_PLATFORM_LINUX, AX_PLATFORM_MACOS);
    EXPECT_NE(AX_PLATFORM_LINUX, AX_PLATFORM_UNKNOWN);
    EXPECT_NE(AX_PLATFORM_MACOS, AX_PLATFORM_UNKNOWN);
}

// Test platform features enum values are valid
TEST_F(AxWindowPlatformTest, PlatformFeaturesEnumValues) {
    // Test that all feature flags are non-zero and unique
    EXPECT_NE(AX_PLATFORM_FEATURE_DPI_AWARENESS, 0);
    EXPECT_NE(AX_PLATFORM_FEATURE_RAW_INPUT, 0);
    EXPECT_NE(AX_PLATFORM_FEATURE_HIGH_DPI, 0);
    EXPECT_NE(AX_PLATFORM_FEATURE_MULTI_MONITOR, 0);
    EXPECT_NE(AX_PLATFORM_FEATURE_OPENGL, 0);
    EXPECT_NE(AX_PLATFORM_FEATURE_DIRECTX, 0);
    EXPECT_NE(AX_PLATFORM_FEATURE_VULKAN, 0);
    EXPECT_NE(AX_PLATFORM_FEATURE_METAL, 0);
    EXPECT_NE(AX_PLATFORM_FEATURE_TOUCH, 0);
    EXPECT_NE(AX_PLATFORM_FEATURE_GAMEPAD, 0);
    EXPECT_NE(AX_PLATFORM_FEATURE_CLIPBOARD, 0);
    EXPECT_NE(AX_PLATFORM_FEATURE_DRAG_DROP, 0);
    
    // Test that all feature flags are unique (no duplicates)
    uint32_t features[] = {
        AX_PLATFORM_FEATURE_DPI_AWARENESS,
        AX_PLATFORM_FEATURE_RAW_INPUT,
        AX_PLATFORM_FEATURE_HIGH_DPI,
        AX_PLATFORM_FEATURE_MULTI_MONITOR,
        AX_PLATFORM_FEATURE_OPENGL,
        AX_PLATFORM_FEATURE_DIRECTX,
        AX_PLATFORM_FEATURE_VULKAN,
        AX_PLATFORM_FEATURE_METAL,
        AX_PLATFORM_FEATURE_TOUCH,
        AX_PLATFORM_FEATURE_GAMEPAD,
        AX_PLATFORM_FEATURE_CLIPBOARD,
        AX_PLATFORM_FEATURE_DRAG_DROP
    };
    
    for (size_t i = 0; i < sizeof(features)/sizeof(features[0]); i++) {
        for (size_t j = i + 1; j < sizeof(features)/sizeof(features[0]); j++) {
            EXPECT_NE(features[i], features[j]) 
                << "Feature flags " << i << " and " << j << " are identical";
        }
    }
}

// Test structure sizes
TEST_F(AxWindowPlatformTest, StructureSizes) {
    EXPECT_GT(sizeof(AxPlatformInfo), 0);
    EXPECT_GT(sizeof(AxPlatformHints), 0);
    
    // Log the actual sizes for reference
    printf("Platform Info Structure Size: %zu bytes\n", sizeof(AxPlatformInfo));
    printf("Platform Hints Structure Size: %zu bytes\n", sizeof(AxPlatformHints));
}

// Test platform hints initialization
TEST_F(AxWindowPlatformTest, PlatformHintsInitialization) {
    AxPlatformHints hints = {};
    
    // Test that we can set Windows-specific hints
    hints.Windows.EnableCompositor = true;
    hints.Windows.EnableAcrylic = false;
    hints.Windows.EnableMica = true;
    hints.Windows.DisableWindowShadows = false;
    hints.Windows.UseImmersiveDarkMode = true;
    
    EXPECT_TRUE(hints.Windows.EnableCompositor);
    EXPECT_FALSE(hints.Windows.EnableAcrylic);
    EXPECT_TRUE(hints.Windows.EnableMica);
    EXPECT_FALSE(hints.Windows.DisableWindowShadows);
    EXPECT_TRUE(hints.Windows.UseImmersiveDarkMode);
    
    // Test that we can set Linux-specific hints
    hints.Linux.Display = ":0";
    hints.Linux.WindowManager = "i3";
    hints.Linux.UseWayland = false;
    hints.Linux.UseX11 = true;
    
    EXPECT_STREQ(hints.Linux.Display, ":0");
    EXPECT_STREQ(hints.Linux.WindowManager, "i3");
    EXPECT_FALSE(hints.Linux.UseWayland);
    EXPECT_TRUE(hints.Linux.UseX11);
    
    // Test that we can set macOS-specific hints
    hints.MacOS.EnableMetalLayer = true;
    hints.MacOS.UseRetinaDisplay = true;
    hints.MacOS.EnableVibrantWindow = false;
    
    EXPECT_TRUE(hints.MacOS.EnableMetalLayer);
    EXPECT_TRUE(hints.MacOS.UseRetinaDisplay);
    EXPECT_FALSE(hints.MacOS.EnableVibrantWindow);
}

// Test platform detection works on any platform
TEST_F(AxWindowPlatformTest, PlatformDetection) {
    printf("Running platform detection test\n");
    
    // Test that we can detect which platform we're on
    bool platform_detected = false;
    
#ifdef AX_PLATFORM_WINDOWS_DETECTED
    printf("Detected Windows platform\n");
    platform_detected = true;
#endif

#ifdef AX_PLATFORM_LINUX_DETECTED
    printf("Detected Linux platform\n");
    platform_detected = true;
#endif

#ifdef AX_PLATFORM_MACOS_DETECTED
    printf("Detected macOS platform\n");
    platform_detected = true;
#endif

    // At least one platform should be detected
    EXPECT_TRUE(platform_detected) << "No platform was detected";
    
    // Test that platform hints work for the detected platform
    AxPlatformHints hints = {};
    
#ifdef AX_PLATFORM_WINDOWS_DETECTED
    hints.Windows.EnableCompositor = true;
    hints.Windows.UseImmersiveDarkMode = true;
    EXPECT_TRUE(hints.Windows.EnableCompositor);
    EXPECT_TRUE(hints.Windows.UseImmersiveDarkMode);
#endif

#ifdef AX_PLATFORM_LINUX_DETECTED
    hints.Linux.Display = ":0";
    hints.Linux.WindowManager = "i3";
    hints.Linux.UseX11 = true;
    EXPECT_STREQ(hints.Linux.Display, ":0");
    EXPECT_STREQ(hints.Linux.WindowManager, "i3");
    EXPECT_TRUE(hints.Linux.UseX11);
#endif

#ifdef AX_PLATFORM_MACOS_DETECTED
    hints.MacOS.EnableMetalLayer = true;
    hints.MacOS.UseRetinaDisplay = true;
    EXPECT_TRUE(hints.MacOS.EnableMetalLayer);
    EXPECT_TRUE(hints.MacOS.UseRetinaDisplay);
#endif
}

// Test platform info structure
TEST_F(AxWindowPlatformTest, PlatformInfoStructure) {
    AxPlatformInfo info = {};

    // Test that we can initialize the structure
    EXPECT_EQ(info.Type, 0);
    EXPECT_EQ(info.Features, 0);
    EXPECT_EQ(info.IsInitialized, false);
    EXPECT_EQ(info.MajorVersion, 0);
    EXPECT_EQ(info.MinorVersion, 0);
    EXPECT_EQ(info.BuildNumber, 0);

    // Test that string fields are accessible
    EXPECT_STREQ(info.Name, "");
    EXPECT_STREQ(info.Version, "");
}

// Test platform feature combinations
TEST_F(AxWindowPlatformTest, PlatformFeatureCombinations) {
    // Test that features can be combined
    uint32_t combined_features = AX_PLATFORM_FEATURE_DPI_AWARENESS | 
                                AX_PLATFORM_FEATURE_RAW_INPUT | 
                                AX_PLATFORM_FEATURE_OPENGL;

    EXPECT_TRUE(combined_features & AX_PLATFORM_FEATURE_DPI_AWARENESS);
    EXPECT_TRUE(combined_features & AX_PLATFORM_FEATURE_RAW_INPUT);
    EXPECT_TRUE(combined_features & AX_PLATFORM_FEATURE_OPENGL);
    EXPECT_FALSE(combined_features & AX_PLATFORM_FEATURE_DIRECTX);

    // Test feature removal
    combined_features &= ~AX_PLATFORM_FEATURE_RAW_INPUT;
    EXPECT_TRUE(combined_features & AX_PLATFORM_FEATURE_DPI_AWARENESS);
    EXPECT_FALSE(combined_features & AX_PLATFORM_FEATURE_RAW_INPUT);
    EXPECT_TRUE(combined_features & AX_PLATFORM_FEATURE_OPENGL);
}

// Test that all platform-specific hint fields are accessible
TEST_F(AxWindowPlatformTest, PlatformHintsFieldAccess) {
    AxPlatformHints hints = {};

    // Test Windows fields
    hints.Windows.EnableCompositor = true;
    hints.Windows.EnableAcrylic = false;
    hints.Windows.EnableMica = true;
    hints.Windows.DisableWindowShadows = false;
    hints.Windows.UseImmersiveDarkMode = true;

    // Test Linux fields
    hints.Linux.Display = "test_display";
    hints.Linux.WindowManager = "test_wm";
    hints.Linux.UseWayland = true;
    hints.Linux.UseX11 = false;

    // Test macOS fields
    hints.MacOS.EnableMetalLayer = true;
    hints.MacOS.UseRetinaDisplay = false;
    hints.MacOS.EnableVibrantWindow = true;

    // Verify all fields can be set and read
    EXPECT_TRUE(hints.Windows.EnableCompositor);
    EXPECT_FALSE(hints.Windows.EnableAcrylic);
    EXPECT_TRUE(hints.Windows.EnableMica);
    EXPECT_FALSE(hints.Windows.DisableWindowShadows);
    EXPECT_TRUE(hints.Windows.UseImmersiveDarkMode);

    EXPECT_STREQ(hints.Linux.Display, "test_display");
    EXPECT_STREQ(hints.Linux.WindowManager, "test_wm");
    EXPECT_TRUE(hints.Linux.UseWayland);
    EXPECT_FALSE(hints.Linux.UseX11);

    EXPECT_TRUE(hints.MacOS.EnableMetalLayer);
    EXPECT_FALSE(hints.MacOS.UseRetinaDisplay);
    EXPECT_TRUE(hints.MacOS.EnableVibrantWindow);
}

// Test that platform type is one of the valid values
TEST_F(AxWindowPlatformTest, PlatformTypeValidation) {
    // Test that platform type enum values are within valid range
    // This ensures the enum is properly defined regardless of platform
    EXPECT_GE(AX_PLATFORM_WINDOWS, 0);
    EXPECT_GE(AX_PLATFORM_LINUX, 0);
    EXPECT_GE(AX_PLATFORM_MACOS, 0);
    EXPECT_GE(AX_PLATFORM_UNKNOWN, 0);

    // Test that we can assign and compare platform types
    enum AxPlatformType test_type = AX_PLATFORM_WINDOWS;
    EXPECT_EQ(test_type, AX_PLATFORM_WINDOWS);

    test_type = AX_PLATFORM_LINUX;
    EXPECT_EQ(test_type, AX_PLATFORM_LINUX);

    test_type = AX_PLATFORM_MACOS;
    EXPECT_EQ(test_type, AX_PLATFORM_MACOS);

    test_type = AX_PLATFORM_UNKNOWN;
    EXPECT_EQ(test_type, AX_PLATFORM_UNKNOWN);
}

// Test that feature flags can be used in bitwise operations
TEST_F(AxWindowPlatformTest, FeatureFlagBitwiseOperations) {
    // Test OR operation
    uint32_t features = AX_PLATFORM_FEATURE_DPI_AWARENESS | AX_PLATFORM_FEATURE_OPENGL;
    EXPECT_TRUE(features & AX_PLATFORM_FEATURE_DPI_AWARENESS);
    EXPECT_TRUE(features & AX_PLATFORM_FEATURE_OPENGL);
    EXPECT_FALSE(features & AX_PLATFORM_FEATURE_DIRECTX);

    // Test AND operation
    features &= AX_PLATFORM_FEATURE_DPI_AWARENESS;
    EXPECT_TRUE(features & AX_PLATFORM_FEATURE_DPI_AWARENESS);
    EXPECT_FALSE(features & AX_PLATFORM_FEATURE_OPENGL);

    // Test XOR operation
    features ^= AX_PLATFORM_FEATURE_OPENGL;
    EXPECT_TRUE(features & AX_PLATFORM_FEATURE_DPI_AWARENESS);
    EXPECT_TRUE(features & AX_PLATFORM_FEATURE_OPENGL);

    features ^= AX_PLATFORM_FEATURE_DPI_AWARENESS;
    EXPECT_FALSE(features & AX_PLATFORM_FEATURE_DPI_AWARENESS);
    EXPECT_TRUE(features & AX_PLATFORM_FEATURE_OPENGL);
}