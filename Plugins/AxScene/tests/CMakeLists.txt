cmake_minimum_required(VERSION 3.15)
project(AxSceneTests VERSION 1.0.0 LANGUAGES CXX)
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
  URL https://github.com/google/googletest/archive/refs/tags/release-1.11.0.zip
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Get google test
FetchContent_MakeAvailable(googletest)

#
# Create Target
#

# Add the executable
add_executable(AxSceneTests)

# Add sources to target
target_sources(AxSceneTests
    PRIVATE
        src/main.cpp
        src/SceneParserTests.cpp
        src/SceneAllocatorTests.cpp
        src/SceneDataStructureTests.cpp
        src/SceneLoaderTests.cpp
        src/SceneEventsTests.cpp
)

#
# Usage Requirements
#

# Add compiler features and definitions
target_compile_features(AxSceneTests PRIVATE cxx_std_11)
target_compile_definitions(AxSceneTests
    PRIVATE
        AXON_LINKS_FOUNDATION
)

# Link dependencies
target_link_libraries(AxSceneTests
    PRIVATE
        Virspace::Foundation
        Virspace::AxScene
        gtest
        gtest_main
)

# Discover Tests
gtest_discover_tests(AxSceneTests)