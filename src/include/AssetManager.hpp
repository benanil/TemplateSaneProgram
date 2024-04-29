

#include "Scene.hpp"

int LoadFBX(const char* path, SceneBundle* fbxScene, float scale);

int SaveGLTFBinary(SceneBundle* gltf, const char* path);

int LoadGLTFBinary(const char* path, SceneBundle* gltf);

void CreateVerticesIndices(SceneBundle* gltf);

void CreateVerticesIndicesSkined(SceneBundle* gltf);

// ABM = AX binary mesh
bool IsABMLastVersion(const char* path);

bool IsTextureLastVersion(const char* path);

// From Texture.cpp
void SaveSceneImages(Prefab* scene, char* path);

void LoadSceneImages(char* path, Texture*& textures, int numImages);
