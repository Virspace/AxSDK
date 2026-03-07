/**
 * AxBuildConfigTests.cpp - Tests for build configuration preprocessor defines
 *
 * These tests verify that the AX_BUILD_CONFIG CMake variable correctly
 * propagates compile definitions to all targets. The tests use #ifdef
 * checks to verify which defines are active at compile time.
 *
 * When built with the default configuration (Development), the tests
 * verify that AX_DEVELOPMENT and AX_ENABLE_ASSERTS are defined, and
 * that AX_DEBUG and AX_SHIPPING are NOT defined.
 */

#include "gtest/gtest.h"

// ---------------------------------------------------------------------------
// Test 1: AX_DEBUG is defined only when building in Debug mode
// ---------------------------------------------------------------------------
TEST(BuildConfig, AxDebugDefinedOnlyInDebug)
{
#if defined(AX_DEBUG)
    // If we reach here, AX_DEBUG is defined -- this should only happen in Debug builds
    SUCCEED() << "AX_DEBUG is defined (expected in Debug configuration)";
    // Verify the other mode defines are NOT also set
    #if defined(AX_DEVELOPMENT)
        FAIL() << "AX_DEBUG and AX_DEVELOPMENT should not both be defined";
    #endif
    #if defined(AX_SHIPPING)
        FAIL() << "AX_DEBUG and AX_SHIPPING should not both be defined";
    #endif
#else
    // AX_DEBUG is not defined -- this is expected in Development and Shipping
    #if defined(AX_DEVELOPMENT) || defined(AX_SHIPPING)
        SUCCEED() << "AX_DEBUG is correctly not defined in non-Debug configuration";
    #else
        FAIL() << "None of AX_DEBUG, AX_DEVELOPMENT, or AX_SHIPPING are defined";
    #endif
#endif
}

// ---------------------------------------------------------------------------
// Test 2: AX_DEVELOPMENT is defined only when building in Development mode
// ---------------------------------------------------------------------------
TEST(BuildConfig, AxDevelopmentDefinedOnlyInDevelopment)
{
#if defined(AX_DEVELOPMENT)
    // If we reach here, AX_DEVELOPMENT is defined -- expected in Development builds
    SUCCEED() << "AX_DEVELOPMENT is defined (expected in Development configuration)";
    #if defined(AX_DEBUG)
        FAIL() << "AX_DEVELOPMENT and AX_DEBUG should not both be defined";
    #endif
    #if defined(AX_SHIPPING)
        FAIL() << "AX_DEVELOPMENT and AX_SHIPPING should not both be defined";
    #endif
#else
    // AX_DEVELOPMENT is not defined -- expected in Debug and Shipping
    #if defined(AX_DEBUG) || defined(AX_SHIPPING)
        SUCCEED() << "AX_DEVELOPMENT is correctly not defined in non-Development configuration";
    #else
        FAIL() << "None of AX_DEBUG, AX_DEVELOPMENT, or AX_SHIPPING are defined";
    #endif
#endif
}

// ---------------------------------------------------------------------------
// Test 3: AX_SHIPPING is defined only when building in Shipping mode
// ---------------------------------------------------------------------------
TEST(BuildConfig, AxShippingDefinedOnlyInShipping)
{
#if defined(AX_SHIPPING)
    // If we reach here, AX_SHIPPING is defined -- expected in Shipping builds
    SUCCEED() << "AX_SHIPPING is defined (expected in Shipping configuration)";
    #if defined(AX_DEBUG)
        FAIL() << "AX_SHIPPING and AX_DEBUG should not both be defined";
    #endif
    #if defined(AX_DEVELOPMENT)
        FAIL() << "AX_SHIPPING and AX_DEVELOPMENT should not both be defined";
    #endif
#else
    // AX_SHIPPING is not defined -- expected in Debug and Development
    #if defined(AX_DEBUG) || defined(AX_DEVELOPMENT)
        SUCCEED() << "AX_SHIPPING is correctly not defined in non-Shipping configuration";
    #else
        FAIL() << "None of AX_DEBUG, AX_DEVELOPMENT, or AX_SHIPPING are defined";
    #endif
#endif
}

// ---------------------------------------------------------------------------
// Test 4: AX_ENABLE_ASSERTS is defined in Debug and Development but NOT Shipping
// ---------------------------------------------------------------------------
TEST(BuildConfig, AxEnableAssertsDefinedInDebugAndDevelopment)
{
#if defined(AX_SHIPPING)
    // In Shipping mode, AX_ENABLE_ASSERTS should NOT be defined
    #if defined(AX_ENABLE_ASSERTS)
        FAIL() << "AX_ENABLE_ASSERTS should not be defined in Shipping configuration";
    #else
        SUCCEED() << "AX_ENABLE_ASSERTS is correctly not defined in Shipping configuration";
    #endif
#else
    // In Debug or Development, AX_ENABLE_ASSERTS SHOULD be defined
    #if defined(AX_ENABLE_ASSERTS)
        SUCCEED() << "AX_ENABLE_ASSERTS is correctly defined in Debug/Development configuration";
    #else
        FAIL() << "AX_ENABLE_ASSERTS should be defined in Debug/Development configuration";
    #endif
#endif
}
