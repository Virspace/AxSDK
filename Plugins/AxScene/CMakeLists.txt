cmake_minimum_required(VERSION 3.15)
project(AxScene VERSION 1.0.0 LANGUAGES C)
include(CTest)

#
# Create Target
#

# Add the library as a shared library (plugin)
add_library(AxScene SHARED)
add_library(Virspace::AxScene ALIAS AxScene)

# Add sources to target
target_sources(AxScene
    PRIVATE
        include/AxScene/AxScene.h
        src/AxScene.c
)

#
# Usage Requirements
#

# Add compiler features and definitions
target_compile_features(AxScene PRIVATE c_std_11)
# target_compile_definitions(AxScene
#     PRIVATE
#         AXON_LINKS_FOUNDATION
# )

include_directories("$<JOIN:$<TARGET_PROPERTY:AxOpenGL,INCLUDE_DIRECTORIES>,;>")

# Specify include directories
target_include_directories(AxScene
    PUBLIC
        $<INSTALL_INTERFACE:include/AxScene>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/AxScene
)

target_link_libraries(AxScene
    PRIVATE
        Virspace::Foundation
)

#
# Install and Export
#

# Set the install destination for cmake files
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/AxScene)

# Install include directory
include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Specify the name of the export set the targets belong to
install(TARGETS AxScene
    EXPORT AxSceneTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/$<CONFIG>
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export the targets to a script
install(EXPORT AxSceneTargets
    FILE AxSceneTargets.cmake
    NAMESPACE Virspace::
    DESTINATION ${INSTALL_CONFIGDIR}
)

# Generate a config file that includes the exports
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/cmake/AxSceneConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxSceneConfig.cmake
    INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
    NO_SET_AND_CHECK_MACRO
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

# Generate the version file for the config file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxSceneConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install the Config, ConfigVersion and custom Find Module
install(FILES
    #${CMAKE_CURRENT_LIST_DIR}/FindAxScene.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxSceneConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxSceneConfigVersion.cmake
    DESTINATION ${INSTALL_CONFIGDIR}
)

# Make this project available from the build directory
export(EXPORT AxSceneTargets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/AxSceneTargets.cmake
    NAMESPACE Virspace::
)

#
# Testing
#

# Add tests subdirectory
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()