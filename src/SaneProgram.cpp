

#include "Renderer.hpp"
#include "Animation.hpp"
#include "Platform.hpp"

#include "Scene.hpp"
#include "SceneRenderer.hpp"

static PrefabID GLTFPrefab = 0;
static PrefabID AnimatedPrefab = 0;
static AnimationController animationController;

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
    if (!g_CurrentScene.ImportPrefab(&GLTFPrefab, "Meshes/SponzaGLTF/scene.gltf", 1.2f))
    {
        AX_ERROR("gltf scene load failed");
        return 0;
    }
   
    g_CurrentScene.Init();
    if (!g_CurrentScene.ImportPrefab(&AnimatedPrefab, "Meshes/Paladin/Paladin.gltf", 1.0f))
    {
        AX_ERROR("gltf scene load failed2");
        return 0;
    }

    Prefab* animatedPrefab = g_CurrentScene.GetPrefab(AnimatedPrefab);
    MemsetZero(&animationController, sizeof(AnimationController));
    CreateAnimationController(animatedPrefab, &animationController);
    SceneRenderer::Init();
    StartAnimationSystem();

    SceneRenderer::BeginUpdateLights();

    g_CurrentScene.AddPointLight(MakeVec3(-13.0f, 7.0f, -1.5f), PackColorRGBU32(0.58f, 0.52f, 0.40f), 0.380f, 12.0f);
    SceneRenderer::UpdateLight(0, &g_CurrentScene.m_PointLights[0]);
    
    LightInstance spotLight;
    spotLight.position  = MakeVec3(-17.0f, 2.0f, -1.0f);
    spotLight.direction = MakeVec3(-1.0f, -0.0f, 0.0f);
    spotLight.color     = PackColorRGBU32(0.88f, 0.10f, 0.18f);
    spotLight.intensity = 20.0f;
    spotLight.cutoff    = 0.8f;
    spotLight.range     = 25.0f;
    g_CurrentScene.AddLight(spotLight);
    SceneRenderer::UpdateLight(0, &spotLight);
    
    SceneRenderer::EndUpdateLights();
    return 1;
}

// static double t;
// do rendering and main loop here
void AXLoop()
{
    g_CurrentScene.Update();
    
    double t = (double)TimeSinceStartup();
    Scene* currentScene = &g_CurrentScene;
    
    using namespace SceneRenderer;
    EvaluateAnimOfPrefab(currentScene->GetPrefab(AnimatedPrefab), 2, t, &animationController);
    
    BeginShadowRendering(&g_CurrentScene);
    {
        RenderShadowOfPrefab(&g_CurrentScene, GLTFPrefab, nullptr);
        RenderShadowOfPrefab(&g_CurrentScene, AnimatedPrefab, &animationController);
    }
    EndShadowRendering();
    
    BeginRendering();
    {
        RenderPrefab(&g_CurrentScene, GLTFPrefab, nullptr);
        RenderPrefab(&g_CurrentScene, AnimatedPrefab, &animationController);
    }
    EndRendering();
    // RenderScene(&FBXScene);
    // todo material system
}

void AXExit()
{
    g_CurrentScene.Destroy();
    ClearAnimationController(&animationController);
    DestroyAnimationSystem();
    SceneRenderer::Destroy();
}
