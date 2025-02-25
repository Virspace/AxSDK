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

if (WIN32)
# Add sources to target
target_sources(Foundation
    PRIVATE
        include/Foundation/AxAPIRegistry.h
        include/Foundation/AxAllocUtils.h
        include/Foundation/AxAllocationData.h
        include/Foundation/AxAllocatorInfo.h
        include/Foundation/AxAllocatorRegistry.h
        include/Foundation/AxApplication.h
        include/Foundation/AxArray.h
        include/Foundation/AxCamera.h
        include/Foundation/AxHeap.h
        #include/Foundation/AxStackAllocator.h
        include/Foundation/AxEditorPlugin.h
        include/Foundation/AxHash.h
        include/Foundation/AxHashTable.h
        #include/Foundation/AxImageLoader.h
        include/Foundation/AxIntrinsics.h
        include/Foundation/AxMath.h
        include/Foundation/AxPlatform.h
        include/Foundation/AxPlugin.h
        include/Foundation/AxTypes.h
        include/Foundation/AxLinkedList.h
        src/AxAPIRegistry.c
        src/AxAllocatorData.h
        src/AxAllocatorInfo.c
        src/AxAllocatorRegistry.c
        src/AxAllocUtils.c
        src/AxCamera.c
        src/AxLinearAllocatorWin32.c
        src/AxHeapWin32.c
        #src/AxImageLoader.c
        src/AxHash.c
        src/AxHashTable.c
        #src/AxStackAllocatorWin32.c
        src/AxIntrinsics.c
        src/AxMath.c
        src/AxPlugin.c
        src/AxWin32Platform.c
        src/AxLinkedList.c
)
else()
    target_sources(Foundation
        PRIVATE
            include/Foundation/AxAPIRegistry.h
            include/Foundation/AxAllocUtils.h
            include/Foundation/AxAllocatorInfo.h
            include/Foundation/AxAllocatorRegistry.h
            include/Foundation/AxApplication.h
            include/Foundation/AxArray.h
            include/Foundation/AxCamera.h
            #include/Foundation/AxHeap.h
            include/Foundation/AxLinearAllocator.h
            #include/Foundation/AxStackAllocator.h
            include/Foundation/AxEditorPlugin.h
            include/Foundation/AxHash.h
            include/Foundation/AxHashTable.h
            #include/Foundation/AxImageLoader.h
            include/Foundation/AxIntrinsics.h
            include/Foundation/AxMath.h
            include/Foundation/AxPlatform.h
            include/Foundation/AxPlugin.h
            include/Foundation/AxTypes.h
            include/Foundation/AxLinkedList.h
            src/AxAPIRegistry.c
            src/AxAllocatorData.h
            src/AxAllocatorInfo.c
            src/AxAllocatorRegistry.c
            src/AxAllocUtils.c
            src/AxCamera.c
            #src/AxHeapLinux.c
            src/AxIntrinsics.c
            src/AxLinearAllocatorLinux.c
            #src/AxImageLoader.c
            src/AxHash.c
            src/AxHashTable.c
            #src/AxStackAllocatorWin32.c
            src/AxMath.c
            src/AxPlugin.c
            src/AxLinuxPlatform.c
            src/AxLinkedList.c
    )
endif()

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
        ${WINDOWS_SDK_INCLUDE_DIR}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/Foundation
)

target_link_libraries(Foundation Shell32 User32)

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
