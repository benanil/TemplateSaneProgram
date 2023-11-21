cmake_minimum_required(VERSION 3.12)
project(ASTL_Project)

# Set the C++ standard to C++14
set(CMAKE_CXX_STANDARD 14)

set(CMAKE_WIN32_EXECUTABLE ON)

# Specify the custom build directory
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)

# Add your source files here
set(SOURCES
    GLTFParser.cpp
    Test/ASTL.cpp
    Profiler.cpp
    Renderer.cpp
    Window.cpp
)

include_directories(${CMAKE_SOURCE_DIR})

# Add an executable target
add_executable(astl_test ${SOURCES})

target_link_libraries(astl_test 
    user32
    gdi32
    opengl32
)

file(COPY ${CMAKE_SOURCE_DIR}/Meshes DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/Textures DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/Shaders DESTINATION ${CMAKE_BINARY_DIR})
