
struct Scene;
struct Prefab;
struct LightInstance;

namespace SceneRenderer
{
    void Init();

    void BeginRendering();

    void RenderPrefab(Scene* scene, PrefabID prefabID, 
                        int animIndex = 0, float animTime = 0.0f); // animTime is between 0.0-1.0
    
    void PostProcessPass();

    void EndRendering();

    void BeginUpdateLights();
    
    void UpdateLight(int index, LightInstance* instance);

    void EndUpdateLights();

    void Destroy();
}