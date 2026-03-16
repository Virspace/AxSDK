# Preamble
cmake_minimum_required(VERSION 3.15)
project(OpenGLGame VERSION 1.0.0 LANGUAGES C CXX)

# Create symlink to scenes folder in build directory (needed in all build modes)
set(SCENES_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/scenes")
set(SCENES_DESTINATION_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/examples/graphics/scenes")
add_custom_target(link_scenes ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/examples/graphics"
    COMMAND ${CMAKE_COMMAND} -E remove ${SCENES_DESTINATION_DIR}
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${SCENES_SOURCE_DIR} ${SCENES_DESTINATION_DIR}
    COMMENT "Creating symlink to scenes folder"
)

# In shipping mode, game sources are compiled directly into AxonEngine
if(AX_BUILD_CONFIG STREQUAL "Shipping")
    add_dependencies(AxonEngine link_scenes)
    return()
endif()

#=============================================================================
# OpenGLGameHost -- standalone executable that links AxEngine statically
#=============================================================================

add_executable(OpenGLGameHost)

target_sources(OpenGLGameHost
    PRIVATE
        src/main.cpp
)

target_compile_features(OpenGLGameHost PRIVATE cxx_std_23)

# Plugin include directories needed by engine headers
target_include_directories(OpenGLGameHost
    PRIVATE
        ${CMAKE_SOURCE_DIR}/plugins/AxOpenGL/include
        ${CMAKE_SOURCE_DIR}/plugins/AxResource/include
        ${CMAKE_SOURCE_DIR}/plugins/AxResource/src
        ${CMAKE_SOURCE_DIR}/plugins/AxWindow/include
)

target_compile_definitions(OpenGLGameHost
    PRIVATE
        AXON_LINKS_FOUNDATION
)

# Link AxEngine static library (brings Foundation, AxLog, stb, cgltf transitively)
target_link_libraries(OpenGLGameHost
    PRIVATE
        Virspace::AxEngine
)

# Export all symbols so Game.dll can resolve engine symbols from the host exe.
# This is the same pattern that was previously on AxEngine.dll (DEC-016).
target_link_options(OpenGLGameHost
    PRIVATE
        -Wl,--export-all-symbols
)

set_target_properties(OpenGLGameHost PROPERTIES
    OUTPUT_NAME "OpenGLGameHost"
    ENABLE_EXPORTS TRUE
)

add_dependencies(OpenGLGameHost link_scenes)

#=============================================================================
# OpenGLGame -- Game.dll (script DLL, hot-reloaded at runtime)
#=============================================================================

add_library(OpenGLGame SHARED)
add_library(Virspace::OpenGLGame ALIAS OpenGLGame)

# Output as libGame.dll (the engine expects this name)
set_target_properties(OpenGLGame PROPERTIES OUTPUT_NAME "Game")

target_sources(OpenGLGame
    PRIVATE
        src/Game.cpp
)

target_compile_features(OpenGLGame PRIVATE cxx_std_23)

# Include directories for engine and plugin headers used by Game.cpp.
# These are listed explicitly because Game.dll links the host exe (not AxEngine
# directly), so AxEngine's PUBLIC includes don't propagate.
target_include_directories(OpenGLGame
    PRIVATE
        ${CMAKE_SOURCE_DIR}/engine/include
        ${CMAKE_SOURCE_DIR}/foundation/include
        ${CMAKE_SOURCE_DIR}/plugins/AxLog/include
        ${CMAKE_SOURCE_DIR}/plugins/AxOpenGL/include
        ${CMAKE_SOURCE_DIR}/plugins/AxResource/include
        ${CMAKE_SOURCE_DIR}/plugins/AxResource/src
        ${CMAKE_SOURCE_DIR}/plugins/AxWindow/include
)

# Game.dll links against the host exe's import library to resolve engine symbols.
# This ensures Game.dll uses the SAME ScriptRegistry::Get() instance as the host,
# rather than getting its own copy from the static AxEngine lib.
target_link_libraries(OpenGLGame
    PRIVATE
        OpenGLGameHost
)

add_dependencies(OpenGLGame link_scenes)

# TODO(mdeforge): See if we can precompile the shaders
function(add_shaders SHADER_DIR)
    file(GLOB SHADERS "${SHADER_DIR}/*")

    list(LENGTH SHADERS FILE_COUNT)
    if(FILE_COUNT EQUAL 0)
        message(FATAL_ERROR "Cannot create a shaders target without any source files")
    endif()

    set(SHADER_COMMANDS)
    set(SHADER_PRODUCTS)

    foreach(SHADER_SOURCE IN LISTS SHADERS)
        cmake_path(ABSOLUTE_PATH SHADER_SOURCE NORMALIZE)
        cmake_path(GET SHADER_SOURCE FILENAME SHADER_NAME)

        list(APPEND SHADER_COMMANDS COMMAND)
        list(APPEND SHADER_COMMANDS Vulkan::glslc)
        list(APPEND SHADER_COMMANDS "${SHADER_SOURCE}")
        list(APPEND SHADER_COMMANDS "-o")
        list(APPEND SHADER_COMMANDS "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${SHADER_NAME}.spv")

        list(APPEND SHADER_PRODUCTS "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${SHADER_NAME}.spv")

    endforeach()

    add_custom_target(${CMAKE_PROJECT_NAME} ALL
        ${SHADER_COMMANDS}
        COMMENT "Compiling Shaders [${CMAKE_PROJECT_NAME}]"
        SOURCES ${SHADERS}
        BYPRODUCTS ${SHADER_PRODUCTS}
    )
endfunction()

#add_shaders(${PROJECT_SOURCE_DIR}/shaders)
