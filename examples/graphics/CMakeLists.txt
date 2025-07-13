# Preamble
cmake_minimum_required(VERSION 3.15)
project(OpenGLGame VERSION 1.0.0 LANGUAGES C CXX)

# Enable folders in IDE Solution
if (CMAKE_GENERATOR MATCHES "Visual Studio")
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

# Get cgltf
FetchContent_Declare(
    cgltf
    GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
    GIT_TAG v1.14
    GIT_SHALLOW     TRUE
    GIT_PROGRESS    TRUE
)

FetchContent_GetProperties(cgltf)
if(NOT cgltf_POPULATED)
    FetchContent_Populate(cgltf)
    message("Fetching cgltf")

    add_library(cgltf INTERFACE ${cgltf_SOURCE_DIR}/cgltf.h)
    target_include_directories(cgltf INTERFACE ${cgltf_SOURCE_DIR})
endif()

# Get stb
FetchContent_Declare(
    axsdk_stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
    GIT_SHALLOW     TRUE
    GIT_PROGRESS    TRUE
)

FetchContent_GetProperties(axsdk_stb)
if(NOT axsdk_stb_POPULATED)
    FetchContent_Populate(axsdk_stb)
    message("Fetching stb")

    add_library(axsdk_stb INTERFACE)
    target_include_directories(axsdk_stb INTERFACE ${axsdk_stb_SOURCE_DIR})
endif()

#
# Create Target
#

# Add the library
add_library(OpenGLGame SHARED)
add_library(Virspace::OpenGLGame ALIAS OpenGLGame)

# Add sources to library target
target_sources(OpenGLGame
    PRIVATE
        src/OpenGLGame.cpp
)

# Add the executable
add_executable(OpenGLExample)

# Add source to executable target
target_sources(OpenGLExample
    PRIVATE
        src/Host.cpp
)

#
# Usage Requirements
#

# TODO(mdeforge): Detect platform
add_definitions(-DAX_OS_WINDOWS)

# Set compile and link options
set(OpenGLGame_COMPILE_OPTIONS
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

set(OpenGLGame_LINK_OPTIONS /incremental:no /opt:ref user32.lib gdi32.lib winmm.lib kernel32.lib)

if(MSVC)
    target_compile_options(OpenGLGame PRIVATE ${OpenGLGame_COMPILE_OPTIONS})
    target_link_options(OpenGLGame PRIVATE ${OpenGLGame_LINK_OPTIONS})

    target_compile_options(OpenGLGame PRIVATE ${OpenGLGame_COMPILE_OPTIONS})
    target_link_options(OpenGLGame PRIVATE ${OpenGLGame_LINK_OPTIONS})
else()
    # add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

# Add compiler features and definitions
target_compile_features(OpenGLExample PRIVATE cxx_std_20)
target_compile_definitions(OpenGLExample
    PRIVATE
        AXON_LINKS_FOUNDATION
)

target_compile_features(OpenGLGame PRIVATE cxx_std_20)
target_compile_definitions(OpenGLGame
    PRIVATE
        AXON_LINKS_FOUNDATION
        _CRT_SECURE_NO_WARNINGS
)

include_directories("$<JOIN:$<TARGET_PROPERTY:AxWindow,INCLUDE_DIRECTORIES>,;>")
include_directories("$<JOIN:$<TARGET_PROPERTY:AxOpenGL,INCLUDE_DIRECTORIES>,;>")

# Specify include directories to use when compiling a given target
target_include_directories(OpenGLGame
    PUBLIC
        $<INSTALL_INTERFACE:include/OpenGLGame> # Used for working with an 'install tree'
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> # Used for working with a 'build tree'
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/OpenGLGame
)

# Link dependencies
target_link_libraries(OpenGLGame
    PRIVATE
        Virspace::Foundation
        cgltf
        axsdk_stb
)

target_link_libraries(OpenGLExample
    PRIVATE
        Virspace::Foundation
        shell32
)

#
# Install and Export
#

# Set the install destination for cmake files
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/OpenGLGame)

# Specify the name of the export set the targets belong to
install(TARGETS OpenGLGame
    EXPORT OpenGLGameTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/$<CONFIG>
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Copy shaders to build directory during build
set(SHADERS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SHADERS_DESTINATION_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders")
add_custom_command(
    TARGET OpenGLGame  # Target executable to associate the copy command with
    POST_BUILD  # Specify that this should happen after building the executable
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${SHADERS_SOURCE_DIR} ${SHADERS_DESTINATION_DIR}
    COMMENT "Copying shader folder to the build folder"
)

# Copy model to build directory during guild
set(MODEL_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/models")
set(MODEL_DESTINATION_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/models")
add_custom_command(
    TARGET OpenGLGame  # Target executable to associate the copy command with
    POST_BUILD  # Specify that this should happen after building the executable
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${MODEL_SOURCE_DIR} ${MODEL_DESTINATION_DIR}
    COMMENT "Copying models folder to the build folder"
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
        list(APPEND SHADER_COMMANDS "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${SHADER_NAME}.spv")

        # Add product
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