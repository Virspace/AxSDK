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
    # Debug / Development: AxEngine SHARED + AxStandalone executable (current behavior)
    #

    add_library(AxEngine SHARED)
    add_library(Virspace::AxEngine ALIAS AxEngine)

    target_sources(AxEngine
        PRIVATE
            ${AXENGINE_SOURCES}
    )

    add_executable(AxStandalone)

    target_sources(AxStandalone
        PRIVATE
            src/AxStandalone.cpp
    )

    # AxEngine settings
    target_compile_features(AxEngine PRIVATE cxx_std_23)
    target_compile_definitions(AxEngine
        PRIVATE
            AXENGINE_EXPORTS
    )

    target_include_directories(AxEngine
        PUBLIC
            $<INSTALL_INTERFACE:include/AxEngine>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include/AxEngine
            ${CMAKE_CURRENT_SOURCE_DIR}/../plugins/AxResource/src
    )

    target_link_libraries(AxEngine
        PRIVATE
            Virspace::Foundation
            Virspace::AxLog
            axsdk_stb
            cgltf
    )

    # Export all symbols for dynamic plugin loading (GCC/Clang only; MSVC uses __declspec)
    if(NOT MSVC)
        target_link_options(AxEngine PRIVATE -Wl,--export-all-symbols)
    endif()

    # AxStandalone settings
    target_compile_features(AxStandalone PRIVATE cxx_std_23)
    target_compile_definitions(AxStandalone
        PRIVATE
            AXON_LINKS_FOUNDATION
    )

    target_link_libraries(AxStandalone
        PRIVATE
            Virspace::Foundation
            Virspace::AxLog
            shell32
    )

    target_include_directories(AxStandalone
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    #
    # Install and Export (Development/Debug only)
    #

    set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/AxEngine)

    include(GNUInstallDirs)
    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    install(TARGETS AxEngine
        EXPORT AxEngineTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/$<CONFIG>
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(EXPORT AxEngineTargets
        FILE AxEngineTargets.cmake
        NAMESPACE Virspace::
        DESTINATION ${INSTALL_CONFIGDIR}
    )

    include(CMakePackageConfigHelpers)
    configure_package_config_file(
        ${CMAKE_CURRENT_LIST_DIR}/cmake/AxEngineConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxEngineConfig.cmake
        INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
        NO_SET_AND_CHECK_MACRO
        NO_CHECK_REQUIRED_COMPONENTS_MACRO
    )

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxEngineConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxEngineConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxEngineConfigVersion.cmake
        DESTINATION ${INSTALL_CONFIGDIR}
    )

    export(EXPORT AxEngineTargets
        FILE ${CMAKE_CURRENT_BINARY_DIR}/AxEngineTargets.cmake
        NAMESPACE Virspace::
    )

endif()

#
# Engine Tests
#

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt)
    add_subdirectory(tests)
endif()
