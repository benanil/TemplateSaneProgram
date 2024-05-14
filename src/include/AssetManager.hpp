
#pragma once

// game build doesn't have astc encoder, ufbx, dxt encoder. 
// because we are only decoding when we release the game
// if true, reduces exe size and you will have faster compile times.
// also it uses zstddeclib instead of entire zstd. (only decompression in game builds) go to CMakeLists.txt for more details
#if defined(__ANDROID__)
    #define AX_GAME_BUILD 1
#else
    #define AX_GAME_BUILD 1 /* make zero for editor build */
#endif


#include "../../ASTL/Additional/GLTFParser.hpp"

int LoadFBX(const char* path, SceneBundle* fbxScene, float scale);

int SaveGLTFBinary(SceneBundle* gltf, const char* path);

int LoadGLTFBinary(const char* path, SceneBundle* gltf);

void CreateVerticesIndices(SceneBundle* gltf);

void CreateVerticesIndicesSkined(SceneBundle* gltf);

// ABM = AX binary mesh
bool IsABMLastVersion(const char* path);

bool IsTextureLastVersion(const char* path);

// From Texture.cpp
void SaveSceneImages(struct Prefab* scene, char* path);

void LoadSceneImages(char* path, struct Texture*& textures, int numImages);
