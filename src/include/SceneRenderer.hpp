
#pragma once

struct Scene;
struct Prefab;
struct LightInstance;
struct AnimationController;
struct CameraBase;
struct Matrix4;

namespace SceneRenderer
{
    const int MaxNumLights = 16;

    void Init();

// Shadow
    void BeginShadowRendering(Scene* scene);

    // first draw statics then draw dynamics
    void RenderShadowOfPrefab(Scene* scene, unsigned short prefabID, AnimationController* animSystem);

    void EndShadowRendering();

// Rendering
    void BeginRendering();

    void RenderPrefab(Scene* scene, unsigned short prefabID, AnimationController* animSystem = nullptr);

    void EndRendering(bool renderToBackBuffer);

    void ShowGBuffer();

    void RenderOutlined(Scene* scene, unsigned short prefabID, int nodeIndex, int primitiveIndex, AnimationController* animSystem = nullptr);

    void PostProcessPass();

    // to make fake shadow
    void SetCharacterPos(float x, float y, float z);

    CameraBase* GetCamera();

    Matrix4* GetViewProjection();

    // we call this when we are using pause menu
    void DrawLastRenderedFrame();
    
// Lights
    void BeginUpdateLights();
    
    void UpdateLight(int index, LightInstance* instance);

    void EndUpdateLights();

    void Destroy();

    void WindowResizeCallback(int width, int height);

    bool ShouldReRender();

    void ShowEditor(float offset = 0.0f, bool* open = nullptr);
    
    void InitRayTracing(Prefab* scene);

    void RayTraceShadows(Prefab* scene);
}