
#include "include/Renderer.hpp"
#include "include/Animation.hpp"
#include "include/Platform.hpp"

#include "include/Scene.hpp"
#include "include/SceneRenderer.hpp"
#include "include/CharacterController.hpp"
#include "include/Camera.hpp"
#include "include/TextRenderer.hpp"

static PrefabID GLTFPrefab = 0;
static PrefabID AnimatedPrefab = 0;

CharacterController characterController={};

void AXInit()
{
    wSetWindowName("Engine");
    // wSetWindowSize(1920, 1080);

    wSetWindowPosition(0, 0);
    wSetVSync(true);
}

// return 1 if success
int AXStart()
{
    g_CurrentScene.Init();

    if (!g_CurrentScene.ImportPrefab(&GLTFPrefab, "Meshes/SponzaGLTF/scene.gltf", 1.2f))
    // if (!g_CurrentScene.ImportPrefab(&GLTFPrefab, "Meshes/GroveStreet/GroveStreet.gltf", 1.14f))
    {
        AX_ERROR("gltf scene load failed");
        return 0;
    }

    if (!g_CurrentScene.ImportPrefab(&AnimatedPrefab, "Meshes/Paladin/Paladin.gltf", 1.0f))
    {
        AX_ERROR("gltf scene load failed2");
        return 0;
    }

    TextRendererInitialize();
    // very good font that has lots of icons: http://www.quivira-font.com/
    LoadFontAtlas("Fonts/Quivira.otf");

    MemsetZero(&characterController, sizeof(CharacterController));
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
void AXLoop(bool shouldRender)
{
    Scene* currentScene = &g_CurrentScene;
    currentScene->Update();
    
    float deltaTime = (float)GetDeltaTime();
    deltaTime = MIN(deltaTime, 0.2f);

    // animate and control the movement of character
    characterController.Update(deltaTime);
    AnimationController* animController = &characterController.mAnimController;

    using namespace SceneRenderer;

    if (!shouldRender)
        return;

    BeginShadowRendering(currentScene);
    {
        RenderShadowOfPrefab(currentScene, GLTFPrefab, nullptr);
        // don't render shadow of character, we will fake it.
        // RenderShadowOfPrefab(currentScene, AnimatedPrefab, animController);
    }
    EndShadowRendering();

    BeginRendering();
    {
        RenderPrefab(currentScene, GLTFPrefab, nullptr);
        RenderPrefab(currentScene, AnimatedPrefab, animController);
    }
    EndRendering();

    static int fps = 60;
    static char fpsTxt[16] = {'6', '0'};

    if (int(TimeSinceStartup()) & 1)
    {
        double dt = GetDeltaTime();
        fps = (int)(1.0 / dt);
        IntToString(fpsTxt, fps);
    }

    DrawText(fpsTxt, 85.0f, 85.0f, 1.0, 0);
    
    DrawText("Anılcan Gülkaya", 
             100.0f, 950.0f, // x,y pos
             1.0f, 0); // scale, atlas

    // RenderScene(&FBXScene);
    // todo material system
}

void AXExit()
{
    g_CurrentScene.Destroy();
    characterController.Destroy();
    DestroyAnimationSystem();
    DestroyTextRenderer();
    SceneRenderer::Destroy();
}
