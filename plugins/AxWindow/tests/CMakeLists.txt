# AxWindow Tests CMakeLists.txt

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create the test executable
add_executable(AxWindowTests AxWindowTests.cpp)

# Set include directories
target_include_directories(AxWindowTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../foundation/include
)

# Link against the AxWindow library if needed
# target_link_libraries(AxWindowTests AxWindow)

# Set output directory
set_target_properties(AxWindowTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
) 