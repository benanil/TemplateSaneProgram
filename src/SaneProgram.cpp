

#include <stdio.h>

#include "../ASTL/Additional/GLTFParser.hpp"

#include "Renderer.hpp"
#include "Platform.hpp"

#include "Scene.hpp"

SubSceneID GLTFScene = 0;

Shader shader;
Texture skyTexture;

void AXInit()
{
    SetWindowName("Engine");
    SetWindowSize(1920, 1080);

    SetWindowPosition(0, 0);
    SetVSync(true);
}

// return 1 if success
int AXStart()
{
    if (!g_CurrentScene.ImportSubScene(&GLTFScene, "Meshes/SponzaGLTF/scene.gltf", 0.02f))
    {
        AX_ERROR("gltf scene load failed");
        return 0;
    }
    
    skyTexture = LoadTexture("Textures/orange-top-gradient-background.jpg", false);
    shader     = ImportShader("Shaders/3DVert.glsl", "Shaders/PBRFrag.glsl");
    
    return 1;
}

// do rendering and main loop here
void AXLoop()
{
    SetDepthTest(false);
    // works like a skybox
    RenderFullScreen(skyTexture.handle);
    SetDepthTest(true);

    g_CurrentScene.UpdateSubScene(GLTFScene);

    BindShader(shader);
    
    g_CurrentScene.RenderSubScene(GLTFScene);
    // RenderScene(&FBXScene);

    // todo material and light system
}

void AXExit()
{
    DeleteShader(shader);
    DeleteTexture(skyTexture);
}
