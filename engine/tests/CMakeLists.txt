cmake_minimum_required(VERSION 3.15)
project(AxEngineTests VERSION 1.0.0 LANGUAGES CXX)
include(GoogleTest)

# GoogleTest requires at least C++11
set(CMAKE_CXX_STANDARD 11)

#
# Get Google Test
#

include(FetchContent)
FetchContent_Declare(
  googletest
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
add_executable(AxEngineTests)

# Add sources to target
target_sources(AxEngineTests
    PRIVATE
        src/main.cpp
        src/AxNodeTests.cpp
        src/AxComponentFactoryTests.cpp
        src/AxSceneExTests.cpp
        src/AxSceneClassTests.cpp
        src/AxStandardComponentTests.cpp
        src/AxCoreSystemTests.cpp
        src/AxEventBusTests.cpp
        src/AxParserExTests.cpp
        src/AxIntegrationTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/AxNode.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/AxComponentFactory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/AxSystemFactory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/AxScene.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Plugins/AxResource/src/AxStandardComponents.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/AxEventBus.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Plugins/AxResource/src/AxTransformSystem.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Plugins/AxResource/src/AxRenderSystem.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Plugins/AxResource/src/AxScriptSystem.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Plugins/AxResource/src/AxScene.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Plugins/AxResource/src/AxCoreSystems.cpp
)

#
# Usage Requirements
#

# Add compiler features and definitions
target_compile_features(AxEngineTests PRIVATE cxx_std_23)
target_compile_definitions(AxEngineTests
    PRIVATE
        AXON_LINKS_FOUNDATION
        AXENGINE_EXPORTS
)

# Specify include directories
target_include_directories(AxEngineTests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Plugins/AxResource/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Plugins/AxResource/src
)

# Link dependencies
target_link_libraries(AxEngineTests
    PRIVATE
        Virspace::Foundation
        gtest
        gtest_main
)

# Discover Tests
gtest_discover_tests(AxEngineTests)

#
# Install and Export
#

# Set the install destination for cmake files
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/AxEngineTests)

# Install include directory
include(GNUInstallDirs)

# Specify the name of the export set the targets belong to
install(TARGETS AxEngineTests
    EXPORT AxEngineTestsTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/$<CONFIG>
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export the targets to a script
install(EXPORT AxEngineTestsTargets
    FILE AxEngineTestsTargets.cmake
    NAMESPACE Virspace::
    DESTINATION ${INSTALL_CONFIGDIR}
)

# Generate a config file that includes the exports
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/cmake/AxEngineTestsConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxEngineTestsConfig.cmake
    INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
    NO_SET_AND_CHECK_MACRO
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

# Generate the version file for the config file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxEngineTestsConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install the Config, ConfigVersion and custom Find Module
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxEngineTestsConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxEngineTestsConfigVersion.cmake
    DESTINATION ${INSTALL_CONFIGDIR}
)

# Make this project available from the build directory
export(EXPORT AxEngineTestsTargets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/AxEngineTestsTargets.cmake
    NAMESPACE Virspace::
)

# keeps your CACHE cleaner
mark_as_advanced(
    BUILD_GMOCK BUILD_GTEST BUILD_SHARED_LIBS
    gmock_build_tests gtest_build_samples gtest_build_tests
    gtest_disable_pthreads gtest_force_shared_crt gtest_hide_internal_symbols
)
