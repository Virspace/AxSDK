# Preamble
cmake_minimum_required(VERSION 3.15)
project(AxSDK VERSION 1.0.0)
include(CTest)

# Project-wide Setup

# Enable folders in IDE Solution
if (CMAKE_GENERATOR MATCHES "Visual Studio")
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

add_definitions(-DAX_OS_WINDOWS)

# TODO(mdeforge): Maybe the Modern CMake approach is to do this per target in their CMakeList files.
if (MSVC)
    add_compile_options(
        /nologo      # Suppresses the display of the copyright banner when the compiler starts up and display of informational messages during compiling.
        /fp:fast     # Allows the compiler to reorder, combine, or simplify floating-point operations to optimize floating-point code for speed and space.
        /fp:except-  # DISABLES code generation that to ensures that any unmasked floating-point exceptions are raised at the exact point at which they occur.
        #/Gm-        # DISABLES minimam rebuild. Technically depricated.
        #/GR-        # DISABLES Adding code to check object types at run time.
        #/EHa-       # DISABLES standard C++ stack unwinding
        #/Od         # DISABLES all optimizations in the program and speeds compilation.
        #/Oi         # DISABLES the replacement of some function calls with intrinsic or otherwise special forms of the function that help your application run faster.
        /WL          # Appends additional information to an error or warning message.
        #/WX         # Treats all compiler warnings as errors. 
        /W4          # Displays level 1, level 2, and level 3 warnings, and all level 4 (informational) warnings that aren't off by default.
        /wd4201      # nonstandard extension used : nameless struct/union.
        /wd4100      # 'identifier' : unreferenced formal parameter.
        /wd4189      # A variable is declared and initialized but not used.
        /wd4505      # 'function' : unreferenced local function has been removed.
        /wd4127      # conditional expression is constant.
        /wd4172      # returning address of local variable or temporary.
        /wd5105      # macro expansion producing 'defined' has undefined behavior.
        /FC          # Causes the compiler to display the full path of source code files passed to the compiler in diagnostics.
        /Z7          # Produces object files that also contain full symbolic debugging information for use with the debugger.
        #/GS-        # Disables Buffer Security Check (Security risk!)
        /Gs9999999   # Controls the threshold for stack probes -- the number of bytes that local variables can occupy before a stack probe is initiated.
    )

    add_link_options(/incremental:no /opt:ref user32.lib gdi32.lib winmm.lib kernel32.lib)
else()
    # add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

# Basic directory setup
#set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/Install)
#set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/Bin)
#set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/Bin)

add_subdirectory(foundation)
set_target_properties(Foundation PROPERTIES FOLDER Foundation)

add_subdirectory(plugins)
add_subdirectory(internal)