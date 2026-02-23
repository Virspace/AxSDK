cmake_minimum_required(VERSION 3.15)
project(AxResource VERSION 2.0.0 LANGUAGES C CXX)
include(CTest)

#
# Fetch dependencies
#

include(FetchContent)

# STB for image loading
FetchContent_Declare(
    axresource_stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
)

FetchContent_GetProperties(axresource_stb)
if(NOT axresource_stb_POPULATED)
    FetchContent_Populate(axresource_stb)
    message("Fetching stb for AxResource")

    add_library(axresource_stb INTERFACE)
    target_include_directories(axresource_stb INTERFACE ${axresource_stb_SOURCE_DIR})
endif()

# CGLTF for GLTF model loading
FetchContent_Declare(
    axresource_cgltf
    GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
    GIT_TAG master
)

FetchContent_GetProperties(axresource_cgltf)
if(NOT axresource_cgltf_POPULATED)
    FetchContent_Populate(axresource_cgltf)
    message("Fetching cgltf for AxResource")

    add_library(axresource_cgltf INTERFACE)
    target_include_directories(axresource_cgltf INTERFACE ${axresource_cgltf_SOURCE_DIR})
endif()

#
# Create Target
#

# Add the library as a shared library (plugin)
add_library(AxResource SHARED)
add_library(Virspace::AxResource ALIAS AxResource)

# Add sources to target
target_sources(AxResource
    PRIVATE
        include/AxResource/AxResource.h
        include/AxResource/AxResourceTypes.h
        src/ResourceSystem.h
        src/ResourceSystem.cpp
        src/AxResource.cpp
)

#
# Usage Requirements
#

# Add compiler features and definitions
target_compile_features(AxResource PRIVATE c_std_11 cxx_std_23)

# Get AxOpenGL include directories
include_directories("$<JOIN:$<TARGET_PROPERTY:AxOpenGL,INCLUDE_DIRECTORIES>,;>")

# Specify include directories
target_include_directories(AxResource
    PUBLIC
        $<INSTALL_INTERFACE:include/AxResource>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/AxResource
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(AxResource
    PRIVATE
        Virspace::Foundation
        Virspace::AxLog
        axresource_stb
        axresource_cgltf
)

#
# Install and Export
#

# Set the install destination for cmake files
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/AxResource)

# Install include directory
include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Specify the name of the export set the targets belongs to
install(TARGETS AxResource
    EXPORT AxResourceTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/$<CONFIG>
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export the targets to a script
install(EXPORT AxResourceTargets
    FILE AxResourceTargets.cmake
    NAMESPACE Virspace::
    DESTINATION ${INSTALL_CONFIGDIR}
)

# Generate a config file that includes the exports
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/cmake/AxResourceConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxResourceConfig.cmake
    INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
    NO_SET_AND_CHECK_MACRO
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

# Generate the version file for the config file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxResourceConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install the Config, ConfigVersion and custom Find Module
install(FILES
    #${CMAKE_CURRENT_LIST_DIR}/FindAxResource.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxResourceConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxResourceConfigVersion.cmake
    DESTINATION ${INSTALL_CONFIGDIR}
)

# Make this project available from the build directory
export(EXPORT AxResourceTargets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/AxResourceTargets.cmake
    NAMESPACE Virspace::
)

#
# Testing
#

# Add tests subdirectory
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
