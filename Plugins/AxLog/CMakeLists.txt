cmake_minimum_required(VERSION 3.15)
project(AxLog VERSION 1.0.0 LANGUAGES CXX C)

#
# Create Target
#

# Add the library
add_library(AxLog SHARED)
add_library(Virspace::AxLog ALIAS AxLog)

# Add sources to target
target_sources(AxLog
    PRIVATE
        include/AxLog/AxLog.h
        src/AxLog.cpp
)

#
# Usage Requirements
#

# Add compiler features and definitions
target_compile_features(AxLog PRIVATE cxx_std_23)
target_compile_definitions(AxLog
    PRIVATE
        AXON_LINKS_FOUNDATION
        AXLOG_EXPORTS
)

# Optional compile-time log level override for consuming targets.
# Usage: cmake -DAX_LOG_COMPILE_LEVEL_OVERRIDE=AX_LOG_LEVEL_INFO
# This propagates to all targets that link AxLog.
if(DEFINED AX_LOG_COMPILE_LEVEL_OVERRIDE)
    target_compile_definitions(AxLog
        PUBLIC
            AX_LOG_COMPILE_LEVEL=${AX_LOG_COMPILE_LEVEL_OVERRIDE}
    )
endif()

# Specify include directories to use when compiling a given target
target_include_directories(AxLog
    PUBLIC
        $<INSTALL_INTERFACE:include/AxLog>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/AxLog
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link dependencies
target_link_libraries(AxLog
    PUBLIC
        Virspace::Foundation
)

#
# Install and Export
#

# Set the install destination for cmake files
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/AxLog)

# Install include directory
include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Specify the name of the export set the targets belong to
install(TARGETS AxLog
    EXPORT AxLogTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/$<CONFIG>
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export the targets to a script
install(EXPORT AxLogTargets
    FILE AxLogTargets.cmake
    NAMESPACE Virspace::
    DESTINATION ${INSTALL_CONFIGDIR}
)

# Generate a config file that includes the exports
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/cmake/AxLogConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxLogConfig.cmake
    INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
    NO_SET_AND_CHECK_MACRO
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

# Generate the version file for the config file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxLogConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install the Config, ConfigVersion and custom Find Module
install(FILES
    #${CMAKE_CURRENT_LIST_DIR}/FindAxLog.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxLogConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/AxLogConfigVersion.cmake
    DESTINATION ${INSTALL_CONFIGDIR}
)

# Make this project available from the build directory
export(EXPORT AxLogTargets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/AxLogTargets.cmake
    NAMESPACE Virspace::
)

#
# Testing
#

# Add tests subdirectory
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
