@echo off

if "%1"=="Debug" (
    echo Debug build is running
    devenv build/SaneProgram.vcxproj /Build "Debug|x64"
) else (
    echo Release build is running
    devenv build/SaneProgram.vcxproj /Build "Release|x64"
    start "" "build/Release/SaneProgram.exe"
)

REM Compile the C++ code using clang++
REM External/zstddeclib.c ^
REM clang++ -std=c++14 -O3 -mavx2 -mfma -march=native -msse4.2 -mwindows -luser32 ^
REM -s -fno-rtti -fno-stack-protector -fno-exceptions -static-libstdc++ -static-libgcc -fno-unwind-tables -D_CRT_SECURE_NO_WARNINGS  ^
REM src/SaneProgram.cpp ^
REM ASTL/Additional/GLTFParser.cpp ^
REM ASTL/Additional/OBJParser.cpp ^
REM External/zstddeclib.c ^ 
REM src/PlatformWindows.cpp ^
REM src/AssetManager.cpp ^
REM src/Animation.cpp ^
REM src/CharacterController.cpp ^
REM src/Renderer.cpp ^
REM src/TextRenderer.cpp ^
REM src/Scene.cpp ^
REM src/SceneRenderer.cpp ^
REM src/Texture.cpp ^
REM External/ProcessDxtc.cpp ^
REM External/ufbx.c ^
REM -o SaneProgram.exe ^
REM -lopengl32 -lgdi32  SaneProgram.res

REM REM Compile the C++ code using g++
REM  g++ -std=c++14 -O3 -mavx2 -mfma -march=native -msse4.2 -mwindows -luser32 ^
REM  -s -fno-rtti -fno-stack-protector -fno-exceptions -static-libstdc++ -static-libgcc -fno-unwind-tables ^
REM  src/SaneProgram.cpp ^
REM  ASTL/Additional/GLTFParser.cpp ^
REM  ASTL/Additional/OBJParser.cpp ^
REM  External/ProcessDxtc.cpp ^
REM  External/zstddeclib.c ^
REM  External/ufbx.c ^
REM  src/PlatformWindows.cpp ^
REM  src/AssetManager.cpp ^
REM  src/Animation.cpp ^
REM  src/CharacterController.cpp ^
REM  src/Renderer.cpp ^
REM  src/TextRenderer.cpp ^
REM  src/Scene.cpp ^
REM  src/SceneRenderer.cpp ^
REM  src/Texture.cpp ^
REM  -o SaneProgram ^
REM  -lopengl32 -lgdi32 -mno-needed SaneProgram.res

REM ------ if this is Editor build compile  External/zstddeclib.c 
REM External/astc-encoder/astcenc_averages_and_directions.cpp     ^
REM External/astc-encoder/astcenc_block_sizes.cpp                 ^
REM External/astc-encoder/astcenc_color_quantize.cpp              ^
REM External/astc-encoder/astcenc_color_unquantize.cpp            ^
REM External/astc-encoder/astcenc_compress_symbolic.cpp           ^
REM External/astc-encoder/astcenc_compute_variance.cpp            ^
REM External/astc-encoder/astcenc_decompress_symbolic.cpp         ^
REM External/astc-encoder/astcenc_diagnostic_trace.cpp            ^
REM External/astc-encoder/astcenc_entry.cpp                       ^
REM External/astc-encoder/astcenc_find_best_partitioning.cpp      ^
REM External/astc-encoder/astcenc_ideal_endpoints_and_weights.cpp ^
REM External/astc-encoder/astcenc_image.cpp                       ^
REM External/astc-encoder/astcenc_integer_sequence.cpp            ^
REM External/astc-encoder/astcenc_mathlib.cpp                     ^
REM External/astc-encoder/astcenc_mathlib_softfloat.cpp           ^
REM External/astc-encoder/astcenc_partition_tables.cpp            ^
REM External/astc-encoder/astcenc_percentile_tables.cpp           ^
REM External/astc-encoder/astcenc_pick_best_endpoint_format.cpp   ^
REM External/astc-encoder/astcenc_quantization.cpp                ^
REM External/astc-encoder/astcenc_symbolic_physical.cpp           ^
REM External/astc-encoder/astcenc_weight_align.cpp                ^
REM External/astc-encoder/astcenc_weight_quant_xfer_tables.cpp    ^

REM g++ -std=c++14 -O3 -mavx2 -march=native -msse4.2 -fno-rtti -fno-stack-protector -fno-exceptions -static-libstdc++ -static-libgcc RandomSSAOKernelGen.cpp -o RandomSSAOKernelGen.exe











