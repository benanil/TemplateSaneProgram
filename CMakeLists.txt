cmake_minimum_required(VERSION 3.12)
project(SaneProgram)

# Set the C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_WIN32_EXECUTABLE ON)

# Specify the custom build directory
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)

# if true, reduces exe size and you will have faster compile times.
# it doesn't build astc encoder and ufbx. also it uses zstddeclib instead of entire zstd. (only decompression in game builds)
# look at Scene.cpp first lines, you can change it over there.
set(AX_GAME_BUILD FALSE)

# Add your source files here
set(SOURCES
    ASTL/Additional/GLTFParser.cpp
    ASTL/Additional/OBJParser.cpp
    ASTL/Additional/Profiler.cpp
    External/vulkan/shVulkan.c

    src/Animation.cpp
    src/AssetManager.cpp
    src/SaneProgram.cpp
    src/Renderer.cpp
    src/UI.cpp
    src/Menu.cpp
    src/PlatformWindows.cpp
    src/VulkanBackend.cpp
    src/Scene.cpp
    src/SceneRenderer.cpp
    src/HBAO.cpp
    src/Texture.cpp
    src/CharacterController.cpp
	src/BVH.cpp
    src/TLAS.cpp
)

if(AX_GAME_BUILD)
	list(APPEND SOURCES External/zstddeclib.c) # decompress files. by facebook (meta)
endif()

# add astc encoder and ufbx if we are in editor build.
if(NOT AX_GAME_BUILD)
  
  list(APPEND SOURCES 
       External/ufbx.c
       External/zstd.c # game build include all zstd library. (compressor and decompressor)
       External/ProcessDxtc.cpp) # compress dxt and bcn textures

  # Add all .cpp files from External/astc-encoder to the SOURCES list
  file(GLOB ASTC_ENCODER_SOURCES External/astc-encoder/*.cpp)
  
  # Append the ASTC_ENCODER_SOURCES to the SOURCES list
  list(APPEND SOURCES ${ASTC_ENCODER_SOURCES})
endif()

include_directories(${CMAKE_SOURCE_DIR})

# link_directories(${CMAKE_SOURCE_DIR}/libs)

# Add an executable target
add_executable(SaneProgram ${SOURCES})

# Set the resource file for Visual Studio
set_target_properties(SaneProgram PROPERTIES
    VS_RESOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/SaneProgram.rc"
)

set_property(TARGET SaneProgram PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/PROFILE")

target_link_libraries(SaneProgram 
    user32
    gdi32
    opengl32
    # GFSDK_SSAO.win64.lib
)

# Set the source files for ASTC encoder in Visual Studio
source_group("ASTC Encoder" FILES ${ASTC_ENCODER_SOURCES})

file(COPY ${CMAKE_SOURCE_DIR}/SaneProgram.res DESTINATION ${CMAKE_BINARY_DIR})