

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

inline Vector3f ColorMix(Vector3f col1, Vector3f col2, float p)
{
    p = 1.0f - p;
    float t = 1.0f - (p * p);
    Vector3f res = Vector3f::Lerp(col1 * col1, col2 * col2, t);
    return MakeVec3(Sqrt(res.x), Sqrt(res.y), Sqrt(res.z));
}

void CreateSkyTexture()
{
    uint pixels[64 * 4];
    uint* currentPixel = pixels;

    Vector3f startColor = MakeVec3(0.92f, 0.91f, 0.985f);
    Vector3f endColor = MakeVec3(247.0f, 173.0f, 50.0f) / MakeVec3(255.0f);

    for (int i = 0; i < 64; i++)
    {
        Vector3f target = ColorMix(startColor, endColor, (float)(i) / 64.0);
        uint color = PackColorRGBU32(&target.x);
        MemSet32(currentPixel, color, 4);
        currentPixel += 4;
    }
    skyTexture = CreateTexture(4, 64, pixels, TextureType_RGBA8, false, false);
}

// return 1 if success
int AXStart()
{
    if (!g_CurrentScene.ImportSubScene(&GLTFScene, "Meshes/SponzaGLTF/scene.gltf", 0.02f))
    {
        AX_ERROR("gltf scene load failed");
        return 0;
    }
   
    CreateSkyTexture();
    shader = ImportShader("Shaders/3DVert.glsl", "Shaders/PBRFrag.glsl");
    g_CurrentScene.Init();
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
    g_CurrentScene.Destroy();
}
