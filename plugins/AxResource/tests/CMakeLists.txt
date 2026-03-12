cmake_minimum_required(VERSION 3.15)
project(AxResourceTests VERSION 1.0.0 LANGUAGES CXX)
include(GoogleTest)

# GoogleTest requires at least C++11
set(CMAKE_CXX_STANDARD 11)

#
# Get Google Test
#

include(FetchContent)
FetchContent_Declare(
  googletest
  # Specify the commit you depend on and update it regularly.
  URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Get google test
FetchContent_MakeAvailable(googletest)

#
# Create Target
#

# Add the executable
add_executable(AxResourceTests)

# Add sources to target
target_sources(AxResourceTests
    PRIVATE
        src/main.cpp
        src/ResourceAPITests.cpp
)

#
# Usage Requirements
#

# Add compiler features and definitions
target_compile_features(AxResourceTests PRIVATE cxx_std_11)
target_compile_definitions(AxResourceTests
    PRIVATE
        AXON_LINKS_FOUNDATION
)

# Include directories for AxResource and Engine headers
target_include_directories(AxResourceTests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../AxOpenGL/include
)

# Link dependencies
target_link_libraries(AxResourceTests
    PRIVATE
        Virspace::Foundation
        Virspace::AxResource
        gtest
        gtest_main
)

# Discover Tests
gtest_discover_tests(AxResourceTests)
