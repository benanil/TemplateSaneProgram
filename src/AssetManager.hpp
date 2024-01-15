

#include "Scene.hpp"

int LoadFBX(const char* path, ParsedGLTF* fbxScene, float scale);

int SaveGLTFBinary(ParsedGLTF* gltf, const char* path);

int LoadGLTFBinary(const char* path, ParsedGLTF* gltf);

void CreateVerticesIndices(ParsedGLTF* gltf);

void SaveSceneImages(Scene* scene, char* path);

void LoadSceneImages(char* path, Texture*& textures, int numImages);

// ABM = AX binary mesh
bool IsABMLastVersion(const char* path);