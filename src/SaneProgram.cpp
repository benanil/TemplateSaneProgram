
#include "include/Renderer.hpp"
#include "include/Animation.hpp"
#include "include/Platform.hpp"

#include "include/Scene.hpp"
#include "include/SceneRenderer.hpp"
#include "include/CharacterController.hpp"
#include "include/Camera.hpp"
#include "include/TextRenderer.hpp"

#include <stdio.h>

static PrefabID GLTFPrefab = 0;
static PrefabID AnimatedPrefab = 0;

CharacterController characterController={};

static void WindowResizeCallback(int width, int height)
{
    SceneRenderer::WindowResizeCallback(width, height);
    uWindowResizeCallback(width, height);
}

void AXInit()
{
    wSetWindowName("Engine");

    wSetWindowPosition(0, 0);
    wSetVSync(true);
    wSetWindowResizeCallback(WindowResizeCallback);
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
    uLoadFont("Fonts/Quivira.otf");

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

static void DrawUI();

// static double t;
// do rendering and main loop here
void AXLoop(bool shouldRender)
{
    Scene* currentScene = &g_CurrentScene;
    currentScene->Update();
    
    float deltaTime = (float)GetDeltaTime();
    deltaTime = MIN(deltaTime, 0.2f);

    // animate and control the movement of character
    const bool isSponza = true;
    characterController.Update(deltaTime, isSponza);
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

    DrawUI();
    // RenderScene(&FBXScene);
    // todo material system
}

enum MenuState_ {
    MenuState_Gameplay,
    MenuState_PauseMenu,
    MenuState_Options
};
typedef int MenuState;

static MenuState menuState = MenuState_Gameplay;
static char logText[32] = {'l','o', 'g'};
static bool wasHovered = false;
static bool isVsyncEnabled = true;

static inline void SetLogText(const char* txt, int size)
{
    MemsetZero(logText, sizeof(logText));
    SmallMemCpy(logText, txt, size);
}

inline void HoverEvents(bool* wasHovered, void(*HoverIn)(), void(*HoverOut)())
{
    if (!*wasHovered && uIsHovered()) 
    {
        if (HoverIn != nullptr) HoverIn();
    }

    if (*wasHovered && !uIsHovered()) 
    {
        if (HoverOut != nullptr) HoverOut();
    }
    
    *wasHovered = uIsHovered();
}

static void PauseMenu()
{
    Vector2f buttonSize = {340.0f, 70.0f};
    Vector2f buttonPosition;
    buttonPosition.x = (1920.0f / 2.0f) - (buttonSize.x / 2.0f);
    buttonPosition.y = 500.0f;

    if (uButton("Play", buttonPosition, buttonSize))
    {
        menuState = MenuState_Gameplay;
        SetLogText("Play", sizeof("Play"));
    }

    float buttonYPadding = 10.0f;
    buttonPosition.y += buttonSize.y + buttonYPadding;
    if (uButton("Options", buttonPosition, buttonSize)) 
    {
        menuState = MenuState_Options;
        SetLogText("Options", sizeof("Options"));
    }

    buttonPosition.y += buttonSize.y + buttonYPadding;
    if (uButton("Quit", buttonPosition, buttonSize)) 
    {
        wRequestQuit();
    }

    uText(logText, MakeVec2(1750.0f, 920.0f), 1.0f);
}

static void OptionsMenu()
{
    Vector2f bgPos;
    Vector2f byScale = { 1200.0f, 600.0f };
    bgPos.x = (1920.0f / 2.0f) - (byScale.x / 2.0f);
    bgPos.y = (1080.0f / 2.0f) - (byScale.y / 2.0f);

    uQuad(bgPos, byScale, uGetColor(uColorQuad));
    Vector2f textSize = uCalcTextSize("Settings", 1.2f);
    
    const float textPadding = 15.0f;
    bgPos.y += textSize.y + textPadding;
    bgPos.x += textPadding;
    uText("Settings", bgPos, 1.2f);
    
    bgPos.y += textSize.y * 2.0f + textPadding;
    uCheckBox("Vsync", &isVsyncEnabled, bgPos, 1.0f);
}

static void DrawUI()
{
    static int fps = 60;
    static char fpsTxt[16] = {'6', '0'};

    double timeSinceStart = TimeSinceStartup();
    if (timeSinceStart - (float)int(timeSinceStart) < 0.1)
    {
        double dt = GetDeltaTime();
        fps = (int)(1.0 / dt);
        IntToString(fpsTxt, fps);
    }

    uQuad(Vector2f::Zero(), MakeVec2(160.0f, 100.0f), uGetColor(uColorQuad));
    uText(fpsTxt, MakeVec2(15.0f, 85.0f), 1.0);
    
    // write to left bottom side of the screen
    uText("Cratoria: Dubrovnik-Sponza", MakeVec2(100.0f, 950.0f), 1.0f); // scale

    switch (menuState)
    {
        case MenuState_Options: OptionsMenu(); break;
        case MenuState_PauseMenu: PauseMenu(); break;
    };

    if (GetKeyPressed(Key_ESCAPE))
    {
        switch (menuState)
        {
            case MenuState_Options:   menuState = MenuState_PauseMenu; break;
            case MenuState_PauseMenu: menuState = MenuState_Gameplay;  break;
            case MenuState_Gameplay:  menuState = MenuState_PauseMenu; break;
        };
    }

    uRender();
}


void AXExit()
{
    g_CurrentScene.Destroy();
    characterController.Destroy();
    DestroyAnimationSystem();
    uDestroy();
    SceneRenderer::Destroy();
}
