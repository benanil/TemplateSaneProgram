
#pragma once

struct Scene;
struct Prefab;
struct LightInstance;
struct AnimationController;
struct Camera;

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
    
    void EndRendering();

    void PostProcessPass();

    // to make fake shadow
    void SetCharacterPos(float x, float y, float z);

    Camera* GetCamera();

// Lights
    void BeginUpdateLights();
    
    void UpdateLight(int index, LightInstance* instance);

    void EndUpdateLights();

    void Destroy();
}