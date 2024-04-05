
#include "Renderer.hpp"
#include "Animation.hpp"
#include "Platform.hpp"

#include "Scene.hpp"
#include "SceneRenderer.hpp"
#include "CharacterController.hpp"
#include "Camera.hpp"


static PrefabID GLTFPrefab = 0;
static PrefabID AnimatedPrefab = 0;

CharacterController characterController={};

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

    StartAnimationSystem();
    Prefab* paladin = g_CurrentScene.GetPrefab(AnimatedPrefab);
    characterController.Start(paladin);

    SceneRenderer::Init();
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
    Scene* currentScene = &g_CurrentScene;
    currentScene->Update();
    
    float deltaTime = (float)GetDeltaTime();

    // animate and control the movement of character
    characterController.Update(deltaTime);
    AnimationController* animController = &characterController.animationController;

    using namespace SceneRenderer;

    BeginShadowRendering(currentScene);
    {
        RenderShadowOfPrefab(currentScene, GLTFPrefab, nullptr);
        RenderShadowOfPrefab(currentScene, AnimatedPrefab, animController);
    }
    EndShadowRendering();

    BeginRendering();
    {
        RenderPrefab(currentScene, GLTFPrefab, nullptr);
        RenderPrefab(currentScene, AnimatedPrefab, animController);
    }
    EndRendering();


    // RenderScene(&FBXScene);
    // todo material system
}

void AXExit()
{
    g_CurrentScene.Destroy();
    characterController.Destroy();
    DestroyAnimationSystem();
    SceneRenderer::Destroy();
}
