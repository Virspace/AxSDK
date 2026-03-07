# Preamble
cmake_minimum_required(VERSION 3.15)
project(AxSDK VERSION 1.0.0)
include(CTest)

# Set compiler flags for x64 architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -m64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64")
else()
    message(FATAL_ERROR "This project requires x64 architecture")
endif()

# =============================================================================
# Build Configuration: Debug, Development (default), or Shipping
# =============================================================================

set(AX_BUILD_CONFIG "Development" CACHE STRING "Build configuration: Debug, Development, or Shipping")

# Validate AX_BUILD_CONFIG
if(NOT AX_BUILD_CONFIG STREQUAL "Debug" AND
   NOT AX_BUILD_CONFIG STREQUAL "Development" AND
   NOT AX_BUILD_CONFIG STREQUAL "Shipping")
    message(FATAL_ERROR "AX_BUILD_CONFIG must be one of: Debug, Development, Shipping (got '${AX_BUILD_CONFIG}')")
endif()

# Map AX_BUILD_CONFIG to CMAKE_BUILD_TYPE
if(AX_BUILD_CONFIG STREQUAL "Debug")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
elseif(AX_BUILD_CONFIG STREQUAL "Development")
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type" FORCE)
elseif(AX_BUILD_CONFIG STREQUAL "Shipping")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# Add compile definitions based on AX_BUILD_CONFIG
if(AX_BUILD_CONFIG STREQUAL "Debug")
    add_compile_definitions(AX_DEBUG AX_ENABLE_ASSERTS)
elseif(AX_BUILD_CONFIG STREQUAL "Development")
    add_compile_definitions(AX_DEVELOPMENT AX_ENABLE_ASSERTS)
elseif(AX_BUILD_CONFIG STREQUAL "Shipping")
    add_compile_definitions(AX_SHIPPING)
    add_compile_definitions(AX_LOG_COMPILE_LEVEL=AX_LOG_LEVEL_WARNING)
endif()

message(STATUS "AxSDK build configuration: ${AX_BUILD_CONFIG} (CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})")

# =============================================================================
# Project-wide Setup
# =============================================================================

enable_testing()

if (WIN32)
    add_definitions(-DAX_OS_WINDOWS)
    add_definitions(-DWINVER=0x0A00)
endif()

# Generate debug symbols on all non-MSVC toolchains
add_compile_options(-g)

# Basic directory setup
set(CMAKE_INSTALL_PREFIX           ${CMAKE_BINARY_DIR}/install)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# =============================================================================
# Shared FetchContent Dependencies
# =============================================================================

include(FetchContent)

# STB for image loading (used by AxEngine, AxResource, and examples)
FetchContent_Declare(
    axsdk_stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
)

FetchContent_GetProperties(axsdk_stb)
if(NOT axsdk_stb_POPULATED)
    FetchContent_Populate(axsdk_stb)
    message(STATUS "Fetching stb (shared)")

    add_library(axsdk_stb INTERFACE)
    target_include_directories(axsdk_stb INTERFACE ${axsdk_stb_SOURCE_DIR})
endif()

# CGLTF for GLTF model loading (used by AxEngine and examples)
FetchContent_Declare(
    cgltf
    GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
    GIT_TAG master
)

FetchContent_GetProperties(cgltf)
if(NOT cgltf_POPULATED)
    FetchContent_Populate(cgltf)
    message(STATUS "Fetching cgltf (shared)")

    add_library(cgltf INTERFACE ${cgltf_SOURCE_DIR}/cgltf.h)
    target_include_directories(cgltf INTERFACE ${cgltf_SOURCE_DIR})
endif()

# =============================================================================
# Subdirectories
# =============================================================================

add_subdirectory(foundation)
set_target_properties(Foundation PROPERTIES FOLDER Foundation)

add_subdirectory(plugins)
add_subdirectory(engine)
add_subdirectory(examples)
