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

# Project-wide Setup
enable_testing()

if (WIN32)
    add_definitions(-DAX_OS_WINDOWS)
    add_definitions(-DWINVER=0x0A00)

    # If using plain clang/clang++, ensure we target MinGW-w64;
    # but *do not* force '-fuse-ld', let clang pick its default linker
    if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
        set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   --target=x86_64-w64-mingw32")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --target=x86_64-w64-mingw32")
    endif()
endif()

# Generate debug symbols on all non-MSVC toolchains
add_compile_options(-g)

# Basic directory setup
set(CMAKE_INSTALL_PREFIX           ${CMAKE_BINARY_DIR}/install)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(foundation)
set_target_properties(Foundation PROPERTIES FOLDER Foundation)

add_subdirectory(plugins)
add_subdirectory(examples)
