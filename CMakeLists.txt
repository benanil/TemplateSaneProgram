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
    ASTL/Additional/OBJParser.cpp
    
    External/ufbx.c
    External/zstd.c # compress files. by facebook (meta)
    
    External/ProcessDxtc.cpp # compress dxt textures
    
    src/AssetManager.cpp
    src/SaneProgram.cpp
    src/Renderer.cpp
    src/PlatformWindows.cpp
    src/Scene.cpp
)

# Add all .cpp files from External/astc-encoder to the SOURCES list
file(GLOB ASTC_ENCODER_SOURCES External/astc-encoder/*.cpp)

# Append the ASTC_ENCODER_SOURCES to the SOURCES list
list(APPEND SOURCES ${ASTC_ENCODER_SOURCES})

include_directories(${CMAKE_SOURCE_DIR})

# Add an executable target
add_executable(SaneProgram ${SOURCES})

# Set the resource file for Visual Studio
set_target_properties(SaneProgram PROPERTIES
    VS_RESOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/SaneProgram.rc"
)

set_property(TARGET SaneProgram PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(SaneProgram 
    user32
    gdi32
    opengl32
)

# Set the source files for ASTC encoder in Visual Studio
source_group("ASTC Encoder" FILES ${ASTC_ENCODER_SOURCES})

file(COPY ${CMAKE_SOURCE_DIR}/SaneProgram.res DESTINATION ${CMAKE_BINARY_DIR})