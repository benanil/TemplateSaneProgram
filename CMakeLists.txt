cmake_minimum_required(VERSION 3.12)
project(SaneProgram)

# Set the C++ standard to C++14
set(CMAKE_CXX_STANDARD 14)

set(CMAKE_WIN32_EXECUTABLE ON)

# Specify the custom build directory
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)

# Add your source files here
set(SOURCES
    ASTL/Additional/GLTFParser.cpp
    SaneProgram.cpp
    Renderer.cpp
    Window.cpp
)

include_directories(${CMAKE_SOURCE_DIR})

# Add an executable target
add_executable(SaneProgram ${SOURCES})

# Set the resource file for Visual Studio
set_target_properties(SaneProgram PROPERTIES
    VS_RESOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/SaneProgram.rc"
)

target_link_libraries(SaneProgram 
    user32
    gdi32
    opengl32
)

file(COPY ${CMAKE_SOURCE_DIR}/Meshes DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/Textures DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/Shaders DESTINATION ${CMAKE_BINARY_DIR})

file(COPY ${CMAKE_SOURCE_DIR}/SaneProgram.res DESTINATION ${CMAKE_BINARY_DIR})