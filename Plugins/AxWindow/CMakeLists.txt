cmake_minimum_required(VERSION 3.15)
project(AxWindow VERSION 1.0.0 LANGUAGES C)
include(CTest) # Enable unit testing via "make test" once the code has been compiled.

#
# Create Target
#

# Add the library
add_library(AxWindow SHARED)
add_library(Virspace::AxWindow ALIAS AxWindow)

# Add sources to target
target_sources(AxWindow
    PRIVATE
        include/AxWindow/AxWindow.h
        src/Win32Window.c
)

#
# Usage Requirements
#

# Add compiler features and definitions
target_compile_features(AxWindow PRIVATE c_std_11)
#target_compile_definitions(AxWindow
#	PRIVATE
#)

# Specify include directories to use when compiling a given target
target_include_directories(AxWindow
    PUBLIC
        $<INSTALL_INTERFACE:include/AxWindow> # Used for working with an 'install tree'
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> # Used for working with a 'build tree'
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/AxWindow
)

# Link dependencies
target_link_libraries(AxWindow
    PRIVATE
        Virspace::Foundation
)

#
# Install and Export
#

# Set the install destination for cmake files
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/AxWindow)

# Install include directory
include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Specify the name of the export set the targets belong to
install(TARGETS AxWindow
    EXPORT AxWindowTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/$<CONFIG>
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export the targets to a script
install(EXPORT AxWindowTargets
    FILE AxWindowTargets.cmake
    NAMESPACE Virspace::
    DESTINATION ${INSTALL_CONFIGDIR}
)

# Generate a config file that includes the exports
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/cmake/AxWindowConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxWindowConfig.cmake
    INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
    NO_SET_AND_CHECK_MACRO
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

# Generate the version file for the config file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxWindowConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install the Config, ConfigVersion and custom Find Module
install(FILES
    #${CMAKE_CURRENT_LIST_DIR}/FindAxWindow.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxWindowConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxWindowConfigVersion.cmake
    DESTINATION ${INSTALL_CONFIGDIR}
)

# Make this project available from the build directory
export(EXPORT AxWindowTargets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/AxWindowTargets.cmake
    NAMESPACE Virspace::
)

#
# Testing
#

#if(BUILD_TESTING)
#    add_subdirectory(tests)
#endif()