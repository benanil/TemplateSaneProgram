@echo off

REM clang++ -std=c++17 -O3 -mavx2 -mfma -msse4.2 -mf16c -mrdseed -s -fno-rtti -fno-stack-protector -fno-exceptions -static-libstdc++ -static-libgcc -fno-unwind-tables -D_CRT_SECURE_NO_WARNINGS ^
REM AdventOfCode.cpp -o AdventOfCode.exe
REM this is test
REM call AdventOfCode.exe

if "%1"=="Debug" (
    echo Debug build is running
    devenv build/SaneProgram.vcxproj /Build "Debug|x64"
) else (
    echo Release build is running
    devenv build/SaneProgram.vcxproj /Build "Release|x64"
    start "" "build/Release/SaneProgram.exe"
)
REM External/zstddeclib.c
REM External/zstd.c ^

REM Compile the C++ code using clang++
REM clang++ -std=c++17 -O3 -mavx2 -mfma -msse4.2 -mf16c -mrdseed -mwindows -luser32 -lopengl32 -lgdi32 -s -fno-rtti -fno-stack-protector -fno-exceptions -static-libstdc++ -static-libgcc -fno-unwind-tables -D_CRT_SECURE_NO_WARNINGS ^
REM -DSANE_WINDOWS_BUILD ^
REM ASTL/Additional/GLTFParser.cpp ^
REM ASTL/Additional/OBJParser.cpp ^
REM ASTL/Additional/Profiler.cpp ^
REM External/vulkan/shVulkan.c ^
REM External/zstd.c ^
REM External/ufbx.c ^
REM External/ProcessDxtc.cpp ^
REM src/Animation.cpp ^
REM src/AssetManager.cpp ^
REM src/SaneProgram.cpp ^
REM src/Renderer.cpp ^
REM src/UI.cpp ^
REM src/Menu.cpp ^
REM src/PlatformWindows.cpp ^
REM src/VulkanBackend.cpp ^
REM src/Scene.cpp ^
REM src/SceneRenderer.cpp ^
REM src/Texture.cpp ^
REM src/CharacterController.cpp ^
REM src/BVH.cpp ^
REM src/TLAS.cpp ^
REM src/HBAO.cpp ^
REM src/Editor.cpp ^
REM src/Terrain.cpp ^
REM -o SaneProgram.exe ^
REM -lopengl32 -lgdi32  SaneProgram.res

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

REM Compile the C++ code using g++
REM g++ -std=c++17 -O3 -mavx2 -mfma -msse4.2 -mf16c -mrdseed -mwindows -luser32 ^
REM -s -fno-rtti -fno-stack-protector -fno-exceptions -static-libstdc++ -static-libgcc -fno-unwind-tables -Wignored-attributes ^
REM -DSANE_WINDOWS_BUILD ^
REM src/SaneProgram.cpp ^
REM ASTL/Additional/GLTFParser.cpp ^
REM ASTL/Additional/OBJParser.cpp ^
REM External/ProcessDxtc.cpp ^
REM External/zstddeclib.c ^
REM External/ufbx.c ^
REM src/PlatformWindows.cpp ^
REM src/AssetManager.cpp ^
REM src/Animation.cpp ^
REM src/CharacterController.cpp ^
REM src/Renderer.cpp ^
REM src/UI.cpp ^
REM src/Menu.cpp ^
REM src/Scene.cpp ^
REM src/SceneRenderer.cpp ^
REM src/Texture.cpp ^
REM src/BVH.cpp ^
REM src/TLAS.cpp ^
REM src/Editor.cpp ^
REM src/Terrain.cpp ^
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
REM -o SaneProgram ^
REM -lopengl32 -lgdi32 SaneProgram.res

REM ------ if this is Editor build compile  External/zstddeclib.c 
REM g++ -std=c++14 -O3 -mavx2 -march=native -msse4.2 -fno-rtti -fno-stack-protector -fno-exceptions -static-libstdc++ -static-libgcc RandomSSAOKernelGen.cpp -o RandomSSAOKernelGen.exe











