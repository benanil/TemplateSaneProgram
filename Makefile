# Makefile for SaneProgram

CXX = g++
CXXFLAGS = -std=c++14 -O3 -mavx2 -march=native -msse4.2 -mf16c -Wall -mwindows -fno-rtti -s -fno-stack-protector -fno-exceptions -fno-unwind-tables
LDFLAGS = -lopengl32 -luser32 -lgdi32
SRC_DIR = src
ASTL_DIR = ASTL/Additional
EXT_DIR = External
OBJ_DIR = obj

SOURCES = $(SRC_DIR)/SaneProgram.cpp \
          $(ASTL_DIR)/GLTFParser.cpp \
          $(ASTL_DIR)/OBJParser.cpp \
          $(SRC_DIR)/PlatformWindows.cpp \
          $(SRC_DIR)/Renderer.cpp \
          $(SRC_DIR)/AssetManager.cpp \
          $(SRC_DIR)/Scene.cpp \
          $(SRC_DIR)/Texture.cpp \
          $(EXT_DIR)/bitset.c

ASTC_SOURCES = $(EXT_DIR)/astc-encoder/astcenccli_error_metrics.cpp     \
$(EXT_DIR)/astc-encoder/astcenccli_image.cpp                    \
$(EXT_DIR)/astc-encoder/astcenccli_image_external.cpp           \
$(EXT_DIR)/astc-encoder/astcenccli_image_load_store.cpp         \
$(EXT_DIR)/astc-encoder/astcenccli_platform_dependents.cpp      \
$(EXT_DIR)/astc-encoder/astcenccli_toplevel.cpp                 \
$(EXT_DIR)/astc-encoder/astcenccli_toplevel_help.cpp            \
$(EXT_DIR)/astc-encoder/astcenc_averages_and_directions.cpp     \
$(EXT_DIR)/astc-encoder/astcenc_block_sizes.cpp                 \
$(EXT_DIR)/astc-encoder/astcenc_color_quantize.cpp              \
$(EXT_DIR)/astc-encoder/astcenc_color_unquantize.cpp            \
$(EXT_DIR)/astc-encoder/astcenc_compress_symbolic.cpp           \
$(EXT_DIR)/astc-encoder/astcenc_compute_variance.cpp            \
$(EXT_DIR)/astc-encoder/astcenc_decompress_symbolic.cpp         \
$(EXT_DIR)/astc-encoder/astcenc_diagnostic_trace.cpp            \
$(EXT_DIR)/astc-encoder/astcenc_entry.cpp                       \
$(EXT_DIR)/astc-encoder/astcenc_find_best_partitioning.cpp      \
$(EXT_DIR)/astc-encoder/astcenc_ideal_endpoints_and_weights.cpp \
$(EXT_DIR)/astc-encoder/astcenc_image.cpp                       \
$(EXT_DIR)/astc-encoder/astcenc_integer_sequence.cpp            \
$(EXT_DIR)/astc-encoder/astcenc_mathlib.cpp                     \
$(EXT_DIR)/astc-encoder/astcenc_mathlib_softfloat.cpp           \
$(EXT_DIR)/astc-encoder/astcenc_partition_tables.cpp            \
$(EXT_DIR)/astc-encoder/astcenc_percentile_tables.cpp           \
$(EXT_DIR)/astc-encoder/astcenc_pick_best_endpoint_format.cpp   \
$(EXT_DIR)/astc-encoder/astcenc_quantization.cpp                \
$(EXT_DIR)/astc-encoder/astcenc_symbolic_physical.cpp           \
$(EXT_DIR)/astc-encoder/astcenc_weight_align.cpp                \
$(EXT_DIR)/astc-encoder/astcenc_weight_quant_xfer_tables.cpp    \
$(EXT_DIR)/astc-encoder/wuffs-v0.3.cpp

EXTERNAL_SOURCES = $(EXT_DIR)/ProcessDxtc.c $(EXT_DIR)/ufbx.c $(EXT_DIR)/zstd.c 

OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SOURCES))
ASTC_OBJECTS = $(patsubst $(EXT_DIR)/astc-encoder/%.cpp,$(OBJ_DIR)/%.o,$(ASTC_SOURCES))
EXTERNAL_OBJECTS = $(OBJ_DIR)/ProcessDxtc.o $(OBJ_DIR)/ufbx.o $(OBJ_DIR)/zstd.o

# resource file that our icon and application information stays
RESOURCE_FILE = SaneProgram.res

TARGET = SaneProgram

# check if obj directory exist, otherwise create it
ifeq ($(wildcard $(OBJ_DIR)),)
    $(shell mkdir $(OBJ_DIR))
endif

$(TARGET): $(OBJECTS) $(ASTC_OBJECTS) $(EXTERNAL_OBJECTS) $(RESOURCE_FILE)
	$(CXX) $(CXXFLAGS) $^ -o $@ -static-libstdc++ -static-libgcc $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(EXT_DIR)/astc-encoder/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(EXT_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(EXT_DIR)/%.c
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/%.res: %.rc
	windres $< -O coff -o $@

clean:
	rm -f $(TARGET) $(OBJECTS) $(ASTC_OBJECTS) $(EXTERNAL_OBJECTS) $(OBJ_DIR)/*.res