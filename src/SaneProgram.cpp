
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
#include "include/Editor.hpp"
#include "include/BVH.hpp"
#include "include/TLAS.hpp"

#include "../ASTL/Additional/Profiler.hpp"
#include "../ASTL/Math/Color.hpp"
#include "../ASTL/HashSet.hpp"
#include "../ASTL/RedBlackTree.hpp"
#include "../ASTL/Queue.hpp"
#include "../ASTL/Array.hpp"
#include "../ASTL/String.hpp"
#include "../ASTL/IO.hpp"

PrefabID SpherePrefab = 0;
static PrefabID MainScenePrefab = 0;

static PrefabID AnimatedPrefab = 0;
static bool PauseMenuOpened = false;

// Editor.cpp
extern int SelectedNodeIndex;
extern int SelectedNodePrimitiveIndex;

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
    uPushFloat(uf::TextScale, 0.5f);
    uText(text, perfTxtPos, 0u);
    perfTxtPos.y += 23.0f;
    uPopFloat(uf::TextScale);
}

static void SetDoubleSidedMaterials(Prefab* mainScene);

extern void InitTerrain();
extern void UpdateTerrain(CameraBase* camera);
extern void RenderTerrain(CameraBase* camera);

// return 1 if success
int AXStart()
{
    g_CurrentScene.Init();
    InitBVH();

    if (!g_CurrentScene.ImportPrefab(&MainScenePrefab, "Assets/Meshes/Bistro/Bistro.gltf", 1.2f))
    // if (!g_CurrentScene.ImportPrefab(&MainScenePrefab, "Assets/Meshes/SponzaGLTF/scene.gltf", 1.2f))
    // // if (!g_CurrentScene.ImportPrefab(&MainScenePrefab, "Assets/Meshes/GroveStreet/GroveStreet.gltf", 1.14f))
    {
        AX_ERROR("gltf scene load failed");
        return 0;
    }

    if (!g_CurrentScene.ImportPrefab(&SpherePrefab, "Assets/Meshes/Sphere.gltf", 0.5f))
    {
        AX_ERROR("gltf scene load failed sphere");
        return 0;
    }

    if (!g_CurrentScene.ImportPrefab(&AnimatedPrefab, "Assets/Meshes/Paladin/Paladin.gltf", 1.0f))
    {
        AX_ERROR("gltf scene load failed2");
        return 0;
    }
    
    uInitialize();
    uSetFloat(uf::TextScale, 0.71f);
    // very good font that has lots of icons: http://www.quivira-font.com/
    uLoadFont("Assets/Fonts/JetBrainsMono-Regular.ttf"); //  Quivira.otf
    MemsetZero(&characterController, sizeof(CharacterController));
    Prefab* paladin = g_CurrentScene.GetPrefab(AnimatedPrefab); 
    characterController.Start(paladin);

    SceneRenderer::Init();
    InitTerrain();

    wSetWindowResizeCallback(WindowResizeCallback);
    wSetKeyPressCallback(KeyPressCallback);

    // SetDoubleSidedMaterials(mainScene); // < for bistro scene

    EditorInit();
    return 1;
}

// for bistro scene
void SetDoubleSidedMaterials(Prefab* mainScene)
{
    for (int i = 0; i < mainScene->numMaterials; i++) {
        mainScene->materials[i].doubleSided = false;
    }

    for (int i = 0; i < mainScene->numNodes; i++)
    {
        ANode* node = mainScene->nodes + i;
    
        if (!StringCompare("Bistro_Research_Exterior_Linde_Tree_Large_linde_tree_large_4051", node->name))
        {
            AMesh* mesh = mainScene->meshes + node->index;
            for (int j = 0; j < mesh->numPrimitives; j++)
            {
                APrimitive* primitive = mesh->primitives + j;
                AMaterial* material = mainScene->materials + primitive->material;
                material->doubleSided = true;
            }
            break;
        }
    }
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
    
    // draw when we are playing game, don't render when using pause menu to save power
    if (canRender && (PauseMenuOpened || GetMenuState() == MenuState_Gameplay || ShouldReRender()))
    {
        UpdateTerrain(camera);
    
        currentScene->Update();
    
        float deltaTime = (float)GetDeltaTime();
            
        // animate and control the movement of character
        const bool isSponza = false;
        characterController.Update(deltaTime, isSponza);
        AnimationController* animController = &characterController.mAnimController;
        
        if (true) 
        {
            BeginShadowRendering(currentScene);
                RenderShadowOfPrefab(currentScene, MainScenePrefab, nullptr);
                // don't render shadow of character, we will fake it.
                // RenderShadowOfPrefab(currentScene, AnimatedPrefab, animController);
            EndShadowRendering();
        }
    
        BeginRendering();
        {
            RenderPrefab(currentScene, MainScenePrefab, nullptr);
            RenderPrefab(currentScene, AnimatedPrefab, animController);
            // RenderPrefab(currentScene, SpherePrefab, nullptr);
        }
        
        RenderTerrain(camera);
    
        bool renderToBackBuffer = !PauseMenuOpened;
        SceneRenderer::EndRendering(renderToBackBuffer);
    
        // RenderOutlined(currentScene, MainScenePrefab, SelectedNodeIndex, SelectedNodePrimitiveIndex);
        
        ShowGBuffer(); // draw all of the graphics to back buffer (inside all window)
    
        EditorShow();
    
        PauseMenuOpened = false;
    }
    else
    {
        DrawLastRenderedFrame();
    }
    PauseMenuOpened = ShowMenu(); // < from Menu.cpp

    rDrawAllLines((float*)SceneRenderer::GetViewProjection());
    
    uRender(); // < user interface end 
    
    EndAndPrintProfile();

    // todo material system
}

extern void TerrainDestroy();

void AXExit()
{
    DestroyBVH();
    TerrainDestroy();
    uDestroy();
    EditorDestroy();
    
    g_CurrentScene.Destroy();
    characterController.Destroy();
    SceneRenderer::Destroy();
}
