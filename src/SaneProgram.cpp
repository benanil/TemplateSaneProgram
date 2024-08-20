
#include <thread>

#include "include/Renderer.hpp"
#include "include/Animation.hpp"
#include "include/Platform.hpp"

#include "include/Scene.hpp"
#include "include/SceneRenderer.hpp"
#include "include/CharacterController.hpp"
#include "include/Camera.hpp"
#include "include/UI.hpp"
#include "include/Menu.hpp"
#include "include/BVH.hpp"
#include "include/TLAS.hpp"

#include "../ASTL/String.hpp"
#include "../ASTL/Math/Half.hpp"
#include "../ASTL/Additional/Profiler.hpp"

#include <stdio.h>

static PrefabID MainScenePrefab = 0;
static PrefabID AnimatedPrefab = 0;
static PrefabID SpherePrefab = 0;

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
}

static Vector2f perfTxtPos;

void PrintPerfFn(const char* text)
{
    uPushFloat(ufTextScale, 0.5f);
    uText(text, perfTxtPos, 0u);
    perfTxtPos.y += 23.0f;
    uPopFloat(ufTextScale);
}

// return 1 if success
int AXStart()
{
    g_CurrentScene.Init();
    InitBVH();

    // if (!g_CurrentScene.ImportPrefab(&MainScenePrefab, "Meshes/Bistro/Bistro.gltf", 1.2f))
    if (!g_CurrentScene.ImportPrefab(&MainScenePrefab, "Meshes/SponzaGLTF/scene.gltf", 1.2f))
    // if (!g_CurrentScene.ImportPrefab(&MainScenePrefab, "Meshes/GroveStreet/GroveStreet.gltf", 1.14f))
    {
        AX_ERROR("gltf scene load failed");
        return 0;
    }

    if (!g_CurrentScene.ImportPrefab(&SpherePrefab, "Meshes/Sphere.gltf", 0.5f))
    {
        AX_ERROR("gltf scene load failed sphere");
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
    Prefab* paladin = g_CurrentScene.GetPrefab(AnimatedPrefab);
    characterController.Start(paladin);

    Prefab* mainScene = g_CurrentScene.GetPrefab(MainScenePrefab);
    int rootNodeIdx = mainScene->GetRootNodeIdx();
    ANode* rootNode = &mainScene->nodes[rootNodeIdx];

    // VecStore(rootNode->rotation, QFromYAngle(HalfPI/2.0f));
    // mainScene->UpdateGlobalNodeTransforms(rootNodeIdx, Matrix4::Identity());
    
    mainScene->tlas = new TLAS(mainScene);
    mainScene->tlas->Build();

    SceneRenderer::Init();
 
    wSetWindowResizeCallback(WindowResizeCallback);
    wSetKeyPressCallback(KeyPressCallback);
    
    return 1;
}

static bool pauseMenuOpened = false;

static void CastRay()
{
    if (!GetMousePressed(MouseButton_Left)) return;

    Prefab* sphere = g_CurrentScene.GetPrefab(SpherePrefab);
    CameraBase* camera = SceneRenderer::GetCamera();
    Scene* currentScene = &g_CurrentScene;

    Vector2f rayPos;
    GetMouseWindowPos(&rayPos.x, &rayPos.y); // { 1920.0f / 2.0f, 1080.0f / 2.0f };

    Prefab* bistro = g_CurrentScene.GetPrefab(MainScenePrefab);
    Triout rayResult = RayCastFromCamera(camera, rayPos, currentScene, MainScenePrefab, nullptr);
    
    // static char rayDistTxt[16] = {};
    // float rayDist = 999.0f;
    if (rayResult.t != RayacastMissDistance) {
        sphere->globalNodeTransforms[0].r[3] = rayResult.position;
    }
    // FloatToString(rayDistTxt, rayDist);
    // uDrawText(rayDistTxt, rayPos);
}

// do rendering and main loop here
void AXLoop(bool canRender)
{
    using namespace SceneRenderer;

    perfTxtPos = { 1100.0f, 500.0f };
    BeginProfile(PrintPerfFn);
    
    uBegin(); // user interface begin
    
    CameraBase* camera = SceneRenderer::GetCamera();
    Scene* currentScene = &g_CurrentScene;

    std::thread raycastThread(CastRay);
    
    // draw when we are playing game, don't render when using pause menu to save power
    if (canRender && (pauseMenuOpened || GetMenuState() == MenuState_Gameplay || ShouldReRender()))
    {
        currentScene->Update();
    
        float deltaTime = (float)GetDeltaTime();
            
        // animate and control the movement of character
        const bool isSponza = false;
        characterController.Update(deltaTime, isSponza);
        AnimationController* animController = &characterController.mAnimController;
        
        BeginShadowRendering(currentScene);
        {
            RenderShadowOfPrefab(currentScene, MainScenePrefab, nullptr);
            // don't render shadow of character, we will fake it.
            // RenderShadowOfPrefab(currentScene, AnimatedPrefab, animController);
        }
        EndShadowRendering();
 
        BeginRendering();
        {
            RenderPrefab(currentScene, MainScenePrefab, nullptr);
            RenderPrefab(currentScene, AnimatedPrefab, animController);
            RenderPrefab(currentScene, SpherePrefab, nullptr);
        }
        bool renderToBackBuffer = !pauseMenuOpened;
        SceneRenderer::EndRendering(renderToBackBuffer);
        pauseMenuOpened = false;
    
        SceneRenderer::ShowEditor();
    }
    else
    {
        DrawLastRenderedFrame();
    }
    pauseMenuOpened = ShowMenu(); // < from Menu.cpp

    rDrawAllLines((float*)SceneRenderer::GetViewProjection());

    uRender(); // < user interface end 
    
    raycastThread.join();
    EndAndPrintProfile();
    
    // RenderScene(&FBXScene);
    // todo material system
}


void AXExit()
{
    DestroyBVH();
    g_CurrentScene.Destroy();
    characterController.Destroy();
    uDestroy();
    SceneRenderer::Destroy();
}
