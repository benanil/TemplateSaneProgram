

#include "Renderer.hpp"
#include "Platform.hpp"

#include "Scene.hpp"
#include "SceneRenderer.hpp"

SubSceneID GLTFScene = 0;

void AXInit()
{
    wSetWindowName("Engine");
    wSetWindowSize(1920, 1080);

    wSetWindowPosition(0, 0);
    wSetVSync(true);
}

// return 1 if success
int AXStart()
{
    if (!g_CurrentScene.ImportSubScene(&GLTFScene, "Meshes/SponzaGLTF/scene.gltf", 0.02f))
    {
        AX_ERROR("gltf scene load failed");
        return 0;
    }
   
    g_CurrentScene.Init();
    SceneRenderer::Init();
    return 1;
}

// do rendering and main loop here
void AXLoop()
{
    g_CurrentScene.UpdateSubScene(GLTFScene);
    SceneRenderer::RenderSubScene(&g_CurrentScene, GLTFScene);
    SceneRenderer::PostProcessPass();
    // RenderScene(&FBXScene);
    // todo material and light system
}

void AXExit()
{
    g_CurrentScene.Destroy();
    SceneRenderer::Destroy();
}
