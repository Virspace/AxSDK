cmake_minimum_required(VERSION 3.15)
project(Foundation VERSION 1.0.0 LANGUAGES C CXX)

#if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest) # Enable unit testing via "make test" once the code has been compiled.
#endif()

#
# Create Target
#

# Add the library
add_library(Foundation STATIC)
add_library(Virspace::Foundation ALIAS Foundation)

# Add sources to target
target_sources(Foundation
    PRIVATE
        include/Foundation/APIRegistry.h
        include/Foundation/Application.h
        include/Foundation/AxArray.h
        include/Foundation/Camera.h
        include/Foundation/EditorPlugin.h
        include/Foundation/Hash.h
        include/Foundation/HashTable.h
        #include/Foundation/ImageLoader.h
        include/Foundation/Intrinsics.h
        include/Foundation/Math.h
        include/Foundation/Platform.h
        include/Foundation/Plugin.h
        include/Foundation/Types.h
        include/Foundation/LinkedList.h
        src/APIRegistry.c
        src/Camera.c
        #src/ImageLoader.c
        src/Hash.c
        src/HashTable.c
        src/Math.c
        src/Plugin.c
        src/Win32Platform.c
        src/LinkedList.c
)

#
# Usage Requirements
#

# Add compiler features and definitions
target_compile_features(Foundation PRIVATE c_std_11 )
target_compile_definitions(Foundation
    PRIVATE
        AXON_LINKS_FOUNDATION
)

# Specify include directories to use when compiling a given target
target_include_directories(Foundation
    PUBLIC
        $<INSTALL_INTERFACE:include/Foundation> # Used for working with an 'install tree'
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> # Used for working with a 'build tree'
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/Foundation
)

#
# Install and Export
#

# Set the install destination for cmake files
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/Foundation)

# Install include directory
include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Specify the name of the export set the targets belong to
install(TARGETS Foundation
    EXPORT FoundationTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/$<CONFIG>
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export the targets to a script
install(EXPORT FoundationTargets
    FILE FoundationTargets.cmake
    NAMESPACE Virspace::
    DESTINATION ${INSTALL_CONFIGDIR}
)

# Generate a config file that includes the exports
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/cmake/FoundationConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/FoundationConfig.cmake
    INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
    NO_SET_AND_CHECK_MACRO
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

# Generate the version file for the config file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/FoundationConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install the Config, ConfigVersion and custom Find Module
install(FILES
    #${CMAKE_CURRENT_LIST_DIR}/FindFoundation.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/FoundationConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/FoundationConfigVersion.cmake
    DESTINATION ${INSTALL_CONFIGDIR}
)

# Make this project available from the build directory
export(EXPORT FoundationTargets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/FoundationTargets.cmake
    NAMESPACE Virspace::
)

#
# Testing
#

# If someone else includes your package, and they use BUILD_TESTING, 
# they probably do not want your tests to build
#if((CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME OR AXON_BUILD_TESTING) AND BUILD_TESTING)
    add_subdirectory(tests)
#endif()