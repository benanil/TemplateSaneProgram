

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

    g_CurrentScene.AddPointLight(MakeVec3(-13.0f, 7.0f, -1.5f), PackColorRGBU32(0.58f, 0.52f, 0.40f), 0.380f, 12.0f);
    g_CurrentScene.AddPointLight(MakeVec3(+13.0f, 2.0f,  0.5f), PackColorRGBU32(0.58f, 0.52f, 0.40f), 0.399f, 9.0f);
    // g_CurrentScene.AddPointLight(MakeVec3(-02.0f, 4.0f, -1.5f), PackColorRGBU32(0.58f, 0.52f, 0.40f), 0.380f, 7.0f);
    // g_CurrentScene.AddPointLight(MakeVec3(+02.0f, 4.0f, -1.5f), PackColorRGBU32(0.58f, 0.52f, 0.40f), 0.380f, 7.0f);

    LightInstance spotLight;
    spotLight.position    = MakeVec3(-17.0f, 2.0f, -1.0f);
    spotLight.direction   = MakeVec3(-1.0f, -0.0f, 0.0f);
    spotLight.color       = PackColorRGBU32(0.88f, 0.10f, 0.18f);
    spotLight.intensity   = 20.0f;
    spotLight.cutoff = 0.8f;
    spotLight.range = 25.0f;
    g_CurrentScene.AddLight(spotLight);

    SceneRenderer::BeginUpdateLights();
    
    SceneRenderer::UpdateLight(0, &spotLight);
    
    for (int i = 0; i < 4; i++)
        SceneRenderer::UpdateLight(i, &g_CurrentScene.m_PointLights[i]);
    
    SceneRenderer::EndUpdateLights();
    return 1;
}

// do rendering and main loop here
void AXLoop()
{
    g_CurrentScene.UpdateSubScene(GLTFScene);
    SceneRenderer::BeginRendering();
    SceneRenderer::RenderSubScene(&g_CurrentScene, GLTFScene);
    SceneRenderer::EndRendering();
    // RenderScene(&FBXScene);
    // todo material and light system
}

void AXExit()
{
    g_CurrentScene.Destroy();
    SceneRenderer::Destroy();
}
