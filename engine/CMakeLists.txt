cmake_minimum_required(VERSION 3.15)
project(AxEngine VERSION 1.0.0 LANGUAGES C CXX)

# Common engine source files
set(AXENGINE_SOURCES
    include/AxEngine/AxEngine.h
    include/AxEngine/AxInput.h
    include/AxEngine/AxNode.h
    include/AxEngine/AxTypedNodes.h
    include/AxEngine/AxEventBus.h
    include/AxEngine/AxSceneTree.h
    include/AxEngine/AxSceneParser.h
    include/AxEngine/AxRenderer.h
    include/AxEngine/AxPrimitives.h
    include/AxEngine/AxMathTypes.h
    include/AxEngine/AxTransformType.h
    include/AxEngine/AxScriptRegistry.h
    include/AxEngine/AxSignal.h
    include/AxEngine/AxScriptLog.h
    src/AxTransformType.cpp
    src/AxScriptRegistry.cpp
    src/AxSceneParser.cpp
    src/AxScriptLog.cpp
    src/AxEngine.cpp
    src/AxInput.cpp
    src/AxNode.cpp
    src/AxTypedNodes.cpp
    src/AxSceneTree.cpp
    src/AxRenderer.cpp
    src/AxEventBus.cpp
    src/AxPrimitives.cpp
)

# Include directories shared by all engine targets
include_directories("$<JOIN:$<TARGET_PROPERTY:AxWindow,INCLUDE_DIRECTORIES>,;>")
include_directories("$<JOIN:$<TARGET_PROPERTY:AxOpenGL,INCLUDE_DIRECTORIES>,;>")
include_directories("$<JOIN:$<TARGET_PROPERTY:AxResource,INCLUDE_DIRECTORIES>,;>")

if(AX_BUILD_CONFIG STREQUAL "Shipping")
    #
    # Shipping: Single monolithic AxonEngine executable
    #

    add_executable(AxonEngine)

    # Game sources compiled directly into the monolithic executable
    set(GAME_SOURCES
        ${CMAKE_SOURCE_DIR}/examples/graphics/src/Game.cpp
    )

    target_sources(AxonEngine
        PRIVATE
            ${AXENGINE_SOURCES}
            ${GAME_SOURCES}
            src/AxStandalone.cpp
    )

    target_compile_features(AxonEngine PRIVATE cxx_std_23)
    target_compile_definitions(AxonEngine
        PRIVATE
            AXON_LINKS_FOUNDATION
    )

    target_include_directories(AxonEngine
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include/AxEngine
            ${CMAKE_CURRENT_SOURCE_DIR}/../plugins/AxResource/src
    )

    target_link_libraries(AxonEngine
        PRIVATE
            Virspace::Foundation
            Virspace::AxLog
            Virspace::AxWindow
            Virspace::AxOpenGL
            Virspace::AxResource
            Virspace::AxAudio
            axsdk_stb
            cgltf
            opengl32
            shell32
    )

    set_target_properties(AxonEngine PROPERTIES
        OUTPUT_NAME "AxonEngine"
        WIN32_EXECUTABLE TRUE
    )

else()
    #
    # Debug / Development: AxEngine STATIC library (DEC-016)
    #
    # All consumers (game host, editor, tests) link this directly.
    # No DLL exports, no AxStandalone bootstrap, no AxEngineAPI indirection.
    #

    add_library(AxEngine STATIC)
    add_library(Virspace::AxEngine ALIAS AxEngine)

    target_sources(AxEngine
        PRIVATE
            ${AXENGINE_SOURCES}
    )

    target_compile_features(AxEngine PRIVATE cxx_std_23)

    target_include_directories(AxEngine
        PUBLIC
            $<INSTALL_INTERFACE:include/AxEngine>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include/AxEngine
            ${CMAKE_CURRENT_SOURCE_DIR}/../plugins/AxResource/src
    )

    # PUBLIC so consumers inherit Foundation, AxLog, stb, cgltf
    target_link_libraries(AxEngine
        PUBLIC
            Virspace::Foundation
            Virspace::AxLog
            axsdk_stb
            cgltf
    )

    # Install/export is not needed for the static library in Development/Debug.
    # Consumers use the Virspace::AxEngine ALIAS target via add_subdirectory.
    # Shipping builds compile engine sources directly into the monolithic exe.

endif()

#
# Engine Tests
#

if(NOT AX_BUILD_CONFIG STREQUAL "Shipping")
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt)
        add_subdirectory(tests)
    endif()
endif()
