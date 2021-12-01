cmake_minimum_required(VERSION 3.15)
project(AxOpenGL VERSION 1.0.0 LANGUAGES C)
include(CTest) # Enable unit testing via "make test" once the code has been compiled.

#
# Create Target
#

# Add the library
add_library(AxOpenGL SHARED)
add_library(Virspace::AxOpenGL ALIAS AxOpenGL)

# Add sources to target
target_sources(AxOpenGL
    PRIVATE
        include/AxOpenGL/AxOpenGL.h
        include/GL/gl3w.h
        include/GL/glcorearb.h
        src/gl3w.c
        #include/glad/glad.h
        #include/KHR/khrplatform.h
        #src/glad.c
        src/AxOpenGLWin32.c
)

#
# Usage Requirements
#

# Add compiler features and definitions
target_compile_features(AxOpenGL PRIVATE c_std_11)
target_compile_definitions(AxOpenGL
	PRIVATE
        GL_GLEXT_PROTOTYPES
        AXON_LINKS_FOUNDATION
)

include_directories("$<JOIN:$<TARGET_PROPERTY:AxWindow,INCLUDE_DIRECTORIES>,;>")

# Specify include directories to use when compiling a given target
target_include_directories(AxOpenGL
    PUBLIC
        $<INSTALL_INTERFACE:include/AxOpenGL> # Used for working with an 'install tree'
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> # Used for working with a 'build tree'
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/AxOpenGL
)

# Link dependencies
target_link_libraries(AxOpenGL
    PUBLIC
        Virspace::Foundation
    PRIVATE
        Virspace::Drawable
        opengl32
)

#
# Install and Export
#

# Set the install destination for cmake files
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/AxOpenGL)

# Install include directory
include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Specify the name of the export set the targets belong to
install(TARGETS AxOpenGL
    EXPORT AxOpenGLTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/$<CONFIG>
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export the targets to a script
install(EXPORT AxOpenGLTargets
    FILE AxOpenGLTargets.cmake
    NAMESPACE Virspace::
    DESTINATION ${INSTALL_CONFIGDIR}
)

# Generate a config file that includes the exports
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/cmake/AxOpenGLConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxOpenGLConfig.cmake
    INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
    NO_SET_AND_CHECK_MACRO
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

# Generate the version file for the config file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxOpenGLConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install the Config, ConfigVersion and custom Find Module
install(FILES
    #${CMAKE_CURRENT_LIST_DIR}/FindAxOpenGL.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxOpenGLConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxOpenGLConfigVersion.cmake
    DESTINATION ${INSTALL_CONFIGDIR}
)

# Make this project available from the build directory
export(EXPORT AxOpenGLTargets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/AxOpenGLTargets.cmake
    NAMESPACE Virspace::
)