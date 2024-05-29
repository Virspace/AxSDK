cmake_minimum_required(VERSION 3.15)
project(FoundationTests VERSION 1.0.0 LANGUAGES CXX)
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
add_executable(FoundationTests)

# Add sources to target
target_sources(FoundationTests
    PRIVATE
        src/main.cpp
        src/AxAllocatorTests.cpp
        #src/AxArrayTests.cpp
        #src/CameraTests.cpp
        src/HashmapTests.cpp
        src/LinkedListTests.cpp
        src/PlatformTests.cpp
        src/MathTests.cpp
)

#
# Usage Requirements
#

# Add compiler features and definitions
target_compile_features(FoundationTests PRIVATE cxx_std_11)
target_compile_definitions(FoundationTests
	PRIVATE
        AXON_LINKS_FOUNDATION
)

# Specify include directories to use when compiling a given target
# target_include_directories(FoundationTests
#     PUBLIC
#         $<INSTALL_INTERFACE:include/FoundationTests> # Used for working with an 'install tree'
#         $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> # Used for working with a 'build tree'
#     PRIVATE
#         ${CMAKE_CURRENT_SOURCE_DIR}/include/FoundationTests
# )

# Link dependencies
target_link_libraries(FoundationTests
    PRIVATE
        Virspace::Foundation
        gtest
        gtest_main
)

# Discover Tests
gtest_discover_tests(FoundationTests)

#
# Install and Export
#

# Set the install destination for cmake files
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/FoundationTests)

# Install include directory
include(GNUInstallDirs)
#install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Specify the name of the export set the targets belong to
install(TARGETS FoundationTests
    EXPORT FoundationTestsTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/$<CONFIG>
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export the targets to a script
install(EXPORT FoundationTestsTargets
    FILE FoundationTestsTargets.cmake
    NAMESPACE Virspace::
    DESTINATION ${INSTALL_CONFIGDIR}
)

# Generate a config file that includes the exports
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/cmake/FoundationTestsConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/FoundationTestsConfig.cmake
    INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
    NO_SET_AND_CHECK_MACRO
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

# Generate the version file for the config file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/FoundationTestsConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install the Config, ConfigVersion and custom Find Module
install(FILES
    #${CMAKE_CURRENT_LIST_DIR}/FindFoundationTests.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/FoundationTestsConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/FoundationTestsConfigVersion.cmake
    DESTINATION ${INSTALL_CONFIGDIR}
)

# Make this project available from the build directory
export(EXPORT FoundationTestsTargets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/FoundationTestsTargets.cmake
    NAMESPACE Virspace::
)

# keeps your CACHE cleaner
mark_as_advanced(
    BUILD_GMOCK BUILD_GTEST BUILD_SHARED_LIBS
    gmock_build_tests gtest_build_samples gtest_build_tests
    gtest_disable_pthreads gtest_force_shared_crt gtest_hide_internal_symbols
)

set_target_properties(gtest PROPERTIES FOLDER External)
set_target_properties(gtest_main PROPERTIES FOLDER External)
set_target_properties(gmock PROPERTIES FOLDER External)
set_target_properties(gmock_main PROPERTIES FOLDER External)