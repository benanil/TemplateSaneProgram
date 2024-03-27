
#pragma once

struct Scene;
struct Prefab;
struct LightInstance;
struct AnimationController;

namespace SceneRenderer
{
    const int MaxNumJoints = 128;
    const int MaxNumLights = 16;

    void Init();

// Shadow
    void BeginShadowRendering(Scene* scene);

    void RenderShadowOfPrefab(Scene* scene, unsigned short prefabID, AnimationController* animSystem);

    void EndShadowRendering();

// Rendering
    void BeginRendering();

    void RenderPrefab(Scene* scene, unsigned short prefabID, AnimationController* animSystem = nullptr);
    
    void EndRendering();

    void PostProcessPass();

// Lights
    void BeginUpdateLights();
    
    void UpdateLight(int index, LightInstance* instance);

    void EndUpdateLights();

    void Destroy();
}