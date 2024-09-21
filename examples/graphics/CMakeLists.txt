# Preamble
cmake_minimum_required(VERSION 3.15)
project(OpenGLExample VERSION 1.0.0 LANGUAGES C CXX)

find_package(Vulkan REQUIRED)

# Enable folders in IDE Solution
if (CMAKE_GENERATOR MATCHES "Visual Studio")
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

#
# Create Target
#

# Add the library
add_library(OpenGLExample SHARED)
add_library(Virspace::OpenGLExample ALIAS OpenGLExample)

# Add sources to library target
target_sources(OpenGLExample
    PRIVATE
        src/OpenGLExample.cpp
)

# Add the executable
add_executable(Host)

# Add source to executable target
target_sources(Host
    PRIVATE
        src/Host.cpp
)

#
# Usage Requirements
#

# TODO(mdeforge): Detect platform
add_definitions(-DAX_OS_WINDOWS)

# Set compile and link options
set(OpenGLExample_COMPILE_OPTIONS
    /nologo     # Suppresses the display of the copyright banner when the compiler starts up and display of informational messages during compiling.
    /fp:fast    # Allows the compiler to reorder, combine, or simplify floating-point operations to optimize floating-point code for speed and space.
    /fp:except- # DISABLES code generation that to ensures that any unmasked floating-point exceptions are raised at the exact point at which they occur.
    #/Gm-        # DISABLES minimam rebuild. Technically depricated.
    #/GR-        # DISABLES Adding code to check object types at run time.
    #/EHa-       # DISABLES standard C++ stack unwinding
    #/Od         # DISABLES all optimizations in the program and speeds compilation.
    #/Oi         # DISABLES the replacement of some function calls with intrinsic or otherwise special forms of the function that help your application run faster.
    /WL         # Appends additional information to an error or warning message.
    #/WX         # Treats all compiler warnings as errors. 
    /W4         # Displays level 1, level 2, and level 3 warnings, and all level 4 (informational) warnings that aren't off by default.
    /wd4201     # nonstandard extension used : nameless struct/union.
    /wd4100     # 'identifier' : unreferenced formal parameter.
    /wd4189     # A variable is declared and initialized but not used.
    /wd4505     # 'function' : unreferenced local function has been removed.
    /wd4127     # conditional expression is constant.
    /wd4172     # returning address of local variable or temporary.
    /wd5105     # macro expansion producing 'defined' has undefined behavior.
    /FC         # Causes the compiler to display the full path of source code files passed to the compiler in diagnostics.
    /Z7         # Produces object files that also contain full symbolic debugging information for use with the debugger.
    #/GS-        # Disables Buffer Security Check (Security risk!)
    /Gs9999999  # Controls the threshold for stack probes -- the number of bytes that local variables can occupy before a stack probe is initiated.
)

set(OpenGLExample_LINK_OPTIONS /incremental:no /opt:ref user32.lib gdi32.lib winmm.lib kernel32.lib)

if(MSVC)
    target_compile_options(OpenGLExample PRIVATE ${OpenGLExample_COMPILE_OPTIONS})
    target_link_options(OpenGLExample PRIVATE ${OpenGLExample_LINK_OPTIONS})

    target_compile_options(OpenGLExample PRIVATE ${OpenGLExample_COMPILE_OPTIONS})
    target_link_options(OpenGLExample PRIVATE ${OpenGLExample_LINK_OPTIONS})
else()
    # add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

# Add compiler features and definitions
target_compile_features(Host PRIVATE cxx_std_20)
target_compile_definitions(Host
    PRIVATE
        AXON_LINKS_FOUNDATION
)

target_compile_features(OpenGLExample PRIVATE cxx_std_20)
target_compile_definitions(OpenGLExample
    PRIVATE
        AXON_LINKS_FOUNDATION
)

include_directories("$<JOIN:$<TARGET_PROPERTY:AxWindow,INCLUDE_DIRECTORIES>,;>")
include_directories("$<JOIN:$<TARGET_PROPERTY:AxOpenGL,INCLUDE_DIRECTORIES>,;>")

# Specify include directories to use when compiling a given target
target_include_directories(OpenGLExample
    PUBLIC
        $<INSTALL_INTERFACE:include/OpenGLExample> # Used for working with an 'install tree'
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> # Used for working with a 'build tree'
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/OpenGLExample
)

# Link dependencies
target_link_libraries(OpenGLExample
    PRIVATE
        Virspace::Foundation
)

target_link_libraries(Host
    PRIVATE
        Virspace::Foundation
        shell32
)

# target_link_options(Host
#     PRIVATE
#         -mwindows
# )

#
# Install and Export
#

# Set the install destination for cmake files
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/OpenGLExample)

# Specify the name of the export set the targets belong to
install(TARGETS OpenGLExample
    EXPORT OpenGLExampleTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/$<CONFIG>
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Copy shaders to build directory during build
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(DESTINATION_DIR "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/shaders")
message(status ${SOURCE_DIR})
message(status ${DESTINATION_DIR})
add_custom_command(
    TARGET OpenGLExample  # Target executable to associate the copy command with
    POST_BUILD  # Specify that this should happen after building the executable
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${SOURCE_DIR} ${DESTINATION_DIR}
    COMMENT "Copying shader folder to the build folder"
)

# TODO(mdeforge): See if we can precompile the shaders
function(add_shaders SHADER_DIR)
    #set(SHADER_SOURCE_FILES ${ARGN}) # the rest of arguments to this function will be assigned as shader source files
    # Get all the shaders in the shader directory
    file(GLOB SHADERS "${SHADER_DIR}/*")

    # Validate that source files have been passed
    list(LENGTH SHADERS FILE_COUNT)
    if(FILE_COUNT EQUAL 0)
        message(FATAL_ERROR "Cannot create a shaders target without any source files")
    endif()

    set(SHADER_COMMANDS)
    set(SHADER_PRODUCTS)

    foreach(SHADER_SOURCE IN LISTS SHADERS)
        cmake_path(ABSOLUTE_PATH SHADER_SOURCE NORMALIZE)
        cmake_path(GET SHADER_SOURCE FILENAME SHADER_NAME)

        # Build command
        list(APPEND SHADER_COMMANDS COMMAND)
        list(APPEND SHADER_COMMANDS Vulkan::glslc)
        list(APPEND SHADER_COMMANDS "${SHADER_SOURCE}")
        list(APPEND SHADER_COMMANDS "-o")
        list(APPEND SHADER_COMMANDS "${CMAKE_CURRENT_BINARY_DIR}/${SHADER_NAME}.spv")

        # Add product
        list(APPEND SHADER_PRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/${SHADER_NAME}.spv")

    endforeach()

    add_custom_target(${CMAKE_PROJECT_NAME} ALL
        ${SHADER_COMMANDS}
        COMMENT "Compiling Shaders [${CMAKE_PROJECT_NAME}]"
        SOURCES ${SHADERS}
        BYPRODUCTS ${SHADER_PRODUCTS}
    )
endfunction()

#add_shaders(${PROJECT_SOURCE_DIR}/shaders)