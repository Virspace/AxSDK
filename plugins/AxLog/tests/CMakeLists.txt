cmake_minimum_required(VERSION 3.15)
project(AxLogTests VERSION 1.0.0 LANGUAGES C CXX)
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
add_executable(AxLogTests)

# Add sources to target
target_sources(AxLogTests
    PRIVATE
        src/main.cpp
        src/AxLogTests.cpp
        src/AxLogCTest.c
)

#
# Usage Requirements
#

# Add compiler features and definitions
target_compile_features(AxLogTests PRIVATE cxx_std_17)
target_compile_definitions(AxLogTests
    PRIVATE
        AXON_LINKS_FOUNDATION
)

# Link dependencies
target_link_libraries(AxLogTests
    PRIVATE
        Virspace::AxLog
        gtest
        gtest_main
)

# Discover Tests
gtest_discover_tests(AxLogTests)

#
# Install and Export
#

# Set the install destination for cmake files
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/AxLogTests)

# Install include directory
include(GNUInstallDirs)

# Specify the name of the export set the targets belong to
install(TARGETS AxLogTests
    EXPORT AxLogTestsTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/$<CONFIG>
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export the targets to a script
install(EXPORT AxLogTestsTargets
    FILE AxLogTestsTargets.cmake
    NAMESPACE Virspace::
    DESTINATION ${INSTALL_CONFIGDIR}
)

# Generate a config file that includes the exports
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/cmake/AxLogTestsConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxLogTestsConfig.cmake
    INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
    NO_SET_AND_CHECK_MACRO
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

# Generate the version file for the config file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxLogTestsConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install the Config, ConfigVersion and custom Find Module
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxLogTestsConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxLogTestsConfigVersion.cmake
    DESTINATION ${INSTALL_CONFIGDIR}
)

# Make this project available from the build directory
export(EXPORT AxLogTestsTargets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/AxLogTestsTargets.cmake
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
