@echo off

REM Compile the C++ code using g++
REM GLTFParser.cpp
g++ -std=c++14 -O3 -mavx2 -march=native -msse4.2 -mwindows -luser32 ^
-s -fno-rtti -fno-stack-protector -fno-exceptions -static-libstdc++ -static-libgcc -fno-unwind-tables ^
SaneProgram.cpp ^
ASTL/Additional/GLTFParser.cpp ^
ASTL/Additional/OBJParser.cpp ^
External/etcpak/ProcessDxtc.cpp ^
External/zstd/zstd.c ^
External/ufbx.c ^
External/astc-encoder/astcenccli_error_metrics.cpp            ^
External/astc-encoder/astcenccli_image.cpp                    ^
External/astc-encoder/astcenccli_image_external.cpp           ^
External/astc-encoder/astcenccli_image_load_store.cpp         ^
External/astc-encoder/astcenccli_platform_dependents.cpp      ^
External/astc-encoder/astcenccli_toplevel.cpp                 ^
External/astc-encoder/astcenccli_toplevel_help.cpp            ^
External/astc-encoder/astcenc_averages_and_directions.cpp     ^
External/astc-encoder/astcenc_block_sizes.cpp                 ^
External/astc-encoder/astcenc_color_quantize.cpp              ^
External/astc-encoder/astcenc_color_unquantize.cpp            ^
External/astc-encoder/astcenc_compress_symbolic.cpp           ^
External/astc-encoder/astcenc_compute_variance.cpp            ^
External/astc-encoder/astcenc_decompress_symbolic.cpp         ^
External/astc-encoder/astcenc_diagnostic_trace.cpp            ^
External/astc-encoder/astcenc_entry.cpp                       ^
External/astc-encoder/astcenc_find_best_partitioning.cpp      ^
External/astc-encoder/astcenc_ideal_endpoints_and_weights.cpp ^
External/astc-encoder/astcenc_image.cpp                       ^
External/astc-encoder/astcenc_integer_sequence.cpp            ^
External/astc-encoder/astcenc_mathlib.cpp                     ^
External/astc-encoder/astcenc_mathlib_softfloat.cpp           ^
External/astc-encoder/astcenc_partition_tables.cpp            ^
External/astc-encoder/astcenc_percentile_tables.cpp           ^
External/astc-encoder/astcenc_pick_best_endpoint_format.cpp   ^
External/astc-encoder/astcenc_quantization.cpp                ^
External/astc-encoder/astcenc_symbolic_physical.cpp           ^
External/astc-encoder/astcenc_weight_align.cpp                ^
External/astc-encoder/astcenc_weight_quant_xfer_tables.cpp    ^
External/astc-encoder/wuffs-v0.3.c                            ^
PlatformWindows.cpp ^
AssetManager.cpp ^
Renderer.cpp ^
Scene.cpp ^
-o SaneProgram ^
-lopengl32 -lgdi32 -mno-needed SaneProgram.res
