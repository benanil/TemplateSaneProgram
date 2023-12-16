# Makefile for SaneProgram

CXX = g++
CXXFLAGS = -std=c++17 -O3 -mavx2 -march=native -msse4.2 -mwindows -fno-rtti -s 
LDFLAGS = -lopengl32 -luser32 -lgdi32
SRC_DIR = .
ASTL_DIR = ASTL/Additional
EXT_DIR = External
OBJ_DIR = obj

SOURCES = $(SRC_DIR)/SaneProgram.cpp \
          $(ASTL_DIR)/GLTFParser.cpp \
          $(ASTL_DIR)/OBJParser.cpp \
          $(EXT_DIR)/ufbx.c \
          $(SRC_DIR)/PlatformWindows.cpp \
          $(SRC_DIR)/Renderer.cpp \
          $(SRC_DIR)/AssetManager.cpp \
          $(SRC_DIR)/Scene.cpp 

OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SOURCES))

# resource file that our icon and application information stays
RESOURCE_FILE = SaneProgram.res

TARGET = SaneProgram

# check if obj directory exist, otherwise create it
ifeq ($(wildcard $(OBJ_DIR)),)
    $(shell mkdir $(OBJ_DIR))
endif

$(TARGET): $(OBJECTS) $(RESOURCE_FILE)
	$(CXX) $(CXXFLAGS) $^ -o $@ -static-libstdc++ -static-libgcc $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/%.res: %.rc
	windres $< -O coff -o $@

clean:
	rm -f $(TARGET) $(OBJECTS) $(OBJ_DIR)/*.res