cmake_minimum_required(VERSION 3.15)
project(AxEngine VERSION 1.0.0 LANGUAGES C CXX)

#
# Create Target
#

# Add the library as a shared library (plugin)
add_library(AxEngine SHARED)
add_library(Virspace::AxEngine ALIAS AxEngine)

# Add sources to target
target_sources(AxEngine
    PRIVATE
        include/AxEngine/AxEngine.h
        include/AxEngine/AxInput.h
        include/AxEngine/AxNode.h
        include/AxEngine/AxComponent.h
        include/AxEngine/AxComponentFactory.h
        include/AxEngine/AxSystem.h
        include/AxEngine/AxSystemFactory.h
        include/AxEngine/AxEventBus.h
        include/AxEngine/AxScene.h
        include/AxEngine/AxRenderer.h
        include/AxEngine/AxSceneManager.h
        include/AxEngine/AxScript.h
        include/AxEngine/AxScripting.h
        src/AxEngine.cpp
        src/AxInput.cpp
        src/AxNode.cpp
        src/AxComponentFactory.cpp
        src/AxSystemFactory.cpp
        src/AxScene.cpp
        src/AxRenderer.cpp
        src/AxSceneManager.cpp
        src/AxEventBus.cpp
        src/AxScript.cpp
        src/AxScripting.cpp
)

# Add the standalone bootstrap executable
add_executable(AxStandalone)

# Add source to executable target
target_sources(AxStandalone
    PRIVATE
        src/AxStandalone.cpp
)


#
# Fetch dependencies
#

include(FetchContent)

# STB for image loading
FetchContent_Declare(
    axsdk_stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
)

FetchContent_GetProperties(axsdk_stb)
if(NOT axsdk_stb_POPULATED)
    FetchContent_Populate(axsdk_stb)
    message("Fetching stb for AxEngine")

    add_library(axsdk_stb INTERFACE)
    target_include_directories(axsdk_stb INTERFACE ${axsdk_stb_SOURCE_DIR})
endif()

# CGLTF for GLTF model loading
FetchContent_Declare(
    cgltf
    GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
    GIT_TAG master
)

FetchContent_GetProperties(cgltf)
if(NOT cgltf_POPULATED)
    FetchContent_Populate(cgltf)
    message("Fetching cgltf for AxEngine")

    add_library(cgltf INTERFACE ${cgltf_SOURCE_DIR}/cgltf.h)
    target_include_directories(cgltf INTERFACE ${cgltf_SOURCE_DIR})
endif()

#
# Usage Requirements
#

# Add compiler features and definitions
target_compile_features(AxEngine PRIVATE cxx_std_23)
target_compile_definitions(AxEngine
    PRIVATE
        AXENGINE_EXPORTS
)

# Add compiler features and definitions for AxStandalone
target_compile_features(AxStandalone PRIVATE cxx_std_23)
target_compile_definitions(AxStandalone
    PRIVATE
        AXON_LINKS_FOUNDATION
)

include_directories("$<JOIN:$<TARGET_PROPERTY:AxWindow,INCLUDE_DIRECTORIES>,;>")
include_directories("$<JOIN:$<TARGET_PROPERTY:AxOpenGL,INCLUDE_DIRECTORIES>,;>")
include_directories("$<JOIN:$<TARGET_PROPERTY:AxResource,INCLUDE_DIRECTORIES>,;>")

# Specify include directories
target_include_directories(AxEngine
    PUBLIC
        $<INSTALL_INTERFACE:include/AxEngine>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/AxEngine
)

target_link_libraries(AxEngine
    PRIVATE
        Virspace::Foundation
        axsdk_stb
        cgltf
        -Wl,--export-all-symbols
)

# Link AxStandalone with Foundation
target_link_libraries(AxStandalone
    PRIVATE
        Virspace::Foundation
        shell32
)

# Add include directories for AxStandalone
target_include_directories(AxStandalone
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

#
# Engine Tests
#

# Add engine tests subdirectory
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt)
    add_subdirectory(tests)
endif()

#
# Install and Export
#

# Set the install destination for cmake files
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/AxEngine)

# Install include directory
include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Specify the name of the export set the targets belong to
install(TARGETS AxEngine
    EXPORT AxEngineTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/$<CONFIG>
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export the targets to a script
install(EXPORT AxEngineTargets
    FILE AxEngineTargets.cmake
    NAMESPACE Virspace::
    DESTINATION ${INSTALL_CONFIGDIR}
)

# Generate a config file that includes the exports
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/cmake/AxEngineConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxEngineConfig.cmake
    INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
    NO_SET_AND_CHECK_MACRO
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

# Generate the version file for the config file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxEngineConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install the Config, ConfigVersion and custom Find Module
install(FILES
    #${CMAKE_CURRENT_LIST_DIR}/FindAxEngine.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxEngineConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxEngineConfigVersion.cmake
    DESTINATION ${INSTALL_CONFIGDIR}
)

# Make this project available from the build directory
export(EXPORT AxEngineTargets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/AxEngineTargets.cmake
    NAMESPACE Virspace::
)
