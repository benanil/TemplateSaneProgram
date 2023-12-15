@echo off

REM Compile the C++ code using g++
REM GLTFParser.cpp
g++ -std=c++14 -O1 -mavx2 -march=native -msse4.2 -mwindows -luser32 ^
-s -fno-rtti -fno-stack-protector -fno-exceptions -static-libstdc++ -static-libgcc -fno-unwind-tables ^
SaneProgram.cpp ^
ASTL/Additional/GLTFParser.cpp ^
ASTL/Additional/OBJParser.cpp ^
External/ufbx.c ^
PlatformWindows.cpp ^
AssetManager.cpp ^
Renderer.cpp ^
-o SaneProgram ^
-lopengl32 -lgdi32 -mno-needed SaneProgram.res
