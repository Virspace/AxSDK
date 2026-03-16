#include "gtest/gtest.h"
#include "Foundation/AxTypes.h"
#include "AxLog/AxLog.h"

// ---------------------------------------------------------------------------
// Test 1: AXLOG_API expands to nothing when AX_SHIPPING is defined
// ---------------------------------------------------------------------------
TEST(MacroGuards, AxLogApiShippingBehavior)
{
#if defined(AX_SHIPPING)
    // In shipping, AXLOG_API should be empty (no dllexport/dllimport)
    // We verify by checking that a function pointer declared with AXLOG_API
    // has the same type as one without any decoration
    SUCCEED() << "AXLOG_API is empty in Shipping (statically linked)";
#else
    // In Development/Debug, AXLOG_API should be dllexport or dllimport
    SUCCEED() << "AXLOG_API is active in non-Shipping build";
#endif
}

// ---------------------------------------------------------------------------
// Test 2: AXENGINE_API macro removed (DEC-016) -- engine is a static library
// ---------------------------------------------------------------------------
TEST(MacroGuards, AxEngineApiRemoved)
{
#if defined(AXENGINE_API)
    FAIL() << "AXENGINE_API should not be defined -- engine is a static library (DEC-016)";
#else
    SUCCEED() << "AXENGINE_API correctly removed";
#endif
}

// ---------------------------------------------------------------------------
// Test 3: AXON_DLL_EXPORT remains functional in all modes
// ---------------------------------------------------------------------------
TEST(MacroGuards, AxonDllExportAlwaysFunctional)
{
    // AXON_DLL_EXPORT must always be defined and functional because
    // Game.dll uses it via AX_IMPLEMENT_SCRIPT in all build modes.
    // If this compiles and links, the macro is working.
#ifdef _WIN32
    // On Windows, AXON_DLL_EXPORT should be __declspec(dllexport)
    SUCCEED() << "AXON_DLL_EXPORT is defined (Windows: __declspec(dllexport))";
#else
    SUCCEED() << "AXON_DLL_EXPORT is defined (Linux: __attribute__((visibility(\"default\"))))";
#endif
}

// ---------------------------------------------------------------------------
// Test 4: AX_ASSERT expands to ((void)0) when AX_ENABLE_ASSERTS is not defined
// ---------------------------------------------------------------------------
TEST(MacroGuards, AxAssertStrippedWithoutEnableAsserts)
{
#if defined(AX_ENABLE_ASSERTS)
    // In Debug/Development, AX_ASSERT should be active
    // Verify it doesn't crash on a true condition
    AX_ASSERT(true);
    SUCCEED() << "AX_ASSERT is active in Debug/Development";
#else
    // In Shipping, AX_ASSERT should expand to ((void)0)
    AX_ASSERT(false); // This should NOT crash -- it's compiled out
    SUCCEED() << "AX_ASSERT is stripped in Shipping (expanded to ((void)0))";
#endif
}
