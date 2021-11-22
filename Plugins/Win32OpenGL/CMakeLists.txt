cmake_minimum_required(VERSION 3.15)
project(Win32OpenGL VERSION 1.0.0 LANGUAGES C)
include(CTest) # Enable unit testing via "make test" once the code has been compiled.

#
# Create Target
#

# Add the library
add_library(Win32OpenGL SHARED)
add_library(Virspace::Win32OpenGL ALIAS Win32OpenGL)

# Add sources to target
target_sources(Win32OpenGL
    PRIVATE
        include/Win32OpenGL/RendererOpenGL.h
        include/GL/gl3w.h
        include/GL/glcorearb.h
        src/gl3w.c
        #include/glad/glad.h
        #include/KHR/khrplatform.h
        #src/glad.c
        src/Win32OpenGL.c
        #src/imgui/imconfig.h
        #src/imgui/imgui.cpp
        #src/imgui/imgui.h
        #src/imgui/imgui_demo.cpp
        #src/imgui/imgui_draw.cpp
        #src/imgui/imgui_impl_opengl3.cpp
        #src/imgui/imgui_impl_opengl3.h
        #src/imgui/imgui_internal.h
        #src/imgui/imgui_tables.cpp
        #src/imgui/imgui_widgets.cpp
        #src/imgui/imstb_rectpack.h
        #src/imgui/imstb_textedit.h
        #src/imgui/imstb_truetype.h
)

#
# Usage Requirements
#

# Add compiler features and definitions
target_compile_features(Win32OpenGL PRIVATE c_std_11)
target_compile_definitions(Win32OpenGL
	PRIVATE
        GL_GLEXT_PROTOTYPES
)

include_directories("$<JOIN:$<TARGET_PROPERTY:AxWindow,INCLUDE_DIRECTORIES>,;>")

# Specify include directories to use when compiling a given target
target_include_directories(Win32OpenGL
    PUBLIC
        $<INSTALL_INTERFACE:include/Win32OpenGL> # Used for working with an 'install tree'
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> # Used for working with a 'build tree'
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/Win32OpenGL
)

# Link dependencies
target_link_libraries(Win32OpenGL
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
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/Win32OpenGL)

# Install include directory
include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Specify the name of the export set the targets belong to
install(TARGETS Win32OpenGL
    EXPORT Win32OpenGLTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/$<CONFIG>
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export the targets to a script
install(EXPORT Win32OpenGLTargets
    FILE Win32OpenGLTargets.cmake
    NAMESPACE Virspace::
    DESTINATION ${INSTALL_CONFIGDIR}
)

# Generate a config file that includes the exports
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/cmake/Win32OpenGLConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/Win32OpenGLConfig.cmake
    INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
    NO_SET_AND_CHECK_MACRO
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

# Generate the version file for the config file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/Win32OpenGLConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install the Config, ConfigVersion and custom Find Module
install(FILES
    #${CMAKE_CURRENT_LIST_DIR}/FindWin32OpenGL.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/Win32OpenGLConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/Win32OpenGLConfigVersion.cmake
    DESTINATION ${INSTALL_CONFIGDIR}
)

# Make this project available from the build directory
export(EXPORT Win32OpenGLTargets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/Win32OpenGLTargets.cmake
    NAMESPACE Virspace::
)