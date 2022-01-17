cmake_minimum_required(VERSION 3.15)
project(AxAudio VERSION 1.0.0 LANGUAGES C)
include(CTest) # Enable unit testing via "make test" once the code has been compiled.

#
# Create Target
#

# Add the library
add_library(AxAudio SHARED)
add_library(Virspace::AxAudio ALIAS AxAudio)

# Add sources to target
target_sources(AxAudio
    PRIVATE
        include/AxAudio/AxAudio.h
        src/Win32Audio.c
)

#
# Usage Requirements
#

# Add compiler features and definitions
target_compile_features(AxAudio PRIVATE c_std_11)
#target_compile_definitions(AxAudio
#	PRIVATE
#)

# Specify include directories to use when compiling a given target
target_include_directories(AxAudio
    PUBLIC
        $<INSTALL_INTERFACE:include/AxAudio> # Used for working with an 'install tree'
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> # Used for working with a 'build tree'
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/AxAudio
)

# Link dependencies
target_link_libraries(AxAudio
    PRIVATE
        Virspace::Foundation
)

#
# Install and Export
#

# Set the install destination for cmake files
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/AxAudio)

# Install include directory
include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Specify the name of the export set the targets belong to
install(TARGETS AxAudio
    EXPORT AxAudioTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/$<CONFIG>
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export the targets to a script
install(EXPORT AxAudioTargets
    FILE AxAudioTargets.cmake
    NAMESPACE Virspace::
    DESTINATION ${INSTALL_CONFIGDIR}
)

# Generate a config file that includes the exports
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/cmake/AxAudioConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxAudioConfig.cmake
    INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
    NO_SET_AND_CHECK_MACRO
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

# Generate the version file for the config file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxAudioConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install the Config, ConfigVersion and custom Find Module
install(FILES
    #${CMAKE_CURRENT_LIST_DIR}/FindAxAudio.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxAudioConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxAudioConfigVersion.cmake
    DESTINATION ${INSTALL_CONFIGDIR}
)

# Make this project available from the build directory
export(EXPORT AxAudioTargets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/AxAudioTargets.cmake
    NAMESPACE Virspace::
)

#
# Testing
#

#if(BUILD_TESTING)
#    add_subdirectory(tests)
#endif()