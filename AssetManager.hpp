#include "ASTL/Additional/GLTFParser.hpp"

int LoadFBX(const char* path, ParsedGLTF* fbxScene, float scale);

bool SaveGLTFBinary(ParsedGLTF* gltf, const char* path);

bool LoadGLTFBinary(const char* path, ParsedGLTF* gltf);
