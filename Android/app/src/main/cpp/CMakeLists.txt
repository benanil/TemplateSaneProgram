# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

cmake_minimum_required(VERSION 3.22.1)

#support neon
ENABLE_LANGUAGE(ASM)

project("test")

# Creates your game shared library. The name must be the same as the
# one used for loading in your Kotlin/Java or AndroidManifest.txt files.
add_library(test SHARED
        PlatformAndroid.cpp
        ../../../../../src/Animation.cpp
        ../../../../../src/AssetManager.cpp
        ../../../../../src/SaneProgram.cpp
        ../../../../../src/Renderer.cpp
        ../../../../../src/UI.cpp
        ../../../../../src/Menu.cpp
        ../../../../../src/Scene.cpp
        ../../../../../src/SceneRenderer.cpp
        ../../../../../src/HBAO.cpp
        ../../../../../src/Texture.cpp
        ../../../../../src/CharacterController.cpp
        ../../../../../src/BVH.cpp
        ../../../../../src/TLAS.cpp
        ../../../../../ASTL/Additional/OBJParser.cpp
        ../../../../../ASTL/Additional/GLTFParser.cpp
        ../../../../../External/zstddeclib.c)

if(ANDROID_ABI STREQUAL armeabi-v7a OR ANDROID_ABI STREQUAL arm64-v8a)
    set_target_properties(test PROPERTIES COMPILE_FLAGS -mfpu=vfpv3-d16)
endif()

if(ANDROID_ABI STREQUAL armeabi-v7a)
    set_target_properties(test PROPERTIES COMPILE_FLAGS -mfpu=neon -ANDROID_ARM_NEON=ON -ANDROID_ABI=armeabi-v7a)
    target_compile_definitions(test PRIVATE ANDROID_ABI=armeabi-v7a RELEASE=1)

elseif(ANDROID_ABI STREQUAL arm64-v8a)
    set_target_properties(test PROPERTIES COMPILE_FLAGS -mfpu=neon -ANDROID_ARM_NEON=ON -ANDROID_ABI=arm64-v8a)
    target_compile_definitions(test PRIVATE ANDROID_ABI=arm64-v8a RELEASE=1)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${../../../../../})

# Searches for a package provided by the game activity dependency
find_package(game-activity REQUIRED CONFIG)
find_package(games-frame-pacing REQUIRED CONFIG)

# Configure libraries CMake uses to link your target library.
target_link_libraries(test
        # The game activity
        game-activity::game-activity
        games-frame-pacing::swappy

        # EGL and other dependent libraries required for drawing
        # and interacting with Android system
        EGL
        GLESv3
        jnigraphics
        android
        log)