#pragma once

#include "Renderer.hpp"

struct Scene
{
    ParsedGLTF data;
    Mesh* meshes;
    Texture* textures;
};

int ImportScene(Scene* scene, const char* path, float scale, bool LoadToGPU);

void RenderScene(Scene* scene);

void RenderOneMesh(Mesh mesh, Texture albedo, Texture normal, Texture metallic, Texture roughness);

void UpdateScene(Scene* scene);

void DestroyScene(Scene* scene);
