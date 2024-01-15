#pragma once

#include "Renderer.hpp"

struct SceneMesh
{
    GPUMesh  primitive;
    GPUMesh* primitives;
};

struct Scene
{
    ParsedGLTF data;
    SceneMesh* meshes;
    Texture* textures;
};

int ImportScene(Scene* scene, const char* path, float scale, bool LoadToGPU);

void RenderScene(Scene* scene);

void RenderOneMesh(GPUMesh mesh, Texture albedo, Texture normal, Texture metallic, Texture roughness);

void UpdateScene(Scene* scene);

void DestroyScene(Scene* scene);
