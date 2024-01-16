# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

cmake_minimum_required(VERSION 3.22.1)

project("game")

#support neon
ENABLE_LANGUAGE(ASM)

# Creates your game shared library. The name must be the same as the
# one used for loading in your Kotlin/Java or AndroidManifest.txt files.
add_library(game SHARED
        ../../../../../src/PlatformAndroid.cpp
        ../../../../../src/SaneProgram.cpp
        ../../../../../src/Renderer.cpp
        ../../../../../src/Scene.cpp
        ../../../../../src/AssetManager.cpp
        ../../../../../src/Texture.cpp
        ../../../../../ASTL/Additional/OBJParser.cpp
        ../../../../../ASTL/Additional/GLTFParser.cpp
        ../../../../../External/zstd.c)

include_directories(${../../../../../})

if(ANDROID_ABI STREQUAL armeabi-v7a)
    set_target_properties(${game} PROPERTIES COMPILE_FLAGS -mfpu=neon -ANDROID_ARM_NEON=ON -ANDROID_ABI=armeabi-v7a)
    target_compile_definitions(game PRIVATE ANDROID_ABI=armeabi-v7a RELEASE=1)

elseif(ANDROID_ABI STREQUAL arm64-v8a)
    set_target_properties(${game} PROPERTIES COMPILE_FLAGS -mfpu=neon -ANDROID_ARM_NEON=ON -ANDROID_ABI=arm64-v8a)
    target_compile_definitions(game PRIVATE ANDROID_ABI=arm64-v8a RELEASE=1)
endif()

#add_definitions(-DDEBUG)
add_definitions(-DRELEASE)

# Searches for a package provided by the game activity dependency
find_package(game-activity REQUIRED CONFIG)

# Configure libraries CMake uses to link your target library.
target_link_libraries(game
        # The game activity
        game-activity::game-activity

        # EGL and other dependent libraries required for drawing
        # and interacting with Android system
        EGL
        GLESv3
        jnigraphics
        android
        log)