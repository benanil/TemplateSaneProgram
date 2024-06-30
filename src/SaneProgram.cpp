
#include "include/Renderer.hpp"
#include "include/Animation.hpp"
#include "include/Platform.hpp"

#include "include/Scene.hpp"
#include "include/SceneRenderer.hpp"
#include "include/CharacterController.hpp"
#include "include/Camera.hpp"
#include "include/UI.hpp"
#include "include/Menu.hpp"

#include "../ASTL/String.hpp"

static PrefabID GLTFPrefab = 0;
static PrefabID AnimatedPrefab = 0;

CharacterController characterController={};

static void WindowResizeCallback(int width, int height)
{
    SceneRenderer::WindowResizeCallback(width, height);
    uWindowResizeCallback(width, height);
}

static void KeyPressCallback(unsigned key)
{
    uKeyPressCallback(key); // send to ui to process
}

void AXInit()
{
    wSetWindowName("Engine");

    wSetWindowPosition(0, 0);
    wSetVSync(true);
    wSetWindowResizeCallback(WindowResizeCallback);
    wSetKeyPressCallback(KeyPressCallback);
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

    uInitialize();
    // very good font that has lots of icons: http://www.quivira-font.com/
    uLoadFont("Fonts/JetBrainsMono-Regular.ttf"); // "Fonts/Quivira.otf"
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

static bool pauseMenuOpened = false;

// do rendering and main loop here
void AXLoop(bool canRender)
{
    using namespace SceneRenderer;
    
    uBegin(); // user interface begin

    // draw when we are playing game, don't render when using pause menu to save power
    if (canRender && (pauseMenuOpened || GetMenuState() == MenuState_Gameplay || ShouldReRender()))
    {
        Scene* currentScene = &g_CurrentScene;
        currentScene->Update();
    
        float deltaTime = (float)GetDeltaTime();
        deltaTime = MIN(deltaTime, 0.2f);
            
        // animate and control the movement of character
        const bool isSponza = true;
        characterController.Update(deltaTime, isSponza);
        AnimationController* animController = &characterController.mAnimController;

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
        bool renderToBackBuffer = !pauseMenuOpened;
        EndRendering(renderToBackBuffer);
        pauseMenuOpened = false;
    }
    else
    {
        DrawLastRenderedFrame();
    }
    pauseMenuOpened = ShowMenu(); // < from Menu.cpp
    
    uRender(); // < user interface end 
    // RenderScene(&FBXScene);
    // todo material system
}


void AXExit()
{
    g_CurrentScene.Destroy();
    characterController.Destroy();
    DestroyAnimationSystem();
    uDestroy();
    SceneRenderer::Destroy();
}
